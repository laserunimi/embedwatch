#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstIterator.h"
#include <llvm/IR/Instructions.h>
#include <llvm/Analysis/LoopInfo.h>
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <stack>
#include <iostream>
#include <fstream>
#include <string>

#define DEBUG_TYPE "zbouncer"

//#define ZBOUNCER_OLD

using namespace llvm;
using namespace std;

namespace{

  struct Zbouncer : public llvm::ModulePass {
    static char ID;
    Zbouncer() : ModulePass(ID) {}

    
    void getAnalysisUsage(AnalysisUsage &AU) const{
      AU.addRequired<DominatorTreeWrapperPass>();
      AU.addPreserved<DominatorTreeWrapperPass>();
      AU.addRequired<LoopInfoWrapperPass>();
      AU.addPreserved<LoopInfoWrapperPass>();
      AU.addRequired<ScalarEvolutionWrapperPass>();
      AU.addPreserved<ScalarEvolutionWrapperPass>();
      AU.setPreservesAll();
    }

    /*
    void checkLoop(int countLoop, Loop *L, LLVMContext &CTX, Module &M){

      BasicBlock *parentLoopPred = L->getLoopPredecessor();
      BasicBlock *parentLoopHead = L->getHeader();

      std::vector<Loop*> subLoopAll = L->getSubLoops();
      Loop::iterator j, f;

      // Allocate stack for counter and store it
      Instruction *alloc_counter = new AllocaInst(IntegerType::get(CTX, 32), 0, "counter");
      parentLoopPred->getInstList().insert(parentLoopPred->getFirstInsertionPt(), alloc_counter);
      Instruction *store_counter = new StoreInst(ConstantInt::get(Type::getInt32Ty(CTX), 0, true), alloc_counter);
      store_counter->insertAfter(alloc_counter);

      //Counter++ instruction insertion
      Instruction *load_to_reg = new LoadInst(alloc_counter, "midCount");
      parentLoopHead->getInstList().insert(parentLoopHead->getFirstInsertionPt(), load_to_reg);
      Instruction *add_inc = BinaryOperator::CreateNSWAdd(load_to_reg, ConstantInt::get(Type::getInt32Ty(CTX), 1, true), "incremented");
      add_inc->insertAfter(load_to_reg);
      Instruction *store_counter_back = new StoreInst(add_inc, alloc_counter);
      store_counter_back->insertAfter(add_inc);

      //Insertion of call to zbouncer_exit_loop lib in all exit blocks
      SmallVector<BasicBlock*, 8> exit_blocks;
      L->getExitBlocks(exit_blocks);
      SmallSetVector<BasicBlock *, 8> ExitBlockSet(exit_blocks.begin(), exit_blocks.end());

      for(SmallSetVector<BasicBlock *, 8>::iterator ib = ExitBlockSet.begin(), eb = ExitBlockSet.end(); ib != eb; ++ib){
        BasicBlock *exit_block = *ib;
        Instruction *load_total = new LoadInst(alloc_counter, "total");
        exit_block->getInstList().insert(exit_block->getFirstInsertionPt(), load_total);

        // Create function prototype to be called
        FunctionCallee Fn = M.getOrInsertFunction("zbouncer_loop_exit", FunctionType::get(Type::getVoidTy(CTX), {Type::getInt32Ty(CTX)}, false));
        CallInst *Call = CallInst::Create(Fn,  ArrayRef<Value *>({load_total}), "", exit_block->getTerminator());
        
      }

      errs() << "loop:" << countLoop << " > subloop:" << subLoopAll.size() << " \n";
      int countSubLoop = 0;

      for(j = subLoopAll.begin(), f = subLoopAll.end(); j != f; ++j){
        countSubLoop++;
        Loop *p = *j;
        checkLoop(countSubLoop, p, CTX, M);
      }

    }
    */

    bool ends_with(string str, string suffix){
      if(str.length() < suffix.length())
          return false;
      
      return !str.compare(str.length() - suffix.length(), suffix.length(), suffix);
    }

    bool runOnModule(Module &M) {

      auto &CTX = M.getContext();
      std::vector<std::string> fnInstrumented;
      std::vector<std::string> blacklist = {
        "zbouncer_init",
        "zbouncer_destroy",
        "zbouncer_violation",
        "zbouncer_entry",
        "zbouncer_exit"};

      std::vector<std::string> inputFunctions = {
        "wrapper_gets", "wrapper_getc", "wrapper_fgets", "wrapper_fread", "wrapper_scanf"};

      #ifdef ZBOUNCER_OLD
        for (auto &F : M) {
          if (F.isDeclaration())
            continue;

          // Checking if function should be instrumented (whitelist)
          if (std::find(blacklist.begin(), blacklist.end(), F.getName()) == blacklist.end()){

            //---------------------------------------------------------------------
            // Start of call instrumentation
            //---------------------------------------------------------------------

            errs() << "Function: " << F.getName() << " is whitelisted\n";

            // Insertion point
            auto *InsertionPt = &*F.getEntryBlock().getFirstInsertionPt();
            // Create call to @llvm.returnaddress
            Instruction *RetAddr = CallInst::Create(Intrinsic::getDeclaration(&M, Intrinsic::returnaddress),
              ArrayRef<Value *>(ConstantInt::get(Type::getInt32Ty(CTX), 0)), "ra", InsertionPt);
            
            int args_num = 0;
            for(auto arg = F.arg_begin(); arg != F.arg_end(); ++arg)
              args_num++;

            std::vector<Value *> args = {RetAddr, ConstantInt::get(Type::getInt32Ty(CTX), args_num)};

            // Create function prototype to be called
            FunctionCallee Fn;
            if (F.getName() != "main"){
              errs() << "Function: " << F.getName() << " is regular -> injecting zbouncer_entry()\n";
              Fn = M.getOrInsertFunction("zbouncer_entry",
                FunctionType::get(Type::getVoidTy(CTX), {Type::getInt8PtrTy(CTX), Type::getInt32Ty(CTX)}, true));  
              CallInst *Call = CallInst::Create(Fn, args, "", InsertionPt);
            }

            // LOOP --------------------------------------------------------------------

            // LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
            
            // int countLoop = 0;

            // //Iteration on loops
            // for(LoopInfo::iterator i = LI.begin(), e = LI.end(); i != e; ++i){
            //   Loop *L = *i;
            //   int subloop = 1;
            //   countLoop++;

            //   BasicBlock *parentLoopPred = L->getLoopPredecessor();
            //   BasicBlock *parentLoopHead = L->getHeader();

            //   std::vector<Loop*> subLoopAll = L->getSubLoops();
            //   Loop::iterator j, f;

            //   checkLoop(countLoop, L, F.getContext(), M);
            // }

            // MEMCPY -------------------------------------------------------------------

            // for (auto &BB : F.getBasicBlockList()) {
            //   for (auto &instruction : BB) {
            //     if (CallInst *callInst = dyn_cast<CallInst>(&instruction)) {
            //       if (Function *calledFunction = callInst->getCalledFunction()) {
            //         if (calledFunction->getName().startswith("llvm.memcpy")) {
            //           errs() << "Function: " << F.getName() << ", found memcpy " << instruction.getOpcodeName() << "\n";

            //           CallSite CS(callInst);
            //           SmallVector<Value *, 8> Args(CS.arg_begin(), CS.arg_end());
            //           SmallVector<Value *, 3> Args_filtered({Args[0], Args[1], Args[2]});
            //           FunctionCallee Fn = M.getOrInsertFunction("zbouncer_wmemcpy",
            //             FunctionType::get(Type::getVoidTy(CTX),
            //             {Type::getInt8PtrTy(CTX), Type::getInt8PtrTy(CTX),
            //             Type::getInt32Ty(CTX)}, false));
            //           CallInst *Call = CallInst::Create(Fn, Args_filtered, "", callInst);

            //         }
            //       }
            //     }
            //   }
            // }


            //---------------------------------------------------------------------
            // End of call instrumentation
            // Start of ret instrumentation
            //---------------------------------------------------------------------
            for (auto &BB : F.getBasicBlockList()) {
              for (auto &I : BB.getInstList()) {
                if(isa<ReturnInst>(I)) {
                  
                  errs() << "Function: " << F.getName() << " found " << I.getOpcodeName() << " -> injecting zbouncer_exit()\n";
                  // Insertion point
                  auto *InsertionPt = BB.getTerminator();
                  // Create call to @llvm.frameadress
                  Instruction *FrameAddr = CallInst::Create(Intrinsic::getDeclaration(&M, Intrinsic::frameaddress),
                    ArrayRef<Value *>(ConstantInt::get(Type::getInt32Ty(CTX), 0)), "fp", InsertionPt);
                  // Create function prototype to be called
                  FunctionCallee Fn = M.getOrInsertFunction("zbouncer_exit", FunctionType::get(Type::getVoidTy(CTX), {Type::getInt8PtrTy(CTX)}, false));
                  // Create call to exit InsertionPt
                  CallInst *Call = CallInst::Create(Fn, ArrayRef<Value *>({FrameAddr}), "", InsertionPt);

                }
              }
            }
            

            //---------------------------------------------------------------------
            // End of ret instrumentation
            //----------------------------------------------------------------errs() << "entry collected 1\n";-----
            //fnInstrumented.push_back(F.getName());
            fnInstrumented.emplace_back(F.getName());
            errs() << "----------------------------------------------------------\n";

          }else{
            errs() << "Function " << F.getName() << " is blacklisted\n";
            errs() << "----------------------------------------------------------\n";
          }

        }

        errs() << "Total " << fnInstrumented.size() << " function instrumented: ";
        for(auto f : fnInstrumented)
          errs() << f << " ";
        errs() << "\n";
        errs() << "----------------------------------------------------------\n";
      
      #endif

      //---------------------------------------------------------------------
      // (remove ) input-wrapped-function
      //---------------------------------------------------------------------
      int count = 0;
      char format[] = "tmp%d";
      char twine[10];

      for (auto &F : M) {
        if (F.isDeclaration())
          continue;

        errs() << "Checking for function: " << F.getName() << "\n";
        for (auto &BB : F.getBasicBlockList()) {
          for (Instruction &instruction : BB) {
            if (CallInst *callInst = dyn_cast<CallInst>(&instruction)) {
              if (Function *calledFunction = callInst->getCalledFunction()) {

                if (std::find(inputFunctions.begin(), inputFunctions.end(), calledFunction->getName()) != inputFunctions.end()){
                  errs() << "\t" << calledFunction->getName() << " | " << instruction << "\n";

                  //callInst->eraseFromParent();

                  // CallSite CS(Call);
                  // SmallVector<Value *, 8> Args(CS.arg_begin(), CS.arg_end());
                  // CallInst *NewCI = CallInst::Create(NewFunc, Args);
                  // NewCI->setCallingConv(NewFunc->getCallingConv());
                  // if (!Call->use_empty())
                  //   Call->replaceAllUsesWith(NewCI);
                  // ReplaceInstWithInst(Call, NewCI);
                
                }

              }
            }
          }
        }
        
      }

      /*
        GET RECURSION TRACKER VARIABLE
      */

      GlobalVariable *rec_counter = M.getGlobalVariable("zbouncer_recursion_counter");

      errs() << "-------------------------LOOPS----------------------------\n";

      for(auto &F : M){
        if (F.isDeclaration())
          continue;
        
        LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();

        //Iteration on loops
        for(LoopInfo::iterator i = LI.begin(), e = LI.end(); i != e; ++i){
            Loop *L = *i;
            int cached = 0;
            for (auto it = L->block_begin(), eit = L->block_end() ; it != eit; it++) {
              const BasicBlock *BB = *it;
              for (auto it = BB->begin(), e = BB->end(); it != e; ++it) {
                 Instruction *i = const_cast<Instruction *>(&*it);

                //---------------------------------------------------------------------
                // Insert zbouncer_collect_entry (alloca)
                //---------------------------------------------------------------------
                if (MDNode *N = i->getMetadata("zdef")) {
                  std::string metadata = string(cast<MDString>(N->getOperand(0))->getString());
                  errs() << "Loop in " << F.getName() <<  ": ZDEF " << metadata << " | " << *i << "\n";

                  //Handle dynamic calls
                  if(CallInst *ci = dyn_cast<CallInst>(i)){
                    errs() << "Injecting zbouncer_collect_entry instead of alloca of call to " << string(ci->getCalledFunction()->getName()) << "\n";
                    Function *called = ci->getCalledFunction();
                    IRBuilder<> dynCallBuilder(i->getNextNode());
                    Value *size; 
                    Value *addr = ci;

                    if(called->getName() == "malloc"){
                      size = ci->getArgOperand(0);
                    } else if(called->getName() == "calloc"){
                      
                      sprintf(twine,format,count++);
                      size = dynCallBuilder.CreateMul(ci->getArgOperand(0),ci->getOperand(1),twine);

                    } else {
                      continue;
                    }

                    errs() << "Size: " <<*size << " Addr: " << *addr << "\n";

                    FunctionCallee Fn = M.getOrInsertFunction("zbouncer_collect_entry", FunctionType::get(Type::getVoidTy(CTX),
                      {Type::getInt32Ty(CTX), Type::getInt32Ty(CTX), addr->getType(), Type::getInt32Ty(CTX),
                      Type::getInt32Ty(CTX), Type::getInt32Ty(CTX)}, false));

                    size_t index = 0;
                    std::string def_string;
                    std::vector<Value *> args;
                    uint16_t def;

                    while((index = metadata.find("_")) != std::string::npos){
                      def_string = metadata.substr(0,index);
                      def = (uint16_t) std::stoi(def_string);

                      errs() << "zbouncer_collect_entry(def=" << def << ", addr=" << addr << ",size=" << size << ")\n";

                      // Move zbouncer_recursion_counter in local space
                      auto local_rec_counter_ptr = dynCallBuilder.CreateLoad(rec_counter);
                      local_rec_counter_ptr->setAlignment(2);
                      Value* local_rec_counter = dynCallBuilder.CreateSExt(local_rec_counter_ptr,IntegerType::getInt32Ty(M.getContext()));

                      // Load def value
                      auto localDef = dynCallBuilder.CreateAlloca(Type::getInt32Ty(CTX));
                      dynCallBuilder.CreateStore(ConstantInt::get(Type::getInt32Ty(CTX), def),localDef);
                      
                      //Shift def value 16 bit
                      auto loadedDef = dynCallBuilder.CreateLoad(localDef);
                      auto shl = dynCallBuilder.CreateShl(loadedDef,16);
                      dynCallBuilder.CreateStore(shl,localDef);

                      // And gv and 0x0000ffff so that the to half is zeroed
                      auto andLocGv = dynCallBuilder.CreateAnd(local_rec_counter, 0x0000ffff);

                      // Or the two values
                      loadedDef = dynCallBuilder.CreateLoad(localDef);
                      auto orValue = dynCallBuilder.CreateOr(loadedDef, andLocGv);
                      dynCallBuilder.CreateStore(orValue,localDef);
                      auto defParam = dynCallBuilder.CreateLoad(localDef);
                      
                      args = {ConstantInt::get(Type::getInt32Ty(CTX), 5), defParam, addr, ConstantInt::get(Type::getInt32Ty(CTX), 0),size, ConstantInt::get(Type::getInt32Ty(CTX), 0)};
                      CallInst *Call = dynCallBuilder.CreateCall(Fn,args);

                      metadata.erase(0,index + 1);
                    }
                    
                    def = (uint16_t) std::stoi(metadata);
                    errs() << "Injecting zbouncer_collect_entry(def=" << def << ", addr=" << addr << ",size=" << size << ")\n";

                    // Move zbouncer_recursion_counter in local space
                    auto local_rec_counter_ptr = dynCallBuilder.CreateLoad(rec_counter);
                    local_rec_counter_ptr->setAlignment(2);
                    Value* local_rec_counter = dynCallBuilder.CreateSExt(local_rec_counter_ptr,IntegerType::getInt32Ty(M.getContext()));

                    // Load def value
                    auto localDef = dynCallBuilder.CreateAlloca(Type::getInt32Ty(CTX));
                    dynCallBuilder.CreateStore(ConstantInt::get(Type::getInt32Ty(CTX), def),localDef);
                    
                    //Shift def value 16 bit
                    auto loadedDef = dynCallBuilder.CreateLoad(localDef);
                    auto shl = dynCallBuilder.CreateShl(loadedDef,16);
                    dynCallBuilder.CreateStore(shl,localDef);

                    // And gv and 0x0000ffff so that the to half is zeroed
                    auto andLocGv = dynCallBuilder.CreateAnd(local_rec_counter, 0x0000ffff);

                    // Or the two values
                    loadedDef = dynCallBuilder.CreateLoad(localDef);
                    auto orValue = dynCallBuilder.CreateOr(loadedDef, andLocGv);
                    dynCallBuilder.CreateStore(orValue,localDef);
                    auto defParam = dynCallBuilder.CreateLoad(localDef);
                    
                    args = {ConstantInt::get(Type::getInt32Ty(CTX), 5), defParam, addr, ConstantInt::get(Type::getInt32Ty(CTX), 0),size, ConstantInt::get(Type::getInt32Ty(CTX), 0)};

                    CallInst *Call = dynCallBuilder.CreateCall(Fn,args);

                    continue;
                  }


                  AllocaInst *a = (AllocaInst *)i;

                  FunctionCallee Fn = M.getOrInsertFunction("zbouncer_collect_entry", FunctionType::get(Type::getVoidTy(CTX),
                    {Type::getInt32Ty(CTX), Type::getInt32Ty(CTX), a->getType(), Type::getInt32Ty(CTX),
                    Type::getInt32Ty(CTX), Type::getInt32Ty(CTX)}, false));
                  
                  IRBuilder<> AllocaBuilder(i->getNextNode());

                  size_t index = 0;
                  std::string def_string;
                  std::vector<Value *> args;
                  uint16_t def;

                  while((index = metadata.find("_")) != std::string::npos){
                    def_string = metadata.substr(0,index);
                    def = (uint16_t) std::stoi(def_string);
                    
                    errs() << "Injecting zbouncer_collect_entry(def=" << def << ", addr=" << a << ")\n";

                    // Move zbouncer_recursion_counter in local space
                    auto local_rec_counter_ptr = AllocaBuilder.CreateLoad(rec_counter);
                    local_rec_counter_ptr->setAlignment(2);
                    Value* local_rec_counter = AllocaBuilder.CreateSExt(local_rec_counter_ptr,IntegerType::getInt32Ty(M.getContext()));

                    // Load def value
                    auto localDef = AllocaBuilder.CreateAlloca(Type::getInt32Ty(CTX));
                    AllocaBuilder.CreateStore(ConstantInt::get(Type::getInt32Ty(CTX), def),localDef);
                    
                    //Shift def value 16 bit
                    auto loadedDef = AllocaBuilder.CreateLoad(localDef);
                    auto shl = AllocaBuilder.CreateShl(loadedDef,16);
                    AllocaBuilder.CreateStore(shl,localDef);

                    // And gv and 0x0000ffff so that the to half is zeroed
                    auto andLocGv = AllocaBuilder.CreateAnd(local_rec_counter, 0x0000ffff);

                    // Or the two values
                    loadedDef = AllocaBuilder.CreateLoad(localDef);
                    auto orValue = AllocaBuilder.CreateOr(loadedDef, andLocGv);
                    AllocaBuilder.CreateStore(orValue,localDef);
                    auto defParam = AllocaBuilder.CreateLoad(localDef);

                    args = {ConstantInt::get(Type::getInt32Ty(CTX), 5), defParam, a, ConstantInt::get(Type::getInt32Ty(CTX), 0), ConstantInt::get(Type::getInt32Ty(CTX), 0), ConstantInt::get(Type::getInt32Ty(CTX), 0)};
                    CallInst *Call = AllocaBuilder.CreateCall(Fn,args);

                    metadata.erase(0,index + 1);
                  }
                  
                  def = (uint16_t) std::stoi(metadata);
                  errs() << "Injecting zbouncer_collect_entry(def=" << def << ", addr=" << a << ")\n";

                  // Move zbouncer_recursion_counter in local space
                  auto local_rec_counter_ptr = AllocaBuilder.CreateLoad(rec_counter);
                  local_rec_counter_ptr->setAlignment(2);
                  Value* local_rec_counter = AllocaBuilder.CreateSExt(local_rec_counter_ptr,IntegerType::getInt32Ty(M.getContext()));

                  // Load def value
                  auto localDef = AllocaBuilder.CreateAlloca(Type::getInt32Ty(CTX));
                  AllocaBuilder.CreateStore(ConstantInt::get(Type::getInt32Ty(CTX), def),localDef);
                  
                  //Shift def value 16 bit
                  auto loadedDef = AllocaBuilder.CreateLoad(localDef);
                  auto shl = AllocaBuilder.CreateShl(loadedDef,16);
                  AllocaBuilder.CreateStore(shl,localDef);

                  // And gv and 0x0000ffff so that the to half is zeroed
                  auto andLocGv = AllocaBuilder.CreateAnd(local_rec_counter, 0x0000ffff);

                  // Or the two values
                  loadedDef = AllocaBuilder.CreateLoad(localDef);
                  auto orValue = AllocaBuilder.CreateOr(loadedDef, andLocGv);
                  AllocaBuilder.CreateStore(orValue,localDef);
                  auto defParam = AllocaBuilder.CreateLoad(localDef);

                  args = {ConstantInt::get(Type::getInt32Ty(CTX), 5), defParam, a, ConstantInt::get(Type::getInt32Ty(CTX), 0), ConstantInt::get(Type::getInt32Ty(CTX), 0), ConstantInt::get(Type::getInt32Ty(CTX), 0)};
                  CallInst *Call = AllocaBuilder.CreateCall(Fn,args);
                  
                }

                if (MDNode *N = i->getMetadata("zsuse")) { 
                  std::string metadata = string(cast<MDString>(N->getOperand(0))->getString());
                  uint16_t use = (uint16_t) std::stoi(metadata);
                  StoreInst *st = (StoreInst *)i;

                  DataLayout dl =  DataLayout(&M);
                  Value *addr = st->getOperand(1);
                  ConstantInt *store_size = ConstantInt::get(llvm::Type::getInt32Ty(M.getContext()),dl.getTypeStoreSize(st->getOperand(0)->getType()));

                  IRBuilder<> Builder(st->getNextNode());

                  sprintf(twine,format,count++);
                  Value *StoreAddr = Builder.CreatePtrToInt(st->getOperand(1),Builder.getInt32Ty(),twine);

                  sprintf(twine,format,count++);
                  Value *Sum = Builder.CreateAdd(StoreAddr, store_size,twine);

                  sprintf(twine,format,count++);
                  Value *EndAddr = Builder.CreateIntToPtr(Sum,addr->getType(),twine);
                  

                  if (MDNode *InN = i->getMetadata("ziuse")){
                    string iuse = string(cast<MDString>(InN->getOperand(0))->getString());
                    errs() << "Loop in " << F.getName() <<  ": ZIUSE " << metadata << " | " << *i << "\n";
                    errs() << "Injecting zbouncer_iuse(use=" << use << ", def=" << iuse << ", addr=" << addr << ")\n";

                    Value *def_ref;

                    for(Function::arg_iterator a =  F.arg_begin(); a != F.arg_end(); a++){
                      if(!string(a->getName()).compare(iuse)){
                        def_ref = a;
                        break;
                      }
                    }

                    FunctionCallee Fn = M.getOrInsertFunction("zbouncer_collect_entry", FunctionType::get(Type::getVoidTy(CTX),
                      {Type::getInt32Ty(CTX), Type::getInt32Ty(CTX), EndAddr->getType(), Type::getInt32Ty(CTX),
                      Type::getInt32Ty(CTX), Type::getInt32Ty(CTX)}, false));

                    // Move zbouncer_recursion_counter in local space
                    auto local_rec_counter_ptr = Builder.CreateLoad(rec_counter);
                    local_rec_counter_ptr->setAlignment(2);
                    Value* local_rec_counter = Builder.CreateSExt(local_rec_counter_ptr,IntegerType::getInt32Ty(M.getContext()));

                    // Load use value
                    auto localUse = Builder.CreateAlloca(Type::getInt32Ty(CTX));
                    Builder.CreateStore(ConstantInt::get(Type::getInt32Ty(CTX), use),localUse);
                    
                    //Shift def value 16 bit
                    auto loadedUse = Builder.CreateLoad(localUse);
                    auto shl = Builder.CreateShl(loadedUse,16);
                    Builder.CreateStore(shl,localUse);

                    // And gv and 0x0000ffff so that the to half is zeroed
                    auto andLocGv = Builder.CreateAnd(local_rec_counter, 0x0000ffff);

                    // Or the two values
                    loadedUse = Builder.CreateLoad(localUse);
                    auto orValue = Builder.CreateOr(loadedUse, andLocGv);
                    Builder.CreateStore(orValue,localUse);
                    auto useParam = Builder.CreateLoad(localUse);

                    std::vector<Value *> args = {ConstantInt::get(Type::getInt32Ty(CTX), 3),
                      useParam, EndAddr, def_ref, ConstantInt::get(Type::getInt32Ty(CTX), 0),
                      ConstantInt::get(Type::getInt32Ty(CTX), 0)};

                    CallInst *Call = Builder.CreateCall(Fn,args); //( Fn, args, "", i->getNextNode());
                    
                  } else {
                    errs() << "Loop in " << F.getName() <<  ": ZSUSE " << metadata << " | " << *i << "\n";
                    errs() << "Injecting zbouncer_use(use=" << use << ", addr=" << addr << ")\n";
                    
                    FunctionCallee Fn = M.getOrInsertFunction("zbouncer_collect_entry", FunctionType::get(Type::getVoidTy(CTX),
                      {Type::getInt32Ty(CTX), Type::getInt32Ty(CTX), EndAddr->getType(), Type::getInt32Ty(CTX),
                      Type::getInt32Ty(CTX), Type::getInt32Ty(CTX)}, false));

                    // Move zbouncer_recursion_counter in local space
                    auto local_rec_counter_ptr = Builder.CreateLoad(rec_counter);
                    local_rec_counter_ptr->setAlignment(2);
                    Value* local_rec_counter = Builder.CreateSExt(local_rec_counter_ptr,IntegerType::getInt32Ty(M.getContext()));

                    // Load use value
                    auto localUse = Builder.CreateAlloca(Type::getInt32Ty(CTX));
                    Builder.CreateStore(ConstantInt::get(Type::getInt32Ty(CTX), use),localUse);

                    //Shift def value 16 bit
                    auto loadedUse = Builder.CreateLoad(localUse);
                    auto shl = Builder.CreateShl(loadedUse,16);
                    Builder.CreateStore(shl,localUse);

                    // And gv and 0x0000ffff so that the to half is zeroed
                    auto andLocGv = Builder.CreateAnd(local_rec_counter, 0x0000ffff);

                    // Or the two values
                    loadedUse = Builder.CreateLoad(localUse);
                    auto orValue = Builder.CreateOr(loadedUse, andLocGv);
                    Builder.CreateStore(orValue,localUse);
                    auto useParam = Builder.CreateLoad(localUse);

                    std::vector<Value *> args = {ConstantInt::get(Type::getInt32Ty(CTX), 3),
                      useParam, EndAddr, ConstantInt::get(Type::getInt32Ty(CTX), 0),
                      ConstantInt::get(Type::getInt32Ty(CTX), 0), ConstantInt::get(Type::getInt32Ty(CTX), 0)};

                    CallInst *Call = Builder.CreateCall(Fn, args);//, i->getNextNode());
                  }
                }

                //---------------------------------------------------------------------
                // Insert zbouncer_collect_entry (zluse)
                //---------------------------------------------------------------------
                if (MDNode *N = i->getMetadata("zluse")) {
                  errs() << "Found zluse in loops " << F.getName() << "\n";
                  std::string metadata = string(cast<MDString>(N->getOperand(0))->getString());
                  errs() << "Loop in " << M.getSourceFileName() << "->" << F.getName() <<  ": ZLUSE " << metadata << " | " << *i << "\n";
                }

                if (MDNode *N = i->getMetadata("zluse")) { 
                  std::string metadata = string(cast<MDString>(N->getOperand(0))->getString());
                  errs() << "Loop in " << F.getName() <<  ": ZLUSE " << metadata << " | " << *i << "\n";

                  // void *memcpy(void *dest, const void *src, size_t n);
                  // int scanf(const char *format, ...);
                  // char *strcpy(char *dest, const char *src);

                  uint16_t use = (uint16_t) std::stoi(metadata);
                  CallInst *call = (CallInst *)i;
                  Value *addr;
                  Value *size_of_write = ConstantInt::get(Type::getInt32Ty(CTX), 0xdeadbeef);

                  IRBuilder<> Builder(call->getNextNode());

                  if(call->getCalledFunction()->getName() == "memcpy"){
                    addr = call->getOperand(1);
                    size_of_write = call->getOperand(2);
                  }

                  if(call->getCalledFunction()->getName() == "scanf"){
                    addr = call->getOperand(1);
                  }

                  if(call->getCalledFunction()->getName() == "__isoc99_scanf"){
                    addr = call->getOperand(1);
                  }

                  if(call->getCalledFunction()->getName() == "strcpy"){
                    addr = call->getOperand(0);
                  }

                  if (MDNode *InN = i->getMetadata("ziuse")){
                    string iuse = string(cast<MDString>(InN->getOperand(0))->getString());
                    errs() << "Injecting zbouncer_iluse(use=" << use << ", def=" << iuse << ", addr=" << addr << ", size_of_write=" << size_of_write << ")\n";

                    Value *def_ref;

                    for(Function::arg_iterator a =  F.arg_begin(); a != F.arg_end(); a++){
                      if(!string(a->getName()).compare(iuse)){
                        def_ref = a;
                        break;
                      }
                    }

                    FunctionCallee Fn = M.getOrInsertFunction("zbouncer_collect_entry", FunctionType::get(Type::getVoidTy(CTX),
                      {Type::getInt32Ty(CTX), Type::getInt32Ty(CTX), addr->getType(), Type::getInt32Ty(CTX),
                      Type::getInt32Ty(CTX), Type::getInt32Ty(CTX)}, false));

                    // Move zbouncer_recursion_counter in local space
                    auto local_rec_counter_ptr = Builder.CreateLoad(rec_counter);
                    local_rec_counter_ptr->setAlignment(2);
                    Value* local_rec_counter = Builder.CreateSExt(local_rec_counter_ptr,IntegerType::getInt32Ty(M.getContext()));

                    // Load use value
                    auto localUse = Builder.CreateAlloca(Type::getInt32Ty(CTX));
                    Builder.CreateStore(ConstantInt::get(Type::getInt32Ty(CTX), use),localUse);
                    
                    //Shift def value 16 bit
                    auto loadedUse = Builder.CreateLoad(localUse);
                    auto shl = Builder.CreateShl(loadedUse,16);
                    Builder.CreateStore(shl,localUse);

                    // And gv and 0x0000ffff so that the to half is zeroed
                    auto andLocGv = Builder.CreateAnd(local_rec_counter, 0x0000ffff);

                    // Or the two values
                    loadedUse = Builder.CreateLoad(localUse);
                    auto orValue = Builder.CreateOr(loadedUse, andLocGv);
                    Builder.CreateStore(orValue,localUse);
                    auto useParam = Builder.CreateLoad(localUse);

                    std::vector<Value *> args = {ConstantInt::get(Type::getInt32Ty(CTX), 4),
                      useParam, addr, def_ref,
                      ConstantInt::get(Type::getInt32Ty(CTX), 0), size_of_write};

                    Builder.CreateCall(Fn, args);

                  } else {
                    errs() << "Injecting zbouncer_luse(use=" << use << ", addr=" << addr << ", size_of_write=" << size_of_write << ")\n";

                    FunctionCallee Fn = M.getOrInsertFunction("zbouncer_collect_entry", FunctionType::get(Type::getVoidTy(CTX),
                      {Type::getInt32Ty(CTX), Type::getInt32Ty(CTX), addr->getType(), Type::getInt32Ty(CTX),
                      Type::getInt32Ty(CTX), Type::getInt32Ty(CTX)}, false));

                    // Move zbouncer_recursion_counter in local space
                    auto local_rec_counter_ptr = Builder.CreateLoad(rec_counter);
                    local_rec_counter_ptr->setAlignment(2);
                    Value* local_rec_counter = Builder.CreateSExt(local_rec_counter_ptr,IntegerType::getInt32Ty(M.getContext()));

                    // Load use value
                    auto localUse = Builder.CreateAlloca(Type::getInt32Ty(CTX));
                    Builder.CreateStore(ConstantInt::get(Type::getInt32Ty(CTX), use),localUse);
                    
                    //Shift def value 16 bit
                    auto loadedUse = Builder.CreateLoad(localUse);
                    auto shl = Builder.CreateShl(loadedUse,16);
                    Builder.CreateStore(shl,localUse);

                    // And gv and 0x0000ffff so that the to half is zeroed
                    auto andLocGv = Builder.CreateAnd(local_rec_counter, 0x0000ffff);

                    // Or the two values
                    loadedUse = Builder.CreateLoad(localUse);
                    auto orValue = Builder.CreateOr(loadedUse, andLocGv);
                    Builder.CreateStore(orValue,localUse);
                    auto useParam = Builder.CreateLoad(localUse);

                    std::vector<Value *> args = {ConstantInt::get(Type::getInt32Ty(CTX), 2),
                      useParam, addr, ConstantInt::get(Type::getInt32Ty(CTX), 0),
                      ConstantInt::get(Type::getInt32Ty(CTX), 0), size_of_write};

                    Builder.CreateCall(Fn, args);
                  }
                }


              }
            }
        }
      }

      errs() << "-------------------------- ZINIT ----------------------------\n";

      for (auto &F : M) {
        if (F.isDeclaration())
          continue;

        if (std::find(blacklist.begin(), blacklist.end(), F.getName()) == blacklist.end()){

          // Insertion point
          auto *InsertionPt = &*F.getEntryBlock().getFirstInsertionPt();

          if (F.getName() == "main"){
            errs() << "Function: " << F.getName() << " is the entry point -> injecting zinit()\n";
            FunctionCallee Fn = M.getOrInsertFunction("zinit",
              FunctionType::get(Type::getVoidTy(CTX), {}, true));

            // Create call to init or entry after InsertionPt
            CallInst *Call = CallInst::Create(Fn, {}, "", InsertionPt);

            //Iterate over the global variables ad add a collect alloca for each one of them
            for (auto &Global : M.getGlobalList()){
                if(GlobalVariable *gv = dyn_cast<GlobalVariable>(&Global)){
                    if(MDNode *N = gv->getMetadata("zdef")){
                      std::string metadata = string(cast<MDString>(N->getOperand(0))->getString());
                      errs() << "METADATA::zdef " << metadata << " | " << *gv << "\n";

                      IRBuilder<> allocaBuilder(InsertionPt);
                      FunctionCallee Fn = M.getOrInsertFunction("zbouncer_collect_alloca", FunctionType::get(Type::getVoidTy(CTX),
                      {Type::getInt32Ty(CTX), gv->getType()}, false)); 

                      size_t index = 0;
                      std::string def_string;
                      std::vector<Value *> args;
                      uint16_t def;

                      while((index = metadata.find("_")) != std::string::npos){
                        def_string = metadata.substr(0,index);
                        def = (uint16_t) std::stoi(def_string);
                        
                        errs() << "Injecting zbouncer_collect_galloca(def=" << def << ", addr=" << gv << ")\n";

                        // Load def value
                        auto localDef = allocaBuilder.CreateAlloca(Type::getInt32Ty(CTX));
                        allocaBuilder.CreateStore(ConstantInt::get(Type::getInt32Ty(CTX), def),localDef);
                        
                        //Shift def value 16 bit
                        auto loadedDef = allocaBuilder.CreateLoad(localDef);
                        auto shl = allocaBuilder.CreateShl(loadedDef,16);
                        allocaBuilder.CreateStore(shl,localDef);

                        auto defParam = allocaBuilder.CreateLoad(localDef);

                        args = {defParam, gv};

                        CallInst *Call = allocaBuilder.CreateCall(Fn,args);

                        metadata.erase(0,index + 1);
                      }
                      
                      def = (uint16_t) std::stoi(metadata);
                      errs() << "Injecting zbouncer_collect_galloca(def=" << def << ", addr=" << gv << ")\n";

                      // Load def value
                      auto localDef = allocaBuilder.CreateAlloca(Type::getInt32Ty(CTX));
                      allocaBuilder.CreateStore(ConstantInt::get(Type::getInt32Ty(CTX), def),localDef);
                      
                      //Shift def value 16 bit
                      auto loadedDef = allocaBuilder.CreateLoad(localDef);
                      auto shl = allocaBuilder.CreateShl(loadedDef,16);
                      allocaBuilder.CreateStore(shl,localDef);

                      auto defParam = allocaBuilder.CreateLoad(localDef);

                      args = {defParam, gv};

                      CallInst *Call = allocaBuilder.CreateCall(Fn,args);
                    }
                }
            }

          }
        }
      }

      errs() << "-------------------------- ZEXIT ----------------------------\n";

      //---------------------------------------------------------------------
      // End of call instrumentation
      // Start of ret instrumentation
      //---------------------------------------------------------------------

      for (Function &F : M){
        if (F.getName() == "main"){
          for (BasicBlock &bb : F.getBasicBlockList()){
            for (Instruction &instr : bb){
              if(ReturnInst *ret = dyn_cast<ReturnInst>(&instr)){
                errs() << "Function: " << F.getName() << " found " << instr.getOpcodeName() << " -> injecting zexit()\n";
                // Create function prototype to be called
                FunctionCallee Fn = M.getOrInsertFunction("zexit", FunctionType::get(Type::getVoidTy(CTX), {}, false));
                // Create call to exit InsertionPt
                CallInst *Call = CallInst::Create(Fn, ArrayRef<Value *>({}), "", ret);
              }
            }
          }
        }
      }

      errs() << "----------------------------------------------------------\n";

      //---------------------------------------------------------------------
      // Insert alloca (call tz)
      //---------------------------------------------------------------------
      for (auto &F : M) {
        for (auto &BB : F.getBasicBlockList()) {
          for (BasicBlock::iterator it = BB.begin(), e = BB.end(); it != e; ++it) {
            Instruction* i = &*it;

            if (MDNode *N = i->getMetadata("zdef")) {

              std::string metadata = string(cast<MDString>(N->getOperand(0))->getString());
              errs() << "METADATA::zdef " << metadata << " | " << *i << "\n";

              //Handle dynamic calls
              if(CallInst *ci = dyn_cast<CallInst>(i)){
                errs() << "Injecting alloca of call to " << string(ci->getCalledFunction()->getName()) << "\n";
                Function *called = ci->getCalledFunction();
                IRBuilder<> dynCallBuilder(i->getNextNode());
                Value *size; 
                Value *addr = ci;

                if(called->getName() == "malloc"){
                  size = ci->getArgOperand(0);
                } else if(called->getName() == "calloc"){
                  
                  sprintf(twine,format,count++);
                  size = dynCallBuilder.CreateMul(ci->getArgOperand(0),ci->getOperand(1),twine);

                } else {
                  continue;
                }

                errs() << "Size: " <<*size << " Addr: " << *addr << "\n";

                FunctionCallee Fn = M.getOrInsertFunction("zbouncer_collect_alloca_dyn", FunctionType::get(Type::getVoidTy(CTX),
                  {Type::getInt32Ty(CTX), addr->getType(), size->getType()}, false));

                size_t index = 0;
                std::string def_string;
                std::vector<Value *> args;
                uint16_t def;

                while((index = metadata.find("_")) != std::string::npos){
                  def_string = metadata.substr(0,index);
                  def = (uint16_t) std::stoi(def_string);
                  
                  errs() << "Injecting zbouncer_collect_dalloca(def=" << def << ", addr=" << addr << ",size=" << size << ")\n";

                  // Move zbouncer_recursion_counter in local space
                  auto local_rec_counter_ptr = dynCallBuilder.CreateLoad(rec_counter);
                  local_rec_counter_ptr->setAlignment(2);
                  Value* local_rec_counter = dynCallBuilder.CreateSExt(local_rec_counter_ptr,IntegerType::getInt32Ty(M.getContext()));

                  // Load def value
                  auto localDef = dynCallBuilder.CreateAlloca(Type::getInt32Ty(CTX));
                  dynCallBuilder.CreateStore(ConstantInt::get(Type::getInt32Ty(CTX), def),localDef);
                  
                  //Shift def value 16 bit
                  auto loadedDef = dynCallBuilder.CreateLoad(localDef);
                  auto shl = dynCallBuilder.CreateShl(loadedDef,16);
                  dynCallBuilder.CreateStore(shl,localDef);

                  // And gv and 0x0000ffff so that the to half is zeroed
                  auto andLocGv = dynCallBuilder.CreateAnd(local_rec_counter, 0x0000ffff);

                  // Or the two values
                  loadedDef = dynCallBuilder.CreateLoad(localDef);
                  auto orValue = dynCallBuilder.CreateOr(loadedDef, andLocGv);
                  dynCallBuilder.CreateStore(orValue,localDef);
                  auto defParam = dynCallBuilder.CreateLoad(localDef);
                  
                  args = {defParam, addr,size};

                  CallInst *Call = dynCallBuilder.CreateCall(Fn,args);

                  metadata.erase(0,index + 1);
                }
                
                def = (uint16_t) std::stoi(metadata);
                errs() << "Injecting zbouncer_collect_dalloca(def=" << def << ", addr=" << addr << ",size=" << size << ")\n";

                // Move zbouncer_recursion_counter in local space
                auto local_rec_counter_ptr = dynCallBuilder.CreateLoad(rec_counter);
                local_rec_counter_ptr->setAlignment(2);
                Value* local_rec_counter = dynCallBuilder.CreateSExt(local_rec_counter_ptr,IntegerType::getInt32Ty(M.getContext()));

                // Load def value
                auto localDef = dynCallBuilder.CreateAlloca(Type::getInt32Ty(CTX));
                dynCallBuilder.CreateStore(ConstantInt::get(Type::getInt32Ty(CTX), def),localDef);
                
                //Shift def value 16 bit
                auto loadedDef = dynCallBuilder.CreateLoad(localDef);
                auto shl = dynCallBuilder.CreateShl(loadedDef,16);
                dynCallBuilder.CreateStore(shl,localDef);

                // And gv and 0x0000ffff so that the to half is zeroed
                auto andLocGv = dynCallBuilder.CreateAnd(local_rec_counter, 0x0000ffff);

                // Or the two values
                loadedDef = dynCallBuilder.CreateLoad(localDef);
                auto orValue = dynCallBuilder.CreateOr(loadedDef, andLocGv);
                dynCallBuilder.CreateStore(orValue,localDef);
                auto defParam = dynCallBuilder.CreateLoad(localDef);
                
                args = {defParam, addr,size};
                CallInst *Call = dynCallBuilder.CreateCall(Fn,args);

                continue;
              }

              AllocaInst *a = (AllocaInst *)i;
              FunctionCallee Fn = M.getOrInsertFunction("zbouncer_collect_alloca", FunctionType::get(Type::getVoidTy(CTX),
                {Type::getInt32Ty(CTX), a->getType()}, false));  
              
              IRBuilder<> AllocaBuilder(i->getNextNode());

              size_t index = 0;
              std::string def_string;
              std::vector<Value *> args;
              uint16_t def;

              while((index = metadata.find("_")) != std::string::npos){
                def_string = metadata.substr(0,index);
                def = (uint16_t) std::stoi(def_string);
                
                errs() << "Injecting zbouncer_collect_alloca(def=" << def << ", addr=" << a << ")\n";
                
                // Move zbouncer_recursion_counter in local space
                auto local_rec_counter_ptr = AllocaBuilder.CreateLoad(rec_counter);
                local_rec_counter_ptr->setAlignment(2);
                Value* local_rec_counter = AllocaBuilder.CreateSExt(local_rec_counter_ptr,IntegerType::getInt32Ty(M.getContext()));

                // Load def value
                auto localDef = AllocaBuilder.CreateAlloca(Type::getInt32Ty(CTX));
                AllocaBuilder.CreateStore(ConstantInt::get(Type::getInt32Ty(CTX), def),localDef);
                
                //Shift def value 16 bit
                auto loadedDef = AllocaBuilder.CreateLoad(localDef);
                auto shl = AllocaBuilder.CreateShl(loadedDef,16);
                AllocaBuilder.CreateStore(shl,localDef);

                // And gv and 0x0000ffff so that the to half is zeroed
                auto andLocGv = AllocaBuilder.CreateAnd(local_rec_counter, 0x0000ffff);

                // Or the two values
                loadedDef = AllocaBuilder.CreateLoad(localDef);
                auto orValue = AllocaBuilder.CreateOr(loadedDef, andLocGv);
                AllocaBuilder.CreateStore(orValue,localDef);
                auto defParam = AllocaBuilder.CreateLoad(localDef);

                args = {defParam, a};
                CallInst *Call = AllocaBuilder.CreateCall(Fn,args);

                metadata.erase(0,index + 1);
              }
              
              def = (uint16_t) std::stoi(metadata);
              errs() << "Injecting zbouncer_collect_alloca(def=" << def << ", addr=" << a << ")\n";
              
              // Move zbouncer_recursion_counter in local space
              auto local_rec_counter_ptr = AllocaBuilder.CreateLoad(rec_counter);
              local_rec_counter_ptr->setAlignment(2);
              Value* local_rec_counter = AllocaBuilder.CreateSExt(local_rec_counter_ptr,IntegerType::getInt32Ty(M.getContext()));

              // Load def value
              auto localDef = AllocaBuilder.CreateAlloca(Type::getInt32Ty(CTX));
              localDef->setAlignment(4);
              AllocaBuilder.CreateStore(ConstantInt::get(Type::getInt32Ty(CTX), def),localDef);
              
              //Shift def value 16 bit
              auto loadedDef = AllocaBuilder.CreateLoad(localDef);
              auto shl = AllocaBuilder.CreateShl(loadedDef,16);
              AllocaBuilder.CreateStore(shl,localDef);

              // And gv and 0x0000ffff so that the to half is zeroed
              auto andLocGv = AllocaBuilder.CreateAnd(local_rec_counter, 0x0000ffff);

              // Or the two values
              loadedDef = AllocaBuilder.CreateLoad(localDef);
              auto orValue = AllocaBuilder.CreateOr(loadedDef, andLocGv);
              AllocaBuilder.CreateStore(orValue,localDef);
              auto defParam = AllocaBuilder.CreateLoad(localDef);

              args = {defParam, a};
              CallInst *Call = AllocaBuilder.CreateCall(Fn,args);
              
            }
          }
        }
      }

      errs() << "----------------------------------------------------------\n";

      //---------------------------------------------------------------------
      // Insert use (call tz)
      //---------------------------------------------------------------------
      
      for (auto &F : M) {
        for (auto &BB : F.getBasicBlockList()) {
          for (BasicBlock::iterator it = BB.begin(), e = BB.end(); it != e; ++it) {
            Instruction* i = &*it;

            if (MDNode *N = i->getMetadata("zsuse")) { 
              errs() << "METADATA::zuse " << cast<MDString>(N->getOperand(0))->getString() << " | " << *i << "\n";
              std::string metadata = string(cast<MDString>(N->getOperand(0))->getString());

              uint16_t use = (uint16_t) std::stoi(metadata);
              StoreInst *st = (StoreInst *)i;

              DataLayout dl =  DataLayout(&M);
              Value *addr = st->getOperand(1);
              ConstantInt *store_size = ConstantInt::get(llvm::Type::getInt32Ty(M.getContext()),dl.getTypeStoreSize(st->getOperand(0)->getType()));

              IRBuilder<> Builder(st->getNextNode());

              sprintf(twine,format,count++);
              Value *StoreAddr = Builder.CreatePtrToInt(st->getOperand(1),Builder.getInt32Ty(),twine);

              sprintf(twine,format,count++);
              Value *Sum = Builder.CreateAdd(StoreAddr, store_size,twine);

              sprintf(twine,format,count++);
              Value *EndAddr = Builder.CreateIntToPtr(Sum,addr->getType(),twine);
              

              if (MDNode *InN = i->getMetadata("ziuse")){
                string iuse = string(cast<MDString>(InN->getOperand(0))->getString());
                errs() << "Injecting zbouncer_iuse(use=" << use << ", def=" << iuse << ", addr=" << addr << ")\n";

                Value *def_ref;

                for(Function::arg_iterator a =  F.arg_begin(); a != F.arg_end(); a++){
                  if(!string(a->getName()).compare(iuse)){
                    def_ref = a;
                    break;
                  }
                }

                FunctionCallee Fn = M.getOrInsertFunction("zbouncer_iuse", FunctionType::get(Type::getVoidTy(CTX),
                  {Type::getInt32Ty(CTX), Type::getInt32Ty(CTX), addr->getType()}, false));  

                // Move zbouncer_recursion_counter in local space
                auto local_rec_counter_ptr = Builder.CreateLoad(rec_counter);
                local_rec_counter_ptr->setAlignment(2);
                Value* local_rec_counter = Builder.CreateSExt(local_rec_counter_ptr,IntegerType::getInt32Ty(M.getContext()));

                // Load use value
                auto localUse = Builder.CreateAlloca(Type::getInt32Ty(CTX));
                Builder.CreateStore(ConstantInt::get(Type::getInt32Ty(CTX), use),localUse);
                
                //Shift def value 16 bit
                auto loadedUse = Builder.CreateLoad(localUse);
                auto shl = Builder.CreateShl(loadedUse,16);
                Builder.CreateStore(shl,localUse);

                // And gv and 0x0000ffff so that the to half is zeroed
                auto andLocGv = Builder.CreateAnd(local_rec_counter, 0x0000ffff);

                // Or the two values
                loadedUse = Builder.CreateLoad(localUse);
                auto orValue = Builder.CreateOr(loadedUse, andLocGv);
                Builder.CreateStore(orValue,localUse);
                auto useParam = Builder.CreateLoad(localUse);
                
                std::vector<Value *> args = {useParam, def_ref, EndAddr};

                CallInst *Call = Builder.CreateCall(Fn,args); //( Fn, args, "", i->getNextNode());
                
              } else {
                errs() << "Injecting zbouncer_use(use=" << use << ", addr=" << addr << ")\n";
                FunctionCallee Fn = M.getOrInsertFunction("zbouncer_use", FunctionType::get(Type::getVoidTy(CTX),
                  {Type::getInt32Ty(CTX), addr->getType()}, false));  

                // Move zbouncer_recursion_counter in local space
                auto local_rec_counter_ptr = Builder.CreateLoad(rec_counter);
                local_rec_counter_ptr->setAlignment(2);
                Value* local_rec_counter = Builder.CreateSExt(local_rec_counter_ptr,IntegerType::getInt32Ty(M.getContext()));

                // Load use value
                auto localUse = Builder.CreateAlloca(Type::getInt32Ty(CTX));
                Builder.CreateStore(ConstantInt::get(Type::getInt32Ty(CTX), use),localUse);
                
                //Shift def value 16 bit
                auto loadedUse = Builder.CreateLoad(localUse);
                auto shl = Builder.CreateShl(loadedUse,16);
                Builder.CreateStore(shl,localUse);

                // And gv and 0x0000ffff so that the to half is zeroed
                auto andLocGv = Builder.CreateAnd(local_rec_counter, 0x0000ffff);

                // Or the two value
                loadedUse = Builder.CreateLoad(localUse);
                auto orValue = Builder.CreateOr(loadedUse, andLocGv);
                Builder.CreateStore(orValue,localUse);
                auto useParam = Builder.CreateLoad(localUse);
                
                std::vector<Value *> args = {useParam, EndAddr};

                CallInst *Call = Builder.CreateCall(Fn, args);//, i->getNextNode());
              }
            }


          }
        }
      }

      errs() << "----------------------------------------------------------\n";

      //---------------------------------------------------------------------
      // Insert lib use (call tz)
      //---------------------------------------------------------------------
      // In this code we insert an instrumentation that extract the size of the write in bytes
      // the tz should recieve (idx, addr_target, size_of_write). addr_target+size_of_write is the
      // maximum address reach for that write and should be sent outside to the verifier
      for (auto &F : M) {
        for (auto &BB : F.getBasicBlockList()) {
          for (BasicBlock::iterator it = BB.begin(), e = BB.end(); it != e; ++it) {
            Instruction* i = &*it;

            if (MDNode *N = i->getMetadata("zluse")) { 
              errs() << "METADATA::zluse " << cast<MDString>(N->getOperand(0))->getString() << " | " << *i << "\n";
              std::string metadata = string(cast<MDString>(N->getOperand(0))->getString());

              // void *memcpy(void *dest, const void *src, size_t n);
              // int scanf(const char *format, ...);
              // char *strcpy(char *dest, const char *src);

              uint16_t use = (uint16_t) std::stoi(metadata);
              CallInst *call = (CallInst *)i;
              Value *addr;
              Value *usetest = ConstantInt::get(Type::getInt32Ty(CTX), use);
              Value *size_of_write = ConstantInt::get(Type::getInt32Ty(CTX), 0xdeadbeef);

              IRBuilder<> Builder(call->getNextNode());

              if(call->getCalledFunction()->getName() == "memcpy"){
                addr = call->getOperand(1);
                size_of_write = call->getOperand(2);
              }

              if(call->getCalledFunction()->getName() == "scanf"){
                addr = call->getOperand(1);
              }

              if(call->getCalledFunction()->getName() == "__isoc99_scanf"){
                addr = call->getOperand(1);
              }

              if(call->getCalledFunction()->getName() == "strcpy"){
                addr = call->getOperand(0);
              }

              if (MDNode *InN = i->getMetadata("ziuse")){
                string iuse = string(cast<MDString>(InN->getOperand(0))->getString());
                errs() << "Injecting zbouncer_iluse(use=" << use << ", def=" << iuse << ", addr=" << addr << ", size_of_write=" << size_of_write << ")\n";

                Value *def_ref;

                for(Function::arg_iterator a =  F.arg_begin(); a != F.arg_end(); a++){
                  if(!string(a->getName()).compare(iuse)){
                    def_ref = a;
                    break;
                  }
                }

                FunctionCallee Fn = M.getOrInsertFunction("zbouncer_iluse", FunctionType::get(Type::getVoidTy(CTX),
                  {Type::getInt32Ty(CTX), Type::getInt32Ty(CTX), addr->getType(), size_of_write->getType()}, false));
                
                // Move zbouncer_recursion_counter in local space
                auto local_rec_counter_ptr = Builder.CreateLoad(rec_counter);
                local_rec_counter_ptr->setAlignment(2);
                Value* local_rec_counter = Builder.CreateSExt(local_rec_counter_ptr,IntegerType::getInt32Ty(M.getContext()));

                // Load use value
                auto localUse = Builder.CreateAlloca(Type::getInt32Ty(CTX));
                Builder.CreateStore(ConstantInt::get(Type::getInt32Ty(CTX), use),localUse);
                
                //Shift def value 16 bit
                auto loadedUse = Builder.CreateLoad(localUse);
                auto shl = Builder.CreateShl(loadedUse,16);
                Builder.CreateStore(shl,localUse);

                // And gv and 0x0000ffff so that the to half is zeroed
                auto andLocGv = Builder.CreateAnd(local_rec_counter, 0x0000ffff);

                // Or the two value
                loadedUse = Builder.CreateLoad(localUse);
                auto orValue = Builder.CreateOr(loadedUse, andLocGv);
                Builder.CreateStore(orValue,localUse);
                auto useParam = Builder.CreateLoad(localUse);

                std::vector<Value *> args ={useParam, def_ref, addr, size_of_write};

                Builder.CreateCall(Fn, args);

              } else {
                //void zbouncer_luse(uint16_t use, char* addr, uint32_t size_of_write){
                errs() << "Injecting zbouncer_luse(use=" << use << ", addr=" << addr << ", size_of_write=" << size_of_write << ")\n";
                FunctionCallee Fn = M.getOrInsertFunction("zbouncer_luse", FunctionType::get(Type::getVoidTy(CTX),
                  {Type::getInt32Ty(CTX), addr->getType(), size_of_write->getType()}, false));  

                // Move zbouncer_recursion_counter in local space
                auto local_rec_counter_ptr = Builder.CreateLoad(rec_counter);
                local_rec_counter_ptr->setAlignment(2);
                Value* local_rec_counter = Builder.CreateSExt(local_rec_counter_ptr,IntegerType::getInt32Ty(M.getContext()));

                // Load use value
                auto localUse = Builder.CreateAlloca(Type::getInt32Ty(CTX));
                Builder.CreateStore(ConstantInt::get(Type::getInt32Ty(CTX), use),localUse);
                
                //Shift def value 16 bit
                auto loadedUse = Builder.CreateLoad(localUse);
                auto shl = Builder.CreateShl(loadedUse,16);
                Builder.CreateStore(shl,localUse);

                // And gv and 0x0000ffff so that the to half is zeroed
                auto andLocGv = Builder.CreateAnd(local_rec_counter, 0x0000ffff);

                // Or the two value
                loadedUse = Builder.CreateLoad(localUse);
                auto orValue = Builder.CreateOr(loadedUse, andLocGv);
                Builder.CreateStore(orValue,localUse);
                auto useParam = Builder.CreateLoad(localUse);

                std::vector<Value *> args = {useParam, addr, size_of_write};

                Builder.CreateCall(Fn, args);
              }
            }


          }
        }
      }

      // Instument function calls to propagate defs
      map<CallInst*,CallInst*> funcalls;

      for (auto &F : M.getFunctionList()) {
        if(F.isDeclaration())
          continue;
        
        for(BasicBlock &bb : F.getBasicBlockList()){
          for (BasicBlock::iterator it = bb.begin(), e = bb.end(); it != e; ++it) {
            Instruction *instr = &*it;
    
            if (CallInst *callInst = dyn_cast<CallInst>(instr)) {

              bool replace = false;

              Function *caller = callInst->getCalledFunction();

              if(caller == NULL || !ends_with(string(caller->getName()),"_instrumented")){
                continue;
              }

              CallSite CS(callInst);
              IRBuilder<> Builder(callInst);

              SmallVector<Value *, 8> Args(CS.arg_begin(), CS.arg_end());
              
              int par_count = 0;
              for(Argument &arg : caller->args()){
                string paramName = string(arg.getName());
                
                if (MDNode *N = instr->getMetadata("zref_" + paramName)) {
                  string zref_name = string(cast<MDString>(N->getOperand(0))->getString());
                  replace = true;

                  errs() << "Instruction: ";
                  instr->print(errs());
                  errs() << "\nHas metadata zref_" << paramName << " with value " <<  zref_name  << "\n";

                  if(std::all_of(zref_name.begin(), zref_name.end(), ::isdigit)){
                    // Move zbouncer_recursion_counter in local space
                    auto local_rec_counter_ptr = Builder.CreateLoad(rec_counter);
                    local_rec_counter_ptr->setAlignment(2);
                    Value* local_rec_counter = Builder.CreateSExt(local_rec_counter_ptr,IntegerType::getInt32Ty(M.getContext()));

                    // Load id value
                    auto localId = Builder.CreateAlloca(Type::getInt32Ty(CTX));
                    Builder.CreateStore(ConstantInt::get(IntegerType::getInt32Ty(M.getContext()),(uint16_t) std::stoi(zref_name),true),localId);
                    
                    //Shift def value 16 bit
                    auto loadedId = Builder.CreateLoad(localId);
                    auto shl = Builder.CreateShl(loadedId,16);
                    Builder.CreateStore(shl,localId);

                    // And gv and 0x0000ffff so that the to half is zeroed
                    auto andLocGv = Builder.CreateAnd(local_rec_counter, 0x0000ffff);

                    // Or the two values
                    loadedId = Builder.CreateLoad(localId);
                    auto orValue = Builder.CreateOr(loadedId, andLocGv);
                    Builder.CreateStore(orValue,localId);
                    auto idParam = Builder.CreateLoad(localId);

                    Args[par_count] = idParam;
                  } else {
                    for(Function::arg_iterator a =  F.arg_begin(); a != F.arg_end(); a++){
                      if(!string(a->getName()).compare(zref_name)){
                       Args[par_count] = a;
                      }
                    }
                  }

                }

                par_count++;


              }

              if(replace){
                errs() << "Arguments recap:\n";
                for(int i = 0; i < Args.size(); i++){
                  Args[i]->print(errs());
                  errs() <<"\n";
                }

                replace = false;
              
                IRBuilder<> Builder(instr);
                CallInst *Call = Builder.CreateCall(callInst->getCalledFunction(),Args); //CallInst::Create(caller,Args,"",callInst->getNextNode());
                Call->setCallingConv(callInst->getCallingConv());
                funcalls.insert({callInst,Call});                
              }
            }
          }
        }
      }

      //Necessary since removing an instruction from a Basic Block we are iterating over
      //lead to a segfault.
      //See https://stackoverflow.com/questions/65475761/segfault-on-removing-instruction-in-llvm-custom-optimization-pass

      for(auto it : funcalls){
      
        errs() << "Replacing ";
        it.first->print(errs());
        errs() << " with ";
        it.second->print(errs());
        errs() << "\n";

        if (!it.first->use_empty()){
          it.first->replaceAllUsesWith(it.second);
        }

        errs() << "Replaced call\n";
        
        it.first->eraseFromParent();
      }
      
      errs() << "Returning\n";

      // for (auto &F : M) {
      //   for (auto &BB : F.getBasicBlockList()) {
      //     for (BasicBlock::iterator it = BB.begin(), e = BB.end(); it != e; ++it) {
      //       Instruction* i = &*it;
      //       errs() << *i << "\n";
      //     }
      //   }
      // }

      return true;
    }


  };

  bool runOnModule(llvm::Module &M) {
    bool Changed = runOnModule(M);
          
    return Changed;
  }

}


//-----------------------------------------------------------------------------
// Legacy PM Registration
//-----------------------------------------------------------------------------
char Zbouncer::ID = 0;
// Register the pass - required for (among others) opt
static RegisterPass<Zbouncer>
  X(/*PassArg=*/"zbouncer", /*Name=*/"Zbouncer",/*CFGOnly=*/false, /*is_analysis=*/false);

