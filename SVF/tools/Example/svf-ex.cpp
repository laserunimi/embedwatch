#include "SVF-FE/LLVMUtil.h"
#include "Graphs/SVFG.h"
#include "Graphs/VFG.h"
#include "WPA/Andersen.h"
#include "WPA/FlowSensitive.h"
#include "SVF-FE/SVFIRBuilder.h"
#include "Util/Options.h"
#include <iostream>
#include <fstream>
#include <string>  
#include <sstream> 
#include "DDA/FlowDDA.h"
#include "DDA/ContextDDA.h"
#include "DDA/DDAVFSolver.h"
#include "DDA/DDAPass.h"
#include "DDA/DDAStat.h"
#include "DDA/DDAClient.h"
#include "MemoryModel/SVFStatements.h"
#include "svf-ex.h"
#include "visits.h"
#include "trace.h"
#include <assert.h>

using namespace llvm;
using namespace std;
using namespace SVF;

#define DEBUG
#define ARCH_PTR_SIZE 32

void printType(Type *t){

    outs() << "Type: ";
    t->print(outs());
    outs() << "\n";

    outs() << "\tVoidType: " << t->isVoidTy() << "\n";
    outs() << "\tFunctionType: " << t->isFunctionTy() << "\n";
    outs() << "\tFirstClassType: " << t->isFirstClassType()<< "\n";
    outs() << "\t\tPointerType: " << t->isPointerTy() << "\n";
    outs() << "\t\tIntegerType: " << t->isIntegerTy() << "\n";
    outs() << "\t\tFloating-pointType: " << t->isFloatingPointTy() << "\n";
    outs() << "\tAggregateType: " << t->isAggregateType() << "\n";
    outs() << "\t\tArrayType: " << t->isArrayTy() << "\n";
    outs() << "\t\tStructType: " << t->isStructTy() << "\n";
}

bool ends_with(string str, string suffix){
    if(str.length() < suffix.length())
        return false;
    
    return !str.compare(str.length() - suffix.length(), suffix.length(), suffix);
}

std::vector<VFGNode *> *VFGNodeFilter(std::vector<VFGNode *> *s, VFGNode::VFGNodeK kind){

    std::vector<VFGNode *> *result = new vector<VFGNode *>();

    for(auto it = s->begin(), eit = s->end(); it!=eit; ++it){
        VFGNode* node = *it;
        if(node->getNodeKind() == kind){
            #ifdef DEBUG
            outs() << "VFG-ID=" << node->getId() << " (store) | " << node->getICFGNode()->toString() << "\n";
            #endif
            result->emplace_back(node);
            
        }
    }

    return result;
}

/*!
 * An example to query/collect all the uses of a definition of a value along value-flow graph (VFG)
 */
std::vector<VFGNode*> *traverseOnVFG(const SVFG* vfg, Value* val){
    SVFIR* pag = PAG::getPAG();
    
    PAGNode* pNode = pag->getGNode(pag->getValueNode(val));
    const VFGNode* vNode = vfg->getDefSVFGNode(pNode);
    FIFOWorkList<const VFGNode*> worklist;
    Set<const VFGNode*> visited;
    worklist.push(vNode);

    /// Traverse along VFG
    while (!worklist.empty())
    {
        const VFGNode* vNode = worklist.pop();
        for (VFGNode::const_iterator it = vNode->OutEdgeBegin(), eit =
                    vNode->OutEdgeEnd(); it != eit; ++it)
        {
            VFGEdge* edge = *it;
            VFGNode* succNode = edge->getDstNode();
            if (visited.find(succNode) == visited.end())
            {
                visited.insert(succNode);
                worklist.push(succNode);
            }
        }
    }

    // convert in std::set with non-const elements
    std::vector<VFGNode *> *result = new vector<VFGNode *>();
    for(Set<const VFGNode*>::const_iterator it = visited.begin(), eit = visited.end(); it!=eit; ++it){
        const VFGNode* node = *it;
        result->emplace_back(const_cast<VFGNode *>(node));
    }

    return result;

}

void printTrace(std::vector<TraceEntity *> *trace){
    for(auto it = trace->begin(), eit = trace->end(); it != eit; it++){
        printEntity(*it);
    }
}

int get_def_id(std::vector<Def *> deflist, PAGNode *node){
    //Cerco tra tutte le definizioni così da inserire l'id appropriato, ritorna -1 in caso non venga trovata la definizione
    for(Def *d : deflist){
        if(defNodeCompare(d,node))
            return d->getId();
    }

    return -1;
}

Argument *check_parameter_aliasing(const Value *param,Function *func, PointerAnalysis *pta) {
    for(llvm::Function::arg_iterator a = func->arg_begin(); a != func->arg_end(); a++ ){
        //Argument arg = *a;
        outs() << "Checking aliasing of " << param->getName() << " with argument " << (*a).getName() << " of " << func->getName() << "\n";
        if(pta->alias(param, &(*a)) == SVF::AliasResult::NoAlias)
            continue;
        
        //outs() << "Parameter " << param->getName() << " is aliased with argument " << (*a).getName() << "\n";
        return &(*a);
    }

    return NULL;
}

string exportEntity(TraceEntity *te,int father,int ident){
    stringstream buffer;
    for(int i = 0; i < ident; i++){
         buffer << "\t";
    }

    if(DDef *def = trace_entity_dynamic_cast<DDef>(te)){
        buffer  << def->traceLine(father);
        buffer << exportEntity(def->getTarget(), def->getId(), ident + 1);
        return buffer.str();   
    }

    buffer << te->traceLine(father);
    

    return buffer.str();
}   


Value* getTagValue(TraceEntity *te){
    if(ADef *v = trace_entity_dynamic_cast<ADef>(te)){
        return v->getDefAlloca();
    }

    if(GDef *v = trace_entity_dynamic_cast<GDef>(te)){
        return v->getDefValue();
    }

    if(DDef *v = trace_entity_dynamic_cast<DDef>(te)){
        return v->getCall();
    }
    
    if(SDef *v = trace_entity_dynamic_cast<SDef>(te)){
        return const_cast<Value*>(v->getNode()->getValue());
    }

    if(LUse *v = trace_entity_dynamic_cast<LUse>(te)){
        return v->getCall();
    }

    if(SUse *v = trace_entity_dynamic_cast<SUse>(te)){
        return v->getStoreUse();
    }

    return nullptr;
}

void updateValueMap(TraceEntity *te, std::map<TraceEntity *, std::tuple<string *, string *, string *>>  *valueMap){
    //Value *v = getTagValue(te);
    char text[100];

    if(!(isUse(te) || isDef(te)))
        return;

    if(isDef(te)){
        sprintf(text, "%d", te->getId());
        if(valueMap->count(te) == 0){
                valueMap->insert({te, tuple<string *, string *, string *>(new string(text), new string(""), new string(""))});
        } else {
            auto elem = valueMap->find(te);
            std::tuple<string *, string *, string *> s = elem->second;
            string *def; string *store; string *lib;
            std::tie(def, store, lib) = s;

            sprintf(text, "%d", te->getId());
            if(def->find(text) == string::npos){                    
                sprintf(text, "_%d", te->getId());
                def->append(text);
            }
        }
    }

    if(trace_entity_dynamic_cast<SUse>(te)){
        sprintf(text, "%d", te->getId());
        if(valueMap->count(te) == 0){
                valueMap->insert({te, tuple<string *, string *, string *>(new string(""), new string(text), new string(""))});
        } else {
            auto elem = valueMap->find(te);
            std::tuple<string *, string *, string *> s = elem->second;
            string *def; string *store; string *lib;
            std::tie(def, store, lib) = s;

            sprintf(text, "%d", te->getId());
            if(store->find(text) == string::npos){                    
                sprintf(text, "_%d", te->getId());
                store->append(text);
            }
        }
    }

    if(trace_entity_dynamic_cast<LUse>(te)){
        sprintf(text, "%d", te->getId());
        if(valueMap->count(te) == 0){
                valueMap->insert({te, tuple<string *, string *, string *>(new string(""), new string(""), new string(text))});
        } else {
            auto elem = valueMap->find(te);
            std::tuple<string *, string *, string *> s = elem->second;
            string *def; string *store; string *lib;
            std::tie(def, store, lib) = s;

            sprintf(text, "%d", te->getId());
            if(lib->find(text) == string::npos){                    
                sprintf(text, "_%d", te->getId());
                lib->append(text);
            }
        }
    }
}


void exportTrace(std::vector<TraceEntity *> &trace, LLVMContext &ctx, Module *M, string traceName){

    //char text[100];
    std::error_code OutErrorInfo;
    llvm::raw_fd_ostream exportFile(llvm::StringRef(traceName), OutErrorInfo, llvm::sys::fs::OF_None);
    std::map<TraceEntity *, std::tuple<string *, string *, string *>> valueMap; // <instr, alloca, store, lib>

    
    for(TraceEntity *te : trace){
         exportFile << exportEntity(te,-1,0);
         outs() <<  exportEntity(te,-1,0);
         updateValueMap(te,&valueMap);
         
         
         if(In *in = trace_entity_dynamic_cast<In>(te)){
            for(auto it = in->subObjectsBegin(), eit = in->subObjectsEnd(); it != eit; it++ ){
                exportFile << exportEntity(*it,in->getId(),1);
                outs() << exportEntity(*it,in->getId(),1);
                updateValueMap(*it,&valueMap);
                
                if(ZUse *use = asUse(*it)){
                    for(auto uit = use->targetObjBegin(), ueit = use->targetObjEnd(); uit != ueit; uit++ ){
                        exportFile << exportEntity(*uit,use->getId(),2);
                        outs() << exportEntity(*uit,use->getId(),2);
                        updateValueMap(*uit,&valueMap);
                    }
                } 
            }
        }
    }

    exportFile.flush();
    exportFile.close();
    
    for(auto it = valueMap.cbegin(); it != valueMap.cend(); ++it){
        std::tuple<string *, string *, string *> s = it->second;
        std::string *def; std::string *store; string* lib;
        std::tie(def, store, lib) = s; 
        TraceEntity *te = it->first;
        Value *v = getTagValue(te);

        outs() << *v << " | defStr: " << *def << " | storeStr: " << *store << " | libStr: " << *lib << "\n";



        if(!def->empty()){
            if(Instruction *instr = dyn_cast<Instruction>(v)){
                instr->setMetadata("zdef", MDNode::get(ctx, MDString::get(ctx, *def)));
            } else if(GlobalVariable *gv = dyn_cast<GlobalVariable>(v)){
                gv->setMetadata("zdef", MDNode::get(ctx, MDString::get(ctx, *def)));
            }
        }

        if(!store->empty()){
            if(Instruction *instr = dyn_cast<Instruction>(v)){
                instr->setMetadata("zsuse", MDNode::get(ctx, MDString::get(ctx, *store)));
            }
        }

        if(!lib->empty()){
            if(Instruction *instr = dyn_cast<Instruction>(v)){
                instr->setMetadata("zluse", MDNode::get(ctx, MDString::get(ctx, *lib)));
            }
        }
    } 
}

Def *getOrBuildDef(std::vector<Def*> *deflist, DefBuilder *db, PAGNode *pn){
    int id;
    if((id = get_def_id(*deflist,pn)) == -1)
        return db->buildDef(pn);
    
    return getDefWithID(deflist,id);
}

std::vector<const SVFGNode *> getSVFGNodesFromValue(SVFIR *pag, const llvm::Value * value, SVFG *svfg) {
    std::vector<const SVFGNode*> ret;
    // search for all PAGEdges first
    for (const PAGEdge* pagEdge : pag->getValueEdges(value))
    {
        VFG::PAGEdgeToStmtVFGNodeMapTy::const_iterator it = svfg->PAGEdgeToStmtVFGNodeMap.find(pagEdge);
        if (it != svfg->PAGEdgeToStmtVFGNodeMap.end())
        {
            ret.push_back(it->second);
        }
    }
    // add all PAGNodes
    PAGNode* pagNode = pag->getGNode(pag->getValueNode(value));
    if(svfg->hasDef(pagNode))
    {
        ret.push_back(svfg->getDefSVFGNode(pagNode));
    }
    return ret;
}

vector<int> *analyzeGep(PAGNode *p, SVFIR *pag, SVFG *svfg, Value *use, std::vector<Def *> *deflist, ZUse *traceUse ) {
    vector<int> *ret = new vector<int>();
    Type *t = p->getValue()->getType();
    p->getValue()->print(outs());
    outs() << " is a struct type, gep analysis needed\n";

    std::vector< const SVFGNode *> svfg_defs = getSVFGNodesFromValue(pag,p->getValue(),svfg);

    //for(const SVFGNode * svfg_def : svfg_defs) {
        const SVFGNode * svfg_def = svfg_defs[0];
        vector<vector<SVFGNode *>*>* pathsToStore = getPathFromTo(const_cast<SVFGNode *>(svfg_def),use);

        for(auto it = pathsToStore->begin(), eit = pathsToStore->end(); it != eit; it++) {
            int size = 0;
            vector<int>* field_list = new vector<int>();
            
            t = p->getValue()->getType();

            outs() << "Found path\n";
            vector<SVFGNode *>* pathToStore = *it;
            for (auto it = pathToStore->begin(), eit = pathToStore->end(); it != eit; it++) {
                SVFGNode *e = *it;

                if(e->getValue() == NULL)
                    continue;

                #ifdef DEBUG
                outs() << "Node: ";
                e->getValue()->print(outs());
                outs() << "\n";
                #endif
            }

            for (auto it = pathToStore->begin(), eit = pathToStore->end(); it != eit; it++) {
                SVFGNode *e = *it;

                if(e->getValue() == NULL)
                    continue;

                #ifdef DEBUG
                outs() << "On Node: ";
                e->getValue()->print(outs());
                outs() << "\n";
                #endif

                if(GetElementPtrInst *gep = const_cast<GetElementPtrInst *>(dyn_cast<GetElementPtrInst>(e->getValue()))){
                    outs() << "Found GEP:\n";
                    gep->print(outs());
                    outs() << "\n";
                    int op_count = 2;


                    while ((t->isStructTy() || ( t->isPointerTy() && t->getPointerElementType()->isStructTy())) && (unsigned int) op_count < gep->getNumOperands()){
                        if(!dyn_cast<ConstantInt>(gep->getOperand(op_count))){
                            outs() << "Operand is not a constant int, breaking out of the loop\n";
                            break;
                        }

                        int operand = dyn_cast<ConstantInt>(gep->getOperand(op_count))->getSExtValue();

                        outs() << "Considering operand " << op_count << "\n";
                        Type *tp;

                        if(t->isStructTy()){
                            tp = t->getStructElementType(operand);
                        } else {
                            tp = t->getPointerElementType()->getStructElementType(operand);
                        }


                        outs() << "Accessing " << operand << "th operand of struct with type ";
                        tp->print(outs());
                        outs() << "\n";

                        field_list->push_back(operand);

                        for (int i = 0; i < operand; i++) {
                            DataLayout dl =  DataLayout(LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule());
                            Type *temp_t;

                            outs() << "Type ";
                            if(t->isStructTy()){
                                temp_t = t->getStructElementType(i);
                            } else {
                                temp_t = t->getPointerElementType()->getStructElementType(i);
                            }

                            temp_t->print(outs());
                            outs() << " has size " << dl.getTypeAllocSize(temp_t) << "\n";
                            size += dl.getTypeAllocSize(temp_t); 
                        }

                        outs() << "Current offset from base: " << size << "\n";
                        op_count++;
                        t = tp;
                    }

                    if(!(t->isStructTy() || ( t->isPointerTy() && t->getPointerElementType()->isStructTy()))) {
                        break;
                    }
                    
                }

            }

            bool found = false;

            for(Def *d : *deflist){
                if (SDef *def = trace_entity_dynamic_cast<SDef>(d)) {
                    if (def->getNode() == p && def->getType() == t && def->getOffset() == size) {
                        ret->emplace_back(def->getId());
                        if (traceUse != NULL)
                            traceUse->addTargetObject(def);
                        found = true;
                    }
                }
            }

            if (!found) {
                if (t == p->getValue()->getType()){
                    if(t->isStructTy()){
                        t = t->getStructElementType(0);
                    } else {
                        t = t->getPointerElementType()->getStructElementType(0);
                    }

                    field_list->emplace_back(0);
                }
                outs() << "Adding SUse with field of type: ";
                t->print(outs());
                outs() << "\n";
                SDef *def = new SDef(p,t,new vector<int>(*field_list),size);
                outs() << "Adding def to deflist\n";
                deflist->emplace_back(def);
                ret->emplace_back(def->getId());
                if (traceUse != NULL)
                    traceUse->addTargetObject(def);
            }

        }
    return ret;
}

int main(int argc, char **argv) {
    int arg_num = 0;
    char **arg_value = new char*[argc];
    std::vector<std::string> moduleNameVec;
    LLVMUtil::processArguments(argc, argv, arg_num, arg_value, moduleNameVec);
    //cl::ParseCommandLineOptions(arg_num, arg_value,
    //"Whole Program Points-to Analysis\n");
    
    if (Options::WriteAnder == "ir_annotator")
    {
        LLVMModuleSet::getLLVMModuleSet()->preProcessBCs(moduleNameVec);
    }

    SVF::BVDataPTAImpl *pta = NULL;

    SVFModule* svfModule = LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(moduleNameVec);
    svfModule->buildSymbolTableInfo();

	/// Build Program Assignment Graph (PAG)
	SVFIRBuilder builder;
	SVFIR* pag = builder.build(svfModule);

    string ptaName = "";
    string argNameDDA = "--pta-dda";
    string argNameFS = "--pta-fs";
    string argNameAnder = "--pta-ander";

    if(argc >= 3){

        outs() << "args PTA type: " << argv[2] << "\n";
        if (argNameDDA.find(string(argv[2])) != std::string::npos){ //s.compare(t)
            ptaName = "dda";
            DDAClient *client = new DDAClient(svfModule);
            client->initialise(svfModule);
            FlowDDA *dda = new FlowDDA(pag, client);
            pta = dda;

            dda->initialize();
            client->answerQueries(dda);
        }else if(argNameAnder.find(string(argv[2])) != std::string::npos){
            ptaName = "ander";
            Andersen* ander = AndersenWaveDiff::createAndersenWaveDiff(pag);
            pta = ander;
        }else if (argNameFS.find(string(argv[2])) != std::string::npos){
            ptaName = "fs";
            FlowSensitive *fs = FlowSensitive::createFSWPA(pag);
            pta = fs;
        }else{
            outs() << "No PTA set, exit" << "\n";
            exit(1);
        }
    }else{
        outs() << "/path_to_svf_bin/svf-ex /path_to_bc/file.bc --pta-dda /path_to_write_output_file" << "\n";
        exit(1);
    }

    //Zbouncer *zbouncer = new Zbouncer();
    Module *m = LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule();
    //zbouncer->runOnModule(m);

    /// Call Graph
    PTACallGraph* callgraph = pta->getPTACallGraph();

    /// ICFG
    ICFG* icfg = pag->getICFG();

    /// Value-Flow Graph (VFG)
    VFG* vfg = new VFG(callgraph);

    /// Sparse value-flow graph (SVFG)
    SVFGBuilder svfBuilder;
    SVFG* svfg = svfBuilder.buildFullSVFG(pta);
    svfg->updateCallGraph(pta);
    outs() << "Test - update svfg\n";

    PAG::CallSiteSet callSite = pag->getCallSiteSet();
    std::vector<PAGNode *>* getDefiningNodes(PAGNode* pgn);

    std::vector<TraceEntity *> trace;
    std::vector<Def *> deflist;
    DefBuilder *db = new DefBuilder(pag);

    /*
    for (Function &F : *m){
        outs() << "Checking function " << F.getName() << "\n";
        for (BasicBlock &bb : F.getBasicBlockList()){
            for (Instruction &instr : bb){
                if(StoreInst *si = dyn_cast<StoreInst>(&instr)){
                    outs() << "Finding defs for: ";
                    si->print(outs());
                    outs() << "\n";
                    std::vector<PAGNode *>* defs = findDefNodesFromStore(si,pag);
                    outs() << "Found:\n";
                    for(auto it = defs->begin(), eit = defs->end(); it != eit; it++){
                        outs() << "\t- NodeId: " << (*it)->getId() << " Instr: ";
                        (*it)->getValue()->print(outs());
                        outs() << "\n";
                    }
                }
            }
        }
    }*/
    

    for(PAG::CallSiteSet::iterator it = callSite.begin(), eit = callSite.end(); it != eit; ++it){
        const CallICFGNode *callBlockNode = *it;
        CallBase *call = (CallBase*) callBlockNode->getCallSite();

        if(call->getCalledFunction() == 0){
            outs() << "Dropping " << call->getName().str() << " because getCalledFunction is nullptr" << "\n";
            continue;
        }
        /*
            PAGNode *pg = const_cast<PAGNode *>(callBlockNode->getRetBlockNode()->getActualRet());
            std::vector<PAGNode *> *defs = findTargetOfCall(pg,pag);
            outs() << "Finding targets of ";
            pg->getValue()->print(outs());
            outs() << "\n";
            outs() << "Found " << defs->size() << " defs \n";

            for(auto it = defs->begin(), eit = defs->end(); it != eit; it++){
                PAGNode *d = *it;
                outs() << "\t- " << d->getId() << "\t";
                d->getValue()->print(outs());
                outs() << "\n";            
            }

            outs() << "------------\n";
        */

        

        if (std::find(inputFunctions.begin(), inputFunctions.end(), call->getCalledFunction()->getName()) != inputFunctions.end()){
                outs() << "(" << call->getCalledFunction()->getName().str() << ") " << callBlockNode->toString() <<  "\n";

                std::vector<const PAGNode *>paramList = callBlockNode->getActualParms();
                const PAGNode *functionParam;
                
                const PAGNode *ret = callBlockNode->getRetICFGNode()->getActualRet();

                if(call->getCalledFunction()->getName().str() == "wrapper_scanf")
                    functionParam = paramList[1];
                
                if(call->getCalledFunction()->getName().str() == "fgetc"){
                    functionParam = ret;
                    if(ret->getOutEdges().size() == 0){
                        outs() << "ERROR: fgetc call PAG::" << ret->getId() << " doesn't have an outgoing store edge" << "\n";
                    }else{
                        functionParam = findFirstStore((PAGNode *)ret);
                    }
                }
                
                outs() << "Finding uses of ";
                functionParam->getValue()->print(outs());
                outs() << " with ID " << functionParam->getId() << "\n";

                In *in = new In(const_cast<PAGNode *>(functionParam));
                trace.emplace_back(in);

                in->setBlacklistedCall(const_cast<CallICFGNode *>(callBlockNode));

                std::vector<PAGNode*> *defsResult = getDefNodes(const_cast<PAGNode*>(functionParam));
                
                for(auto it = defsResult->begin(), eit = defsResult->end(); it != eit; ++it){
                    PAGNode *def = *it;
                    //Discutere in call -> Dovrebbe essere equivalente a quanto fatto prima
                    if(!def)
                        continue;

                    outs() << "Found:\n";
                    Value *v = const_cast<Value*>(def->getValue());
                    outs() << "\t- " << def->getId() << " ";
                    v->print(outs());
                    outs() << "\n";

                    //Non dovrebbe mai verificarsi, discuterne in call
                    if(dyn_cast<CallInst>(v)){
                        PAGNode *defVector = findTargetOfCall(def,pag);
                        v = const_cast<Value*>(defVector->getValue());
                        outs() << "\tValue is a call, return type stored to ";
                        v->print(outs());
                        outs() << "\n";
                    }
                    
                    Def *d = getOrBuildDef(&deflist,db,def);//db->buildDef(def);
                    in->entityInsert(d);
                    deflist.emplace_back(d);

                    std::vector<VFGNode *> *allUseNodes = traverseOnVFG(svfg, (Value *)v);
                    std::vector<VFGNode *> *storeUse = VFGNodeFilter(allUseNodes, VFGNode::Store);

                    
                    for (vector<VFGNode *>::iterator it = storeUse->begin(), eit = storeUse->end(); it != eit; ++it){
                        VFGNode *storeNode = *it;
                        StoreInst *store = dyn_cast<StoreInst>(const_cast<Value*>(storeNode->getValue()));

                        if(!store){
                            outs() << "Instruction " << *storeNode->getValue() << " is not a store. Skipping\n";
                            continue;
                        }
                        Value *where = store->getOperand(1);

                        //Variabile globale non pre-inizializzata
                        if(dyn_cast<ConstantPointerNull>(const_cast<Value *>(storeNode->getValue())) && dyn_cast<GlobalValue>(const_cast<Value *>(def->getValue()))){
                            outs() << "Ignoring " << storeNode->toString() << "\n";
                            continue;
                        }

                        SUse *use = new SUse(storeNode,store);
                        use->setStoreParamSVFGNode(store->getPointerOperand());
                        in->entityInsert(use);

                        #ifdef DEBUG
                        outs() << "Store: " << *store << "\n";
                        outs() << "Store where: " << *where << "\n";
                        #endif

                        /*
                        for(const SVFGNode *node : svfg->fromValue(storeNode->getValue())){
                            for(const PAGEdge *edge : node->getICFGNode()->getPAGEdges()){
                                SVFGNode *defOfStoreNode = const_cast<SVFGNode *>(svfg->getDefSVFGNode(edge->getDstNode()));
                                PAGNode *pagDef = pag->getPAGNode(pag->getValueNode(defOfStoreNode->getValue()));

                                #ifdef DEBUG
                                outs() << "SVFG  node: " << *node << "\n";
                                outs() << "SVFG Def site: " << *defOfStoreNode << "\n";
                                outs() << "PAG  Def site: " << *pagDef << "\n";
                                outs() << "ICFG node: " << *(node->getICFGNode()) << "\n";
                                outs() << "PAG Edge: " << edge->getSrcID() << " --> " << edge->getDstID() << "(" << edge->getEdgeKind() << ")\n";
                                #endif

                                
                                std::vector<PAGNode *> *defsStoreResult = getDefNodes(pagDef);
                                
                                outs() << "Found defs:\n";
                                for (auto it = defsStoreResult->begin(), eit = defsStoreResult->end(); it != eit; ++it){
                                    PAGNode *p = *it;
                                    outs() << *p << "\n";
                                    Def *d = getOrBuildDef(&deflist,db,p);//db->buildDef(p);
                                    use->addTargetObject(d);
                                    deflist.emplace_back(d);
                                }
                            }
                        }*/

                        
                        //Parlare in call su quale soluzione sia migliore
                                
                        std::vector<PAGNode *> *defsStoreResult = findDefNodesFromStore(store,pag);
                        
                        outs() << "[STORE] Found defs:\n";
                        for (auto it = defsStoreResult->begin(), eit = defsStoreResult->end(); it != eit; ++it){
                            PAGNode *p = *it;
                            Type *t = p->getValue()->getType();
                            if(t->isStructTy() || ( t->isPointerTy() && t->getPointerElementType()->isStructTy())){
                                analyzeGep(p,pag,svfg,store, &deflist,use);

                                //}
                            } else {
                                Def *d = getOrBuildDef(&deflist,db,p);//db->buildDef(p);
                                use->addTargetObject(d);
                                deflist.emplace_back(d);
                            }
                        }    
                    }
            }
        } 

        if(std::find(libFunctions.begin(), libFunctions.end(), call->getCalledFunction()->getName()) != libFunctions.end()){
            outs() << "(lib: " << call->getCalledFunction()->getName().str() << ") " << callBlockNode->toString() <<  "\n";

            std::vector<const PAGNode *>paramList = callBlockNode->getActualParms();
            const PAGNode *functionParam;

            //const PAGNode *ret = callBlockNode->getRetICFGNode()->getActualRet();

            if(call->getCalledFunction()->getName().str() == "scanf")
                functionParam = paramList[1];
            if(call->getCalledFunction()->getName().str() == "__isoc99_scanf")
                functionParam = paramList[1];
            if(call->getCalledFunction()->getName().str() == "strcpy")
                functionParam = paramList[0];
            if(call->getCalledFunction()->getName().str() == "memcpy")
                functionParam = paramList[0];

            In *in = new In(const_cast<PAGNode *>(functionParam));
            trace.emplace_back(in);

            in->setBlacklistedCall(const_cast<CallICFGNode *>(callBlockNode));

            outs() << "Finding defs of " << functionParam->toString() << "\n";
            std::vector<PAGNode *> *defsResult = getDefNodes(const_cast<PAGNode*>(functionParam));

            LUse *luse = new LUse(const_cast<CallICFGNode *>(callBlockNode));
            in->entityInsert(luse);
            luse->setCallTargetParam(const_cast<Value *>(functionParam->getValue()));


            outs() << "Defs:\n";
            for (auto it = defsResult->begin(), eit = defsResult->end(); it != eit; ++it){
                PAGNode *def = *it;
                outs() << def->toString() << "\n";
                Type *t = def->getValue()->getType();

                if(t->isStructTy() || ( t->isPointerTy() && t->getPointerElementType()->isStructTy())){
                    analyzeGep(def,pag,svfg,call,&deflist,luse);
                } else {
                    Def *d = getOrBuildDef(&deflist,db,def);//db->buildDef(def);
                    //in->entityInsert(d);
                    deflist.emplace_back(d);
                    luse->addTargetObject(d);
                }
                
            }
        }
    }

    LLVMContext &ctx = LLVMModuleSet::getLLVMModuleSet()->getContext();
    
    for(PAG::CallSiteSet::iterator cs = callSite.begin(); cs != callSite.end(); cs++){
        const CallICFGNode *callBlockNode = *cs;
        CallBase *callee = (CallBase*) callBlockNode->getCallSite();
        Function *caller = callee->getCaller();
        
        if(callee->getCalledFunction() == 0)
            continue;
        
        string func_name = string(callee->getCalledFunction()->getName());
        outs() << " Analyzing call to " << func_name << " inside " <<  callee->getCaller()->getName() << "\n" ; 

        bool instrumented = ends_with(func_name, "_instrumented");

        if(!instrumented){
            continue;
        }
    
        outs() << "Function " << func_name << "is instrumented\n";
        
        std::vector<const PAGNode *>paramList = callBlockNode->getActualParms();

        for( long unsigned int i = 0; i < paramList.size() ; i++){
            if(!paramList[i]->isPointer()){
                outs() << "Parameter " << i << " is not a pointer\n";
                continue;
            }

            outs() << "Parameter " << i <<  " is a pointer\n";


            llvm::Argument *arg;
            //Discuterne in call -> errore su xml parser per chiamata xml_strtok_r_instrumented con parametro null
            if (!paramList[i]->hasValue()){
                continue;
            }

            if ((arg = check_parameter_aliasing(paramList[i]->getValue(),caller,pta)) != NULL){
                string name = "zref_" + string(m->getFunction(func_name)->getArg(i)->getName()) + "_id";
                string value = string((*arg).getName()) + "_id";
                callee->setMetadata(name,MDNode::get(ctx, MDString::get(ctx, value)));

                outs() << "Parameter " << i << " is aliased with argument " << arg->getName() << "\n";
                continue;
            }
                

            const SVFGNode *defOfParam = getSVFGNodesFromValue(pag,paramList[i]->getValue(),svfg)[0];
            outs() << "Def of param: " << defOfParam->toString() << "\n";
            std::vector<PAGNode *> *defsResult = getDefNodes(pag->getGNode(pag->getValueNode(defOfParam->getValue())));
            outs() << "Defs of param " << i << " are " << defsResult->size() << "\n";

            for(auto it = defsResult->begin(); it != defsResult->end(); it++ ){
                PAGNode *def = *it;
                
                if(isDominatorNode(def)){
                    outs() << "Ignoring " << def->toString() << " since it's a dominator\n";
                    continue;
                }

                outs() << def->toString() << "\n";
       
                char text[100];

                int def_id = get_def_id(deflist,def);

                if(def_id == -1){                        
                    outs() << "ISSUE: Definition of " << def->toString() << " is not present in trace\n";

                    Type *t = def->getValue()->getType();

                    if(t->isStructTy() || ( t->isPointerTy() && t->getPointerElementType()->isStructTy())){
                        vector<int> *ids = analyzeGep(def,pag,svfg,const_cast<Value *>(defOfParam->getValue()),&deflist,NULL);
                        //assert(ids->size() == 1);
                        def_id = (*ids)[0];
                    } else {
                        Def *d = getOrBuildDef(&deflist,db,def);//db->buildDef(def);

                        //Discuterne in call
                        if(!d){
                            /*
                            d = getOrBuildDef(&deflist,db,findTargetOfCall(def,pag));
                            def_id = d->getId();
                            outs() << "ISSUE: Definition " << *def << " is a call to an unhandled function, falling back to store target with id"<< def_id << "\n";
                            */
                            continue;
                        }
                        trace.emplace_back(d);
                        deflist.emplace_back(d);
                        def_id = d->getId();
                    }
                        
                }

                Def * d = getDefWithID(&deflist,def_id);

                outs() << "Defs of param " << paramList[i]->getValueName() << " number " << i << " is " << d->getId() << " and has lenght " << 
                    d->getBytes() << "\n";

                string name = "zref_" + string(m->getFunction(func_name)->getArg(i)->getName()) + "_id";
                string value = "";
                sprintf(text,"%d",def_id);
                value.append(text);

                callee->setMetadata(name,MDNode::get(ctx, MDString::get(ctx, value)));
 
                break;
            }

        }
    }

    for (Function &F : *m){
        outs() << "Checking function " << F.getName() << "\n";
        for (BasicBlock &bb : F.getBasicBlockList()){
            for (Instruction &instr : bb){
                if(IntToPtrInst *iptr = dyn_cast<IntToPtrInst>(&instr)){
                    outs() << "############ Found inttopr in " << F.getName() << "  ";
                    iptr->print(outs());
                    outs() << "\n";

                    Value *v = iptr->getOperand(0);
                    outs() << "############ Target " ;
                    v->print(outs());
                    outs() << "\n";

                    PAGNode *sourceNode = pag->getGNode(pag->getValueNode(v));
                    outs() << "############ Corresponding PAG Node " << sourceNode->getId() << "\n";
                    SVFGNode * n = const_cast<SVFGNode *>(svfg->getDefSVFGNode(sourceNode));
                    outs() << "[inttoptr analysis] On Node " << n->getId() << "\n";

                    std::vector<SVFGNode *>* callBlocks = visitAndSelectSVFGNodes(n,selectCallBlockNodes);

                    for(SVFGNode *callBlock : *callBlocks){
                        outs() << "[inttoptr analysis] Found callblock node to ";
                        callBlock->getValue()->print(outs());
                        outs() << "\n";

                        for(TraceEntity *te : trace){
                            if(In *in = trace_entity_dynamic_cast<In>(te)){
                                if(CallInst *ci = dyn_cast<CallInst>(const_cast<Instruction*>(in->getBlacklistedCall()->getCallSite()))){
                                    if(ci == callBlock->getValue()){
                                        outs() << "[inttoptr analysis] Instruction ";
                                        iptr->print(outs());
                                        outs() << " is input dependent\n";

                                        PAGNode *def = pag->getGNode(pag->getValueNode(iptr));
                                        Value *v = iptr;
                                        std::vector<VFGNode *> *allUseNodes = traverseOnVFG(svfg, (Value *) v);
                                        std::vector<VFGNode *> *storeUse = VFGNodeFilter(allUseNodes, VFGNode::Store);

                                        
                                        for (vector<VFGNode *>::iterator it = storeUse->begin(), eit = storeUse->end(); it != eit; ++it){
                                            VFGNode *storeNode = *it;
                                            StoreInst *store = dyn_cast<StoreInst>(const_cast<Value*>(storeNode->getValue()));

                                            if(!store){
                                                outs() << "Instruction " << *storeNode->getValue() << " is not a store. Skipping\n";
                                                continue;
                                            }
                                            Value *where = store->getOperand(1);

                                            //Variabile globale non pre-inizializzata
                                            if(dyn_cast<ConstantPointerNull>(const_cast<Value *>(storeNode->getValue())) && dyn_cast<GlobalValue>(const_cast<Value *>(def->getValue()))){
                                                outs() << "Ignoring " << storeNode->toString() << "\n";
                                                continue;
                                            }

                                            SUse *use = new SUse(storeNode,store);
                                            use->setStoreParamSVFGNode(store->getPointerOperand());
                                            in->entityInsert(use);

                                            #ifdef DEBUG
                                            outs() << "[inttoptr] Store: " << *store << "\n";
                                            outs() << "[inttoptr] Store where: " << *where << "\n";
                                            #endif
                                                    
                                            std::vector<PAGNode *> *defsStoreResult = findDefNodesFromStore(store,pag);
                                            
                                            outs() << "[inttoptr] Found defs:\n";
                                            for (auto it = defsStoreResult->begin(), eit = defsStoreResult->end(); it != eit; ++it){
                                                PAGNode *p = *it;
                                                outs() << p->toString() << "\n";
                                                Type *t = p->getValue()->getType();

                                                if(t->isStructTy() || ( t->isPointerTy() && t->getPointerElementType()->isStructTy())){
                                                    analyzeGep(p,pag,svfg,iptr,&deflist,use);
                                                } else {
                                                    Def *d = getOrBuildDef(&deflist,db,p);//db->buildDef(p);
                                                    use->addTargetObject(d);
                                                    deflist.emplace_back(d);
                                                }
                                            }    
                                        }
                                    }
                                }
                            }
                        }
                    }
                
                }
            }
        }
     }

    for( TraceEntity *te : trace){    
        for (Function &F : *m){
            outs() << "Checking function " << F.getName() << "\n";
            for (BasicBlock &bb : F.getBasicBlockList()){
                for (Instruction &instr : bb){
                    if(StoreInst *si = dyn_cast<StoreInst>(&instr)){
                        outs() << "Finding defs for: ";
                        si->print(outs());
                        outs() << "\n";
                        std::vector<PAGNode *>* defs = findDefNodesFromStore(si,pag);
                        outs() << "Found:\n";
                        for(auto it = defs->begin(), eit = defs->end(); it != eit; it++){
                            outs() << "\t- NodeId: " << (*it)->getId() << " Instr: ";
                            (*it)->getValue()->print(outs());
                            outs() << "\n";
                        }
                    }
                }
            }
        }

        if(!trace_entity_dynamic_cast<In>(te))
            continue;

        In *in = trace_entity_dynamic_cast<In>(te);

        outs() << "On input ";
        printIn(in);
        outs() << "\n";

        for(auto it = in->subObjectsBegin(), eit = in->subObjectsEnd(); it != eit; it++){            
            if(ZUse *use = asUse(*it)){
                PAGNode *start = pag->getGNode(pag->getValueNode(use->getTarget()));

                for(PAGEdge *pagEdge : start->getInEdges()){
                    #ifdef DEBUG
                    outs() << "Checking PagEdge node associated to: ";
                    pagEdge->getValue()->print(outs());
                    outs() << "\n";
                    #endif
                    if(pagEdge->getValue() == use->getTarget()){
                        start = pagEdge->getSrcNode();
                        outs() << "Visist starts at PAGNode " << start->getId() << "\n";
                        break;
                    }
                }

                PAGNode *pn = findFirstDominator(start);
                
                //Discuterne in call
                if(!pn)
                    continue;

                Type *t = pn->getValue()->getType();

                vector<int> *ids = new vector<int>();

                Value *v;

                if(SUse *suse = trace_entity_dynamic_cast<SUse>(use)){
                    v = suse->getStoreUse();
                } else {
                    v = trace_entity_dynamic_cast<LUse>(use)->getCall();
                }

                if(t->isStructTy() || ( t->isPointerTy() && t->getPointerElementType()->isStructTy())){
                    for ( int i : *analyzeGep(pn,pag,svfg,v,&deflist,NULL)) {
                        ids->emplace_back(i);
                    }
                } else {
                    ids->emplace_back(get_def_id(deflist,pn));
                }

                for (int id : *ids) {
                    Def * d = getDefWithID(&deflist,id);
                    Instruction *i;

                    if(!(i=getUseInstruction(use)))
                        continue;

                    outs() << "Considering Def:\n";
                    printDef(d);
                    outs() << "\nOf Use:\n";
                    printUse(use);
                    outs() << "\n";   

                    if(d->getType()->isAggregateType()){
                        outs() << "Avoiding pre-calculated indirect link\n";
                        continue;
                    }

                    assert(pn->getOutgoingEdges(PAGEdge::Addr).size() == 1);

                    PAGNode *target;
                    llvm::Argument *arg;
                    Function *func;

                    for(PAGEdge *pe : pn->getOutgoingEdges(PAGEdge::Addr)){
                        target = pe->getDstNode();
                    }

                    outs() << "Pag ID of source is " << target->getId() << "\n";
                    outs() << "Target: " << target->toString() << "\n";

                    for(auto it = target->getIncomingEdgesBegin(PAGEdge::Store), eit = target->getIncomingEdgesEnd(PAGEdge::Store) ; it != eit; it++){
                        StoreStmt *se = SVFUtil::cast<StoreStmt>(*it);
                        outs() << se->toString() << "\n" << *se->getValue() << "\n";

                        //Parlarne in call, non ho un grafico da vedere. Però il problema sembra essere legato agli usi di variabili globali in cui si va a cercare la store locale alla funzione.
                        if(!dyn_cast<Instruction>(const_cast<Value*>(se->getValue()))){
                            continue;
                        }

                        func = dyn_cast<Instruction>(const_cast<Value*>(se->getValue()))->getFunction();

                        if(!ends_with(string(func->getName()),"_instrumented"))
                            continue;
                        
                        //TODO: Controllare se sia RHS o LHS 
                        Value *store_src = const_cast<Value *>(se->getRHSVar()->getValue());

                        if((arg = check_parameter_aliasing(store_src,func,pta)) != NULL){
                            outs() << "Aliased with " << arg->getName() << "\n";

                            string name = "ziuse" ;
                            string value = string((*arg).getName()) + "_id";
                            i->setMetadata(name,MDNode::get(ctx, MDString::get(ctx, value)));

                            break;
                        }
                    }
                }
            }
        }
    }

    string output_main = string(argv[3]);

    exportTrace(trace, LLVMModuleSet::getLLVMModuleSet()->getContext(), m, output_main + "/ztrace/ztrace.txt" );

    outs() << "---------------------------------------------------" << "\n";

    pag->dump(output_main + "/graph/pag");
    svfg->dump(output_main + "/graph/svfg");
    icfg->dump(output_main + "/graph/icfg");

    // clean up memory
    delete vfg;
    delete svfg;
    SVFIR::releaseSVFIR();

    LLVMModuleSet::getLLVMModuleSet()->dumpModulesToFile(".svf.bc");
    
    std::string str;
    raw_string_ostream rawstr(str);


	return 0;
}
