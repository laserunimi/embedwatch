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

using namespace std;
using namespace llvm;
using namespace SVF;

std::vector<PAGNode *>* getDefNodes(PAGNode* pgn);
std::vector<PAGNode *>* visitAndSelectPagNodes(PAGNode* start, void (*)(PAGNode *, std::vector<PAGNode *>*,std::vector<PAGNode *>*, std::set<PAGNode *>* ));
std::vector<PAGNode *>* findDefNodesFromStore(StoreInst *si,PAG *pag);

PAGNode *findTargetOfCall(PAGNode *callNode, PAG *pag);
PAGNode *findFirstStore(PAGNode *top);
PAGNode *findFirstDominator(PAGNode *top);

bool isDominatorNode(PAGNode *);

void selectDefs(PAGNode *current, std::vector<PAGNode *>* worklist, std::vector<PAGNode *>* visited, std::set<PAGNode *>* ret);

std::vector<SVFGNode *>* visitAndSelectSVFGNodes(SVFGNode* start, void (*)(SVFGNode *, std::vector<SVFGNode *>*,std::vector<SVFGNode *>*,std::set<SVFGNode *>* ));
void selectCallBlockNodes(SVFGNode *, std::vector<SVFGNode *>* , std::vector<SVFGNode *>* , std::set<SVFGNode *>*);

std::vector<std::vector<SVFGNode *>*>* getPathFromTo(SVFGNode *from, Value *to);

std::set<PAGNode *> findAllStore(PAGNode *top);
