#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstIterator.h"
#include <llvm/IR/Instructions.h>
#include <llvm/Analysis/LoopInfo.h>
#include <stack>
#include <iostream>
#include <fstream>
#include <string>
#include <map>

#define DEBUG_TYPE "RecursionTracker"


using namespace llvm;
using namespace std;

namespace {

  struct RecursionTracker : public llvm::ModulePass {
    static char ID;

    RecursionTracker() : ModulePass(ID) {}

    bool trackRecursion(Function &F, Module &M, GlobalVariable *gv){
      errs() << "Tracking recursion for " << F.getName() << "\n";
      bool first_bb = true;
      bool firts_i = true;

      for( BasicBlock &BB : F.getBasicBlockList()) {
        for ( Instruction &instruction : BB){
          if (first_bb && firts_i){
            errs() << "Instrumenting entry point of " << F.getName() << "\n";
            IRBuilder<> RecBuilder(&instruction);

            //Move global variable to local "space"
            LoadInst* localgv_ptr = RecBuilder.CreateLoad(gv);
            localgv_ptr->setAlignment(2);
            Value* localgv = RecBuilder.CreateSExt(localgv_ptr,IntegerType::getInt32Ty(M.getContext()));

            //Add 1 to local copy of gv then update gv
            Value* res = RecBuilder.CreateAdd(localgv,ConstantInt::get(IntegerType::getInt32Ty(M.getContext()),1,true));
            res = RecBuilder.CreateTrunc(res,IntegerType::getInt16Ty(M.getContext()));
            RecBuilder.CreateStore(res,gv);

            first_bb = false;
            firts_i = false;
          }
        }
      }

      for( BasicBlock &BB : F.getBasicBlockList()) {
        for ( Instruction &instruction : BB){
          if (ReturnInst *retInst = dyn_cast<ReturnInst>(&instruction)) {
            errs() << "Instrumenting exit point of " << F.getName() << "\n";
            IRBuilder<> RecBuilder(retInst);

            //Move global variable to local "space"
            LoadInst* localgv_ptr = RecBuilder.CreateLoad(gv);
            localgv_ptr->setAlignment(2);
            Value* localgv = RecBuilder.CreateSExt(localgv_ptr,IntegerType::getInt32Ty(M.getContext()));

            //Add 1 to local copy of gv then update gv
            Value* res = RecBuilder.CreateAdd(localgv,ConstantInt::get(IntegerType::getInt32Ty(M.getContext()),-1,true));
            res = RecBuilder.CreateTrunc(res,IntegerType::getInt16Ty(M.getContext()));
            RecBuilder.CreateStore(res,gv);
          }
        }
      }

      return true;
    }

    bool runOnModule(Module &M) {
      
      std::vector<std::string> fnInstrumented;
      bool res = false;

      // Inject global variable inside module
      GlobalVariable *gv = new GlobalVariable(M, IntegerType::getInt16Ty(M.getContext()),
      false, GlobalValue::CommonLinkage,
      0, "zbouncer_recursion_counter");
      gv->setAlignment(2);
      ConstantInt *init = ConstantInt::get(IntegerType::getInt16Ty(M.getContext()),0,true);
      gv->setInitializer(init);

      for( auto &F : M.getFunctionList()){
        if (!F.isDeclaration()){
          errs() << "Instrumenting Function: " << F.getName() << "\n";
          res = trackRecursion(F,M,gv); 
        }
      }

      return res;
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
char RecursionTracker::ID = 0;
// Register the pass - required for (among others) opt
static RegisterPass<RecursionTracker>
  X(/*PassArg=*/"RecursionTracker", /*Name=*/"RecursionTracker",/*CFGOnly=*/false, /*is_analysis=*/false);

