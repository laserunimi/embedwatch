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
#include <stack>
#include <iostream>
#include <fstream>
#include <string>

#define DEBUG_TYPE "InputWrapper"

using namespace llvm;
using namespace std;

namespace{

  struct InputWrapper : public llvm::ModulePass {
    static char ID;
    InputWrapper() : ModulePass(ID) {}

    bool runOnModule(Module &M) {

      auto &CTX = M.getContext();
      std::vector<std::string> fnInstrumented;
      std::vector<std::string> blacklist = {
        "zbouncer_init",
        "zbouncer_destroy",
        "zbouncer_violation",
        "zbouncer_entry",
        "zbouncer_exit",
        "wrapper_scanf",
        "wrapper_gets"};

      for (auto &F : M) {
        if (F.isDeclaration())
          continue;

        // Checking if function should be instrumented (whitelist)
        if (std::find(blacklist.begin(), blacklist.end(), F.getName()) == blacklist.end()){

          //---------------------------------------------------------------------
          // Start of call instrumentation
          //---------------------------------------------------------------------

          errs() << "Function: " << F.getName() << " is whitelisted\n";

          for (auto &BB : F.getBasicBlockList()) {
            for (auto &instruction : BB) {
              if (CallInst *callInst = dyn_cast<CallInst>(&instruction)) {
                if (Function *calledFunction = callInst->getCalledFunction()) {

                  errs() << "FN:" << calledFunction->getName() << "\n";

                  if (calledFunction->getName().find("scanf") != std::string::npos){
                    errs() << "Function: " << F.getName() << ", found scanf " << instruction.getOpcodeName() << "\n";

                    CallSite CS(callInst);
                    SmallVector<Value *, 8> Args(CS.arg_begin(), CS.arg_end());
                    errs() << "Args size:" << Args.size() << "\n";
                    std::vector<Type*> ParamTypes;
                    for (int i = 0, e = Args.size(); i != e; ++i)
                      ParamTypes.push_back(Args[i]->getType());

                    FunctionCallee Fn = M.getOrInsertFunction("wrapper_scanf", FunctionType::get(Type::getInt8PtrTy(CTX),
                      {Type::getInt8PtrTy(CTX), Type::getInt8PtrTy(CTX)}, false));
                    CallInst *Call = CallInst::Create(Fn, {Args[0], Args[1]}, "", callInst);
                  }

                  if (calledFunction->getName().find("gets") != std::string::npos){
                    if(calledFunction->getName().find("fgets") != std::string::npos){ // is already "secure" because of size
                      errs() << "FFFgets " << F.getName() << " " << instruction.getOpcodeName() << " skip...\n";

                    }else{
                      errs() << "Function: " << F.getName() << ", found gets " << instruction.getOpcodeName() << "\n";
                      errs() << calledFunction->getName().find("gets") << "\n";

                      CallSite CS(callInst);
                      SmallVector<Value *, 8> Args(CS.arg_begin(), CS.arg_end());
                      errs() << "Args size:" << Args.size() << "\n";
                      std::vector<Type*> ParamTypes;
                      for (int i = 0, e = Args.size(); i != e; ++i)
                        ParamTypes.push_back(Args[i]->getType());

                      FunctionCallee Fn = M.getOrInsertFunction("wrapper_gets", FunctionType::get(Type::getInt8PtrTy(CTX),
                        {Type::getInt8PtrTy(CTX)}, false));
                      CallInst *Call = CallInst::Create(Fn, {Args[0]}, "", callInst);
                    }
                    
                  }

                }
              }
            }
          }

          fnInstrumented.push_back(F.getName());
          errs() << "----------------------------------------------------------\n";

        }else{
          errs() << "Function " << F.getName() << " is blacklisted\n";
          errs() << "----------------------------------------------------------\n";
        }

      }

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
char InputWrapper::ID = 0;
// Register the pass - required for (among others) opt
static RegisterPass<InputWrapper>
  X(/*PassArg=*/"InputWrapper", /*Name=*/"InputWrapper",/*CFGOnly=*/false, /*is_analysis=*/false);

