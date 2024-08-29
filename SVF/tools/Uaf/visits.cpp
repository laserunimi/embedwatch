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
#include "visits.h"
#include <assert.h>

//#define DEBUG

using namespace std;
using namespace llvm;
using namespace SVF;

const std::vector<std::string> libFunctions = {"scanf", "__isoc99_scanf", "memcpy", "strcpy"};
const std::vector<std::string> inputFunctions = {
    "wrapper_gets", "wrapper_getc", "wrapper_fgets", "fgetc", "wrapper_fread", "wrapper_scanf"};


std::vector<PAGNode *>* getDefNodes(PAGNode* pgn){
    return visitAndSelectPagNodes(pgn,&selectDefs);
}

void selectDefs(PAGNode *current, std::vector<PAGNode *>* worklist, std::vector<PAGNode *> *visited, std::set<PAGNode *>* ret){
    #ifdef DEBUG
    outs() << "[SelectDef] On node: " << current->toString() << "\n";
    #endif

    for(PAGEdge *pe : current->getInEdges()){
            #ifdef DEBUG
            outs() << "On edge " << pe->getSrcID() << "-->" << pe->getDstID() << "\n";
            #endif

            if(pe->getSrcNode()->getNodeKind() == PAGNode::DummyObjNode || pe->getSrcNode()->getNodeKind() == PAGNode::DummyValNode){
                #ifdef DEBUG
                outs() << "Discarding edge\n";
                #endif

                continue;
            }

            if(pe->getEdgeKind() == PAGEdge::Addr){
                if(current->getInEdges().size() > 1){
                    #ifdef DEBUG
                    outs() << "[SelectDefs] Found AddrEdge with other incoming edges, probably a dominator:\n\t- " << pe->getDstID()<<"\n\t-";
                    pe->getSrcNode()->getValue()->print(outs());
                    outs() << "\n";
                    #endif
                } 

                #ifdef DEBUG
                outs() << "[SelectDefs] Adding node: " << pe->getSrcNode()->toString() << "\n";
                #endif
                
                ret->insert(pe->getSrcNode());
                            
            } else {
                if(std::find(visited->begin(),visited->end(),pe->getSrcNode()) == visited->end()){
                    worklist->emplace_back(pe->getSrcNode());
                    visited->emplace_back(pe->getSrcNode());
                } else {
                    #ifdef DEBUG
                    outs() << "Node " << pe->getSrcID() << " already visited\n";
                    #endif
                }
            }
    }

    visited->emplace_back(current);

}

bool isDominatorNode(PAGNode *pn){
    PAGNode *dst;

    //assert(pn->getOutgoingEdges(PAGEdge::Addr).size() == 1);

    for(PAGEdge *pe : pn->getOutgoingEdges(PAGEdge::Addr)){
        dst  =pe->getDstNode();
    }

    return dst->getInEdges().size() > 1;
    
}

bool concreteGetPathFromTo(SVFGNode *from, Value *to, std::vector<SVFGNode *> *path,  std::vector<SVFGNode *> *visited, vector<vector<SVFGNode *>*>* ret) {
    if(std::find(visited->begin(),visited->end(),from) != visited->end()){
        return false;
    }

    visited->emplace_back(from);
    
    for (VFGEdge *p : from->getOutEdges()){
        //outs() << "On edge " << p->getSrcID() << "-->" << p->getDstID() << "\n";
        path->push_back(p->getDstNode());
        if(p->getDstNode()->getValue() == to){
            vector<SVFGNode *>* p = new vector<SVFGNode *>(*path);
            ret->push_back(p);
        }

        concreteGetPathFromTo(p->getDstNode(),to, path, visited,ret);

        path->pop_back();
        
    }

    return ! ret->empty();
}

std::vector<std::vector<SVFGNode *>*>* getPathFromTo(SVFGNode *from, Value *to) {
    std::vector<SVFGNode *>* path = new std::vector<SVFGNode *>();
    std::vector<SVFGNode *>* visited = new std::vector<SVFGNode *>();
    std::vector<std::vector<SVFGNode *>*>* paths = new std::vector<std::vector<SVFGNode *>*>();
    
    concreteGetPathFromTo(from, to, path, visited,paths);

    return paths;
}

std::vector<PAGNode *>* visitAndSelectPagNodes(PAGNode* start, void (*selectionFunction)(PAGNode *, std::vector<PAGNode *>*,std::vector<PAGNode *>*,std::set<PAGNode *>* )){
    std::set<PAGNode *>* ret = new std::set<PAGNode *>();
    std::vector<PAGNode *>* worklist = new std::vector<PAGNode *>();
    std::vector<PAGNode *>* visited = new std::vector<PAGNode *>();

    PAGNode *current;

    worklist->emplace_back(start);
    while(!worklist->empty()){
        current = worklist->back();
        worklist->pop_back();

        selectionFunction(current,worklist,visited,ret);
    }

    return new std::vector<PAGNode *>(ret->begin(),ret->end());

}

std::vector<PAGNode *>* findDefNodesFromStore(StoreInst *si,PAG *pag){
    Value *target = si->getOperand(1);
    PAGNode * start;

    start = pag->getGNode(pag->getValueNode(target));

    #ifdef DEBUG
    outs() << "Target of ";
    si->print(outs());
    outs() << " is ";
    target->print(outs());
    outs() << " with PAG id " << start->getId() << "\n";
    #endif


    for(PAGEdge *pagEdge : start->getInEdges()){
        #ifdef DEBUG
        outs() << "Checking PagEdge node associated to: ";
        pagEdge->getValue()->print(outs());
        outs() << "\n";
        #endif
        if(pagEdge->getValue() == target){
            start = pagEdge->getSrcNode();
            #ifdef DEBUG
            outs() << "Visist starts at PAGNode " << start->getId() << "\n";
            #endif
            break;
        }
    }

    std::vector<PAGNode*> *ret =  getDefNodes(start);

    if(start->getOutgoingEdges(PAGEdge::Addr).size() > 0){
        #ifdef DEBUG
        outs() << "Start node is a Def already, adding "<< start->toString() << " to result\n";
        #endif
        ret->emplace_back(start);
    }

    return ret;
}

void selectFirstStore(PAGNode *current, std::vector<PAGNode *>* worklist, std::vector<PAGNode *>* visited, std::set<PAGNode *>* ret){
    for(PAGEdge *pe : current->getOutEdges()){

        if(pe->getSrcNode()->getNodeKind() == PAGNode::DummyObjNode || pe->getSrcNode()->getNodeKind() == PAGNode::DummyValNode)
                continue;

        if(pe->getEdgeKind() == PAGEdge::Store){
             ret->insert(pe->getDstNode());
             worklist->clear();
             break;
        } else {
            worklist->emplace_back(pe->getDstNode());
        }
    }
}



void selectAllStore(PAGNode *current, std::vector<PAGNode *>* worklist, std::vector<PAGNode *>* visited, std::set<PAGNode *>* ret){
    for(PAGEdge *pe : current->getOutEdges()){

        if(pe->getSrcNode()->getNodeKind() == PAGNode::DummyObjNode || pe->getSrcNode()->getNodeKind() == PAGNode::DummyValNode)
            continue;

        if(pe->getEdgeKind() == PAGEdge::Store){
            ret->insert(pe->getDstNode());
            //worklist->clear();
            //break;
        }
        worklist->emplace_back(pe->getDstNode());
        /* else {
            worklist->emplace_back(pe->getDstNode());
        } */
    }
}

std::set<PAGNode *> findAllStore(PAGNode *top){
    vector<PAGNode *> *ret = visitAndSelectPagNodes(top,&selectAllStore);
    std::set<PAGNode *> retset(ret->begin(), ret->end());
    return retset;
}

PAGNode *findFirstStore(PAGNode *top){
    vector<PAGNode *> *ret = visitAndSelectPagNodes(top,&selectFirstStore);

    #ifdef DEBUG
    outs() << "[DEBUG] findFirstStore found " << ret->size() << " elements\n"; 
    #endif

    if(ret->size() == 0)
        return nullptr;
    else
        return ret->at(0);

}

void selectFirstDominator(PAGNode *current, std::vector<PAGNode *>* worklist, std::vector<PAGNode *>* visited, std::set<PAGNode *>* ret){
    for(PAGEdge *pe : current->getInEdges()){
        if(pe->getSrcNode()->getNodeKind() == PAGNode::DummyObjNode || pe->getSrcNode()->getNodeKind() == PAGNode::DummyValNode)
                continue;

        if(pe->getEdgeKind() == PAGEdge::Addr){
            ret->insert(pe->getSrcNode());
            worklist->clear();   
            break;
        } else {
            worklist->emplace_back(pe->getSrcNode());
        }
    }
}

PAGNode *findFirstDominator(PAGNode *top){
    vector<PAGNode *> *ret = visitAndSelectPagNodes(top,&selectFirstDominator);

    #ifdef DEBUG
    outs() << "[DEBUG] findFirstDominator found " << ret->size() << " elements\n"; 
    #endif

    if(ret->size() == 0)
        return nullptr;
    else
        return ret->at(0);
}

//FIX: Non vanno trovate tutte le definizioni ma solo il primo dominatore 
PAGNode *findTargetOfCall(PAGNode *callNode,PAG *pag){
    //Value *v = const_cast<Value*>(callNode->getValue());
    //CallInst *call = dyn_cast<CallInst>(v);
    PAGNode *storeNode = findFirstStore(callNode);
    if(!storeNode)
        return nullptr;

    if(storeNode->getIncomingEdges(PAGEdge::Store).size() > 1)
        outs() << "[ISSUE] More than one store edge found\n";

    for(PAGEdge *pe : storeNode->getIncomingEdges(PAGEdge::Store)){

        if(pe->getSrcNode()->getNodeKind() == PAGNode::DummyObjNode || pe->getSrcNode()->getNodeKind() == PAGNode::DummyValNode)
            continue;

        StoreInst *si = dyn_cast<StoreInst>(const_cast<Value*>(pe->getValue()));
        Value *target = si->getOperand(1);
        PAGNode* start = pag->getGNode(pag->getValueNode(target));
        
        return findFirstDominator(start);
    }

    return nullptr;
}

std::vector<SVFGNode *>* visitAndSelectSVFGNodes(SVFGNode* start, void (*selectionFunction)(SVFGNode *, std::vector<SVFGNode *>*,std::vector<SVFGNode *>*,std::set<SVFGNode *>* )){
    std::set<SVFGNode *>* ret = new std::set<SVFGNode *>();
    std::vector<SVFGNode *>* worklist = new std::vector<SVFGNode *>();
    std::vector<SVFGNode *>* visited = new std::vector<SVFGNode *>();

    SVFGNode *current;

    worklist->emplace_back(start);
    while(!worklist->empty()){
        current = worklist->back();
        worklist->pop_back();

        selectionFunction(current,worklist,visited,ret);
    }

    return new std::vector<SVFGNode *>(ret->begin(),ret->end());
}

void selectCallBlockNodes(SVFGNode *current, std::vector<SVFGNode *>* worklist, std::vector<SVFGNode *>* visited, std::set<SVFGNode *>* ret){
    visited->emplace_back(current);
        
    if(current->getValue() != NULL){
        if(dyn_cast<CallInst>(const_cast<Value *>(current->getValue()))){
            ret->insert(current);
        }
    }

    for(SVFGEdge *se : current->getInEdges()){
        if(std::find(visited->begin(),visited->end(),se->getSrcNode()) == visited->end()){
            worklist->emplace_back(se->getSrcNode());
        } else {
            #ifdef DEBUG
            outs() << "Node " << se->getSrcID() << " already visited\n";
            #endif
        }
    }
}





