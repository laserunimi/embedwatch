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

#define DEBUG_TYPE "FunctionInstrumentor"


using namespace llvm;
using namespace std;

namespace{

  struct FunctionInstrumentor : public llvm::ModulePass {
    static char ID;

    FunctionInstrumentor() : ModulePass(ID) {}

    std::vector<std::string> blacklist = {
      "main",
      "wrapper_scanf",
      "wrapper_gets"
    };

    std::map<std::string,int> instrumentedFunctions;
    
    void dumpKeys(map<string,int> M, vector<string> *V){
        for(map<string,int>::iterator it = M.begin(); it != M.end(); it++){
          V->push_back(it->first);
          errs() << "Function " << it->first << " was extended with " << it->second << " arguments\n";
        }
    }

    bool instrumentCalls(Module &M){
      bool ret = false;
      vector<string> instrumentedFunctionsNames;
      map<CallInst*,CallInst*> funcalls;

      dumpKeys(instrumentedFunctions,&instrumentedFunctionsNames);

      errs() << "Searching for calls to:\n";
      for(string s : instrumentedFunctionsNames){
        errs() << "\t- " << s << "\n";
      }

      for (Function &F : M){
        if(F.isDeclaration() || count(instrumentedFunctionsNames.begin(),instrumentedFunctionsNames.end(),F.getName())){
          // If the function is only declared, or it is an instrumented function avoid the analysis
          continue;
        }

        for( BasicBlock &BB : F.getBasicBlockList()) {
          for ( Instruction &instruction : BB){
            if (CallInst *callInst = dyn_cast<CallInst>(&instruction)) {
                if (Function *calledFunction = callInst->getCalledFunction()) {
                  if(count(instrumentedFunctionsNames.begin(),instrumentedFunctionsNames.end(),calledFunction->getName())){
                    errs() << "Found call to " << calledFunction->getName() << " inside " << F.getName() << "\n";

                    CallSite CS(callInst);
                    SmallVector<Value *, 8> Args(CS.arg_begin(), CS.arg_end());

                    Function *instrumentedFunc = M.getFunction(string(calledFunction->getName()) + "_instrumented");

                    if (!instrumentedFunc){
                      errs() << "No such function: " << calledFunction->getName() << "\n";
                      continue;
                    }

                    Args.append(instrumentedFunctions.at(string(calledFunction->getName())) ,ConstantInt::get(IntegerType::getInt32Ty(M.getContext()),0,true));
              
                    CallInst *Call = CallInst::Create(instrumentedFunc,Args);
                    funcalls.insert({callInst,Call});
                    ret = true;
                  }
                }
            }
          }
        }

      }

      //Necessary since removing an instruction from a Basic Block we are iterating over
      //lead to a segfault.
      //See https://stackoverflow.com/questions/65475761/segfault-on-removing-instruction-in-llvm-custom-optimization-pass
      errs() << "Replacing calls\n";
      for(auto it : funcalls){
        ReplaceInstWithInst(it.first,it.second);
      }

      return ret;
    }
    
    bool instrumentFunction(Function &F, Module &M){
      int instrumentable = 0;

      StringRef name = F.getName();

      std::vector<std::string> pointerNames;
      std::vector<Type*> paramTypes;

      errs() << "Function " << name << " has parameters:\n" ;

      for(Argument &arg : F.args()){
        paramTypes.push_back(arg.getType());
        errs() << "\t-";
        arg.print(errs());
        errs() << "\n"; 
        
        if( arg.getType()->isPointerTy()){
          instrumentable++;
          pointerNames.push_back(string(arg.getName()) + "_id");
        }

      }

      if(!instrumentable || count(blacklist.begin(),blacklist.end(),name)){
        errs() << "Function " << name << " doesn't need instrumentation since either no pointer arguments are present or it is blacklisted\n";
        return false;
      }

      for (int i = 0; i < instrumentable; i++)
        paramTypes.push_back(Type::getInt32Ty(M.getContext()));

      FunctionType *FT = FunctionType::get(F.getReturnType(),paramTypes,false); //TODO: Verificare che vada bene false
      Function *instrumented = Function::Create(FT,F.getLinkage(),name + "_instrumented",M);

      ValueToValueMapTy vm;
      Function::arg_iterator destArg = instrumented -> arg_begin();
      for (const Argument & A : F.args()){
        destArg->setName(A.getName());
        vm[&A] = &*destArg++;
      }

      for(auto name = pointerNames.begin();destArg < instrumented -> arg_end(); destArg++,name++){
        destArg->setName(*name);
      }

      SmallVector<ReturnInst*, 8> returns;
      CloneFunctionInto(instrumented,&F,vm,F.getSubprogram() != nullptr,returns,"_instrumented");
            
      instrumentedFunctions.insert({string(F.getName()),instrumentable});
      return true;
    }

    bool runOnModule(Module &M) {
      
      std::vector<std::string> fnInstrumented;
      bool res = false;

      for( auto &F : M.getFunctionList()){
        if (!F.isDeclaration()){
          errs() << "Instrumenting Function: " << F.getName() << "\n";
          bool res = instrumentFunction(F,M);

          if(res){
            blacklist.push_back(string(F.getName()) + "_instrumented");
            res = true;
          } else {
            blacklist.push_back(string(F.getName()));
          }
        }
      }

      res |= instrumentCalls(M);

      for(map<string,int>::iterator it = instrumentedFunctions.begin(); it != instrumentedFunctions.end(); it++){
        errs() << "Removing function " << it->first << "\n";
        Function *f = M.getFunction(it->first);
        f->eraseFromParent();
        errs() << "Remove OK " << it->first << "\n";
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
char FunctionInstrumentor::ID = 0;
// Register the pass - required for (among others) opt
static RegisterPass<FunctionInstrumentor>
  X(/*PassArg=*/"FunctionInstrumentor", /*Name=*/"FunctionInstrumentor",/*CFGOnly=*/false, /*is_analysis=*/false);

