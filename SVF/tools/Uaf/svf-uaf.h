#include "SVF-FE/LLVMUtil.h"
#include "Graphs/SVFG.h"
#include "WPA/Andersen.h"
#include "WPA/FlowSensitive.h"
#include "SVF-FE/SVFIRBuilder.h"
#include "Util/Options.h"
#include <iostream>
#include <fstream>
#include "DDA/FlowDDA.h"
#include "DDA/ContextDDA.h"
#include "DDA/DDAVFSolver.h"
#include "DDA/DDAPass.h"
#include "DDA/DDAStat.h"
#include "DDA/DDAClient.h"
#include <assert.h>
#include "visits.h"



using namespace llvm;
using namespace std;
using namespace SVF;

#define ARCH_PTR_SIZE 32

static llvm::cl::opt<std::string> InputFilename(cl::Positional, llvm::cl::desc("<input bitcode>"), llvm::cl::init("-"));

const std::vector<std::string> allocFunctions = {
    "malloc",
    "calloc",
    "realloc"
};
const std::vector<std::string> freeFunctions = {
    "free"
};
const std::vector<std::string> allocAndFreeFn = {
    "malloc",
    "calloc",
    "realloc",
    "free"
};
const std::vector<std::string> mainFunctions = {
    "main"
};
