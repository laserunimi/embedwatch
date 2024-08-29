#include <vector>
#include <queue>
#include <unordered_set>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <string>  
#include <sstream> 
#include "SVF-FE/LLVMUtil.h"
#include "Graphs/SVFG.h"
#include "Graphs/VFG.h"
#include "WPA/Andersen.h"
#include "WPA/FlowSensitive.h"
#include "SVF-FE/SVFIRBuilder.h"
#include "Util/Options.h"
#include "DDA/FlowDDA.h"
#include "DDA/ContextDDA.h"
#include "DDA/DDAVFSolver.h"
#include "DDA/DDAPass.h"
#include "DDA/DDAStat.h"
#include "DDA/DDAClient.h"
#include "MemoryModel/SVFStatements.h"
#include "svf-uaf.h"
#include "visits.h"

using namespace llvm;
using namespace std;
using namespace SVF;

#define DEBUG
#define ARCH_PTR_SIZE 32

std::vector<ICFGNode *> allocAndFreeICFGNode;

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

void replace(std::string& str, const std::string& from, const std::string& to) {
    if(from.empty())
        return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

// Can return null
ICFGNode *getICFGNodeFromPAGNode(ICFG *icfg, PAGNode *n){
    for(PAGEdge *e : n->getOutEdges()){
        const Instruction *inst = e->getInst();
        if(inst == nullptr){
            return NULL;
        }
        return icfg->getICFGNode(inst);
    }

    return NULL;
}

enum MFPathType{
    Alloc,
    Free,
    Solved,
};

class MFPath {
public:
    MFPath(ICFGNode *freeICFG, PAGNode *freePAG, const CallICFGNode *freeCall, set<PAGNode *> *freeDefs,
            ICFGNode *allocICFG, PAGNode *allocPAG, const CallICFGNode *allocCall, set<PAGNode *> *allocDefs) {

        free = std::make_tuple(freeICFG, freePAG, freeCall, freeDefs);
        alloc = std::make_tuple(allocICFG, allocPAG, allocCall, allocDefs);

        if (allocDefs != nullptr)
            pathDefs.insert(freeDefs->begin(), freeDefs->end());

        if (allocDefs != nullptr)
            pathDefs.insert(allocDefs->begin(), allocDefs->end());

    }

    ~MFPath() {}

    void addLastAlloc(ICFGNode *allocICFG, PAGNode *allocPAG, const CallICFGNode *allocCall, set<PAGNode *> *allocDefs) {
        lastAlloc.insert(std::make_tuple(allocICFG, allocPAG, allocCall, allocDefs));
    }

    ICFGNode *getICFGAlloc(){
        ICFGNode *nodeICFG;
        tie(nodeICFG, std::ignore, std::ignore, std::ignore) = alloc;
        return nodeICFG;
    }

    PAGNode *getPAGAlloc(){
        PAGNode *nodePAG;
        tie(std::ignore, nodePAG, std::ignore, std::ignore) = alloc;
        return nodePAG;
    }

    ICFGNode *getICFGFree(){
        ICFGNode *nodeICFG;
        tie(nodeICFG, std::ignore, std::ignore, std::ignore) = free;
        return nodeICFG;
    }

    PAGNode *getPAGFree(){
        PAGNode *nodePAG;
        tie(std::ignore, nodePAG, std::ignore, std::ignore) = free;
        return nodePAG;
    }

    int getMFMPathCounter(){
        return lastAlloc.size();
    }

    bool isIntraFunction(){
        ICFGNode *aICFG, *fICFG;
        tie(aICFG, std::ignore, std::ignore, std::ignore) = alloc;
        tie(fICFG, std::ignore, std::ignore, std::ignore) = free;   
        if(aICFG->getFun() == fICFG->getFun())
            return true;            
        return false;
    }

    void printMFPath(){
        ICFGNode *aICFG, *fICFG;
        PAGNode *aPAG, *fPAG;
        tie(aICFG, aPAG, std::ignore, std::ignore) = alloc;
        tie(fICFG, fPAG, std::ignore, std::ignore) = free;  

        outs() << "MFPath\n";
        outs() << "alloc ICFG=" << aICFG->getId() << " (PAG=" << aPAG->getId() << ") --> " << "free ICFG=" << aICFG->getId() << " (PAG=" << aPAG->getId() << ")\n";
        
        outs() << "DEFS: ";
        for (PAGNode *d : pathDefs) 
            outs() << d->getId() << ", ";
        outs() << "\n";
    }

    void printMFPathFull(){
        ICFGNode *aICFG, *fICFG;
        PAGNode *aPAG, *fPAG;
        tie(aICFG, aPAG, std::ignore, std::ignore) = alloc;
        tie(fICFG, fPAG, std::ignore, std::ignore) = free;  

        outs() << "MFPath\n";
        outs() << "alloc ICFG=" << aICFG->toString() << "\n";
        outs() << "free ICFG=" << aICFG->toString() << "\n";
        
        outs() << "DEFS: ";
        for (PAGNode *d : pathDefs) 
            outs() << "\t[-]" << d->toString() << " ";
        outs() << "\n";
    }

private:
    tuple<ICFGNode *, PAGNode *, const CallICFGNode *, set<PAGNode *> *> alloc;
    tuple<ICFGNode *, PAGNode *, const CallICFGNode *, set<PAGNode *> *> free;
    set<tuple<ICFGNode *, PAGNode *, const CallICFGNode *, set<PAGNode *> *>> lastAlloc;
    set<PAGNode *> pathDefs;

    ICFG *icfg;
};


template <typename T>
class Graph {
public:
    // Constructor
    Graph() {}

    // Destructor
    ~Graph() {}

    // Function to add a node to the graph
    void addNode(const T& value) {
        if (nodes.find(value) == nodes.end()) {
            nodes[value] = std::unordered_set<T>();
        }
    }

    // Function to add a directed edge from 'from' to 'to' in the graph
    void addEdge(const T& from, const T& to) {
        nodes[from].insert(to);
    }

    // Function to remove a node and its associated edges from the graph
    void removeNode(const T& value) {
        // Remove incoming edges associated with the node
        for (auto& entry : nodes) {
            entry.second.erase(value);
        }
        // Remove the node itself
        nodes.erase(value);
    }


    // Function to remove a directed edge from 'from' to 'to' in the graph
    void removeEdge(const T& from, const T& to) {
        auto it = nodes.find(from);
        if (it != nodes.end()) {
            it->second.erase(to);
        }
    }

    // Function to get incoming nodes for a node in a directed graph
    std::vector<T> getIncomingNodes(const T& node) const {
        std::vector<T> incomingNodes;
        for (const auto& entry : nodes) {
            if (entry.second.find(node) != entry.second.end()) {
                incomingNodes.push_back(entry.first);
            }
        }
        return incomingNodes;
    }

    // Function to get outgoing nodes for a node in a directed graph
    std::vector<T> getOutgoingNodes(const T& node) const {
        auto it = nodes.find(node);
        if (it != nodes.end()) {
            return std::vector<T>(it->second.begin(), it->second.end());
        }
        return std::vector<T>();
    }

    bool hasPath(const T& node1, const T& node2) const {
        std::unordered_set<T> visited;
        std::stack<T> stack;
        stack.push(node1);

        while (!stack.empty()) {
            T current = stack.top();
            stack.pop();

            if (current == node2) {
                return true;
            }

            visited.insert(current);
            for (const T& neighbor : getOutgoingNodes(current)) {
                if (visited.find(neighbor) == visited.end()) {
                    stack.push(neighbor);
                }
            }
        }

        return false;
    }

    // Function to print the graph
    void printICFGGraph() {
        for (const auto& entry : nodes) {
            ICFGNode *n = entry.first;
            std::cout << n->getId() << " >>> ";
            for (const T& neighbor : entry.second) {
                ICFGNode *v = neighbor;
                std::cout << v->getId() << " ";
            }
            std::cout << std::endl;
            std::cout << std::endl;
        }
    }

    // Function to print the graph
    void printICFGGraphFull() {
        for (const auto& entry : nodes) {
            ICFGNode *n = entry.first;
            std::cout << n->toString() << " >>> ";
            for (const T& neighbor : entry.second) {
                ICFGNode *v = neighbor;
                std::cout << v->toString() << " ";
            }
            std::cout << std::endl;
            std::cout << std::endl;
        }
    }

    // Function to print the graph in DOT format to a file
    void printICFGDotGraph(const std::string& filename) const {
        std::ofstream dotFile(filename);

        if (!dotFile.is_open()) {
            std::cerr << "Error opening file: " << filename << std::endl;
            return;
        }

        dotFile << "digraph G {" << std::endl;

        for (const auto& entry : nodes) {
            ICFGNode *n = entry.first;

            if(n->getId() != 0){
                std::string name = n->toString(); 
                replace(name, "\n", "\\n");
                replace(name, "{", "\\{");
                replace(name, "}", "\\}");
                replace(name, "<", "\\<");
                // Print the node
                if(name.find("malloc") != std::string::npos || name.find("calloc") != std::string::npos || name.find("realloc") != std::string::npos)
                    dotFile << "Node" << n << " [shape=record,color=blue,label=\"" << name << "\"];" << std::endl;
                else if(name.find("alloc") != std::string::npos)
                    dotFile << "Node" << n << " [shape=record,color=green,label=\"" << name << "\"];" << std::endl;
                else
                    dotFile << "Node" << n << " [shape=record,color=red,label=\"" << name << "\"];" << std::endl;
            }else{
                dotFile << "Node" << n << " [shape=record,color=black,label=\"start\"];" << std::endl;
            }
        }


        for (const auto& entry : nodes) {
            ICFGNode *n = entry.first;
            // Print the outgoing edges
            for (const T& neighbor : entry.second) {
                ICFGNode *v = neighbor;
                dotFile << "Node" << n << " -> Node" << v << " [style=solid];" << std::endl;
            }
        }

        dotFile << "}" << std::endl;

        dotFile.close();
        std::cout << "DOT file '" << filename << "' has been created." << std::endl;
    }


private:
    std::unordered_map<T, std::unordered_set<T>> nodes; // Adjacency list representation
};


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
    while (!worklist.empty()){
        const VFGNode* vNode = worklist.pop();
        for (VFGNode::const_iterator it = vNode->OutEdgeBegin(), eit = vNode->OutEdgeEnd(); it != eit; ++it){
            VFGEdge* edge = *it;
            VFGNode* succNode = edge->getDstNode();
            if (visited.find(succNode) == visited.end()){
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


bool evictedIfNotAllocOrFree(ICFGNode *node){

    for(ICFGNode *n : allocAndFreeICFGNode){
        if(n->getId() == node->getId()){
            outs() << "*** Not evicted - " << node->toString() << "\n";
            return false;
        }
    }

    return true;
}

// Function to traverse the graph and remove nodes based on a condition
void traverseAndRemoveNodes(Graph<ICFGNode *> *g, ICFGNode *start, ICFGNode *end, bool (*evictionCriteria)(ICFGNode *)) {
    std::unordered_set<ICFGNode *> visited;
    std::queue<ICFGNode *> worklist;

    worklist.push(start);

    while (!worklist.empty()) {
        ICFGNode *current = worklist.front();
        worklist.pop();

        //if (current == end)
        //    break;

        visited.insert(current);

        for(ICFGNode *succNode : g->getOutgoingNodes(current)){
            if (visited.find(succNode) == visited.end()){
                visited.insert(succNode);
                worklist.push(succNode);
            }
        }

        // Check if the node is in allocAndFreeFn whitelist
        if (evictionCriteria(current)) { 
            std::vector<ICFGNode *>nodi_entranti = g->getIncomingNodes(current);
            std::vector<ICFGNode *>nodi_uscenti = g->getOutgoingNodes(current);

            g->removeNode(current);

            for(ICFGNode *in : nodi_entranti){
                for(ICFGNode *out : nodi_uscenti){
                    g->addEdge(in, out);
                }
            }
        }

    }
}

// Function to traverse the graph and remove nodes based on a condition
void traverseAndCopyGraph(Graph<ICFGNode *> *g, ICFG *icfg, ICFGNode *start, ICFGNode *end) {
    std::unordered_set<ICFGNode *> visited;
    std::queue<ICFGNode *> worklist;

    worklist.push(start);
    g->addNode(start);

    while (!worklist.empty()) {
        ICFGNode *current = worklist.front();
        worklist.pop();

        visited.insert(current);
        g->addNode(current);

        for (ICFGNode::const_iterator it = current->OutEdgeBegin(), eit = current->OutEdgeEnd(); it != eit; ++it){
            ICFGEdge* edge = *it;
            ICFGNode* succNode = edge->getDstNode();
            if (visited.find(succNode) == visited.end()){
                visited.insert(succNode);
                worklist.push(succNode);
            }
            g->addNode(succNode);
            g->addEdge(current, succNode);
        }

    }
}

// Function to traverse the graph and remove nodes based on a condition
void traverseAndCreateAllocFreeFlowGraph(Graph<ICFGNode *> *g, vector<SVF::ICFGNode *> *allocNodes, vector<SVF::ICFGNode *> *defFreeNodes) {
    std::unordered_set<ICFGNode *> visited;
    std::vector<tuple<ICFGNode *, ICFGNode *>> worklist; // <next_target,from_node>

    ICFGNode dummyICFGNode = ICFGNode(-1, SVF::ICFGNode::FunEntryBlock);
    g->addNode(&dummyICFGNode);

    for(auto it = defFreeNodes->begin(), eit = defFreeNodes->end(); it != eit; ++it){
        ICFGNode *n = *it;
        worklist.insert(worklist.begin(), {n, &dummyICFGNode}); // push front
    }

    int i = 0;

    while (!worklist.empty()) {
        outs() << "--------------------\n";
        outs() << "DEBUG i=" << i << "\n";

        for(int j = 0; j < (int)worklist.size(); j++){
            ICFGNode *c;
            ICFGNode *f;
            std::tie(c, f) = worklist.at(j);
            outs() << "(c:" << c->getId() << ", f:" << f->getId() << ") ";
        }
        outs() << " -- size: " << worklist.size() << "\n";

        ICFGNode *current;
        ICFGNode *from;
        std::tie(current, from) = worklist.front();
        worklist.erase(worklist.begin());

        outs() << "Erase front: (c:" << current->getId() << ", f:" << from->getId() << ")\n";
        outs() << "Compare all def/alloca with current ^^^\n";

        for(ICFGNode *a : *defFreeNodes){
            if(a->getId() == current->getId()){
                g->addNode(current);
                g->addEdge(from, current);
                from = current;
                outs() << "Found DEF:\n";
                outs() << "[-] def(a):" << a->getId() << ",  current:" << current->getId() << "\n";
            }
        }

        for(ICFGNode *a : *allocNodes){
            if(a->getId() == current->getId()){
                g->addNode(current);
                g->addEdge(from, current);
                from = current;
                outs() << "Found ALLOCA:\n";
                outs() << "[-] alloca(a):" << a->getId() << ", current:" << current->getId() << "\n";
            }
        }

        //visited.insert(current);
        outs() << "Visited: current=(" << current->getId() << ") no from here\n";
        outs() << "Handle nexts: out adges from current\n";

        for (ICFGNode::const_iterator it = current->OutEdgeBegin(), eit = current->OutEdgeEnd(); it != eit; ++it){
            ICFGEdge* edge = *it;
            ICFGNode* succNode = edge->getDstNode();
            outs() << "Next node: " << succNode->getId() << ", should be added to worklist\n";
            if (visited.find(succNode) == visited.end()){
                visited.insert(succNode);
                worklist.insert(worklist.begin(), {succNode, from}); // push front
                outs() << "Added to worklist\n";
            }else{
                outs() << "Not added, already visited!\n";
            }
            
        }

        for(int j = 0; j < (int)worklist.size(); j++){
            ICFGNode *c;
            ICFGNode *f;
            std::tie(c, f) = worklist.at(j);
            outs() << "(c:" << c->getId() << ", f:" << f->getId() << ") ";
        }
        outs() << " -- size: " << worklist.size() << "\n";

        i++;
    }
}

bool existPathWithoutForbiddenNodes(ICFGNode* nodeA, const ICFGNode* nodeB, set<u32_t> *forbiddenNodesId) {
    if (nodeA->getId() == nodeB->getId()) {
        return true;
    }

    std::vector<ICFGNode *> visited;
    std::queue<ICFGNode *> worklist;
    worklist.push(nodeA);

    while (!worklist.empty()) {
        ICFGNode *current = worklist.front();
        worklist.pop();

        if (std::find(visited.begin(), visited.end(), current) == visited.end()) {
            visited.emplace_back(current);

            for(ICFGEdge *outEdge : current->getOutEdges()){
                ICFGNode* succNode = outEdge->getDstNode();
                if (std::find(forbiddenNodesId->begin(), forbiddenNodesId->end(), succNode->getId()) == forbiddenNodesId->end() &&
                    std::find(visited.begin(), visited.end(), succNode) == visited.end()) {

                    if (succNode->getId() == nodeB->getId()) {
                        return true;
                    } else {
                        worklist.push(succNode);
                    }
                }
            }
        }
    }

    return false;
}

void createAllocFreeFlowGraph(Graph<ICFGNode *> *g, ICFG *icfg, std::vector<tuple<ICFGNode *, PAGNode *, const CallICFGNode *, set<PAGNode *> *>> *freeList,
                                                                std::vector<tuple<ICFGNode *, PAGNode *, const CallICFGNode *, set<PAGNode *> *>> *allocList){

    set<u32_t> forbiddenNodesId;
    vector<ICFGNode *> allocAndFreeAndDefNodes;

    outs() << "inside createAllocFreeFlowGraph\n";

    for(auto it = freeList->begin(), eit = freeList->end(); it != eit; it++){
        ICFGNode *fICFG;
        set<PAGNode *> *fDefs;
        tie(fICFG, std::ignore, std::ignore, fDefs) = *it;

        allocAndFreeAndDefNodes.emplace_back(fICFG);
        for(PAGNode *d : *fDefs){
            ICFGNode *n = getICFGNodeFromPAGNode(icfg, d);
            if(n != NULL)
                allocAndFreeAndDefNodes.emplace_back(n);
        }
    }
    outs() << "OKOKOK 1\n";

    for(auto it = allocList->begin(), eit = allocList->end(); it != eit; it++){
        ICFGNode *fICFG;
        set<PAGNode *> *fDefs;
        tie(fICFG, std::ignore, std::ignore, fDefs) = *it;

        allocAndFreeAndDefNodes.emplace_back(fICFG);
        for(PAGNode *d : *fDefs){
            ICFGNode *n = getICFGNodeFromPAGNode(icfg, d);
            if(n != NULL)
                allocAndFreeAndDefNodes.emplace_back(n);
        }
    }
    outs() << "OKOKOK 2\n";

    for(ICFGNode *nodeA : allocAndFreeAndDefNodes){
        for(ICFGNode *nodeB : allocAndFreeAndDefNodes){

            outs() << "node A: " << nodeA->getId() << ", node B:" << nodeB->getId() << "\n";
            
            forbiddenNodesId.clear();
            for(ICFGNode *a : allocAndFreeAndDefNodes){
                forbiddenNodesId.insert(a->getId());
            }
            outs() << "forbiddenid OK\n";

            for (auto it = forbiddenNodesId.begin(); it != forbiddenNodesId.end();){
                if (*it == nodeB->getId()){
                    it = forbiddenNodesId.erase(it);
                }else{
                    ++it;
                }
            }
            outs() << "eraseB OK\n";

            for (auto it = forbiddenNodesId.begin(); it != forbiddenNodesId.end();){
                if (*it == nodeA->getId())
                    it = forbiddenNodesId.erase(it);
                else
                    ++it;
            }
            outs() << "eraseA OK\n";

            if(existPathWithoutForbiddenNodes(nodeA, nodeB, &forbiddenNodesId)){
                g->addNode(nodeA);
                g->addNode(nodeB);
                g->addEdge(nodeA, nodeB);
            }

        }
    }


}

void initMFFlow(vector<MFPath *> *flow, std::vector<tuple<ICFGNode *, PAGNode *, const CallICFGNode *, set<PAGNode *> *>> *allocList,
    std::vector<tuple<ICFGNode *, PAGNode *, const CallICFGNode *, set<PAGNode *> *>> *freeList){

    for(auto it = freeList->begin(), eit = freeList->end(); it != eit; it++){
        ICFGNode *freeICFG;
        PAGNode *freePAG;
        const CallICFGNode *freeCallBlockNode;
        set<PAGNode *> *freeDefs;
        tie(freeICFG, freePAG, freeCallBlockNode, freeDefs) = *it;

        for(auto it = allocList->begin(), eit = allocList->end(); it != eit; it++){
            ICFGNode *allocICFG;
            PAGNode *allocPAG;
            const CallICFGNode *allocCallBlockNode;
            set<PAGNode *> *allocDefs;
            tie(allocICFG, allocPAG, allocCallBlockNode, allocDefs) = *it;

            for(PAGNode *freeDef : *freeDefs){
                for(PAGNode *allocDef : *allocDefs){
                    if(freeDef->getId() == allocDef->getId()){

                        outs() << "MATCH: " << freeICFG->toString() << " --> \n";
                        outs() << "--> " << allocICFG->toString() << "\n";
                        MFPath *path = new MFPath(freeICFG, freePAG, freeCallBlockNode, freeDefs,
                                                    allocICFG, allocPAG, allocCallBlockNode, allocDefs);

                        flow->emplace_back(path);
                    }
                }
            }
        }
    }
}

void resolveMFMFlow(Graph<ICFGNode *> *g, vector<MFPath *> *flow, std::vector<tuple<ICFGNode *, PAGNode *, const CallICFGNode *, set<PAGNode *> *>> *allocList){

    set<u32_t> forbiddenNodesId;
    vector<ICFGNode *> allocAndFreeAndDefNodes;

    for(MFPath *path : *flow){
        ICFGNode *free = path->getICFGFree();
        
        for(auto it = allocList->begin(), eit = allocList->end(); it != eit; it++){
            ICFGNode *allocICFG;
            PAGNode *allocPAG;
            const CallICFGNode *allocCallBlockNode;
            set<PAGNode *> *allocDefs;
            tie(allocICFG, allocPAG, allocCallBlockNode, allocDefs) = *it;

            if(g->hasPath(free, allocICFG)){
                path->addLastAlloc(allocICFG, allocPAG, allocCallBlockNode, allocDefs);
                outs() << "[*] MFM new flow: " << path->getICFGAlloc()->getId() << " --> " << free->getId() << " --> " << allocICFG->getId() << "\n";
            }
        }
    }


}

 uint64_t getSizeInBytes(Type *Ty) {
    DataLayout DL =  DataLayout(LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule());
    return DL.getTypeAllocSize(Ty);
  }

  uint64_t getSizeOfStructInBytes(StructType *StructTy) {
    uint64_t totalSize = 0;
    for (unsigned i = 0; i < StructTy->getNumElements(); ++i) {
      totalSize += getSizeInBytes(StructTy->getElementType(i));
    }
    return totalSize;
  }


  bool isSizeConstant(Value *sizeArg) {
    if (auto *constantSize = dyn_cast<ConstantInt>(sizeArg)) {
      outs() << "Found statically known size: " << constantSize->getZExtValue() << " bytes\n";
      return true;
    } else {
        if (Instruction *sizeExpr = dyn_cast<Instruction>(sizeArg)) {
            if (sizeExpr->getOpcode() == Instruction::GetElementPtr) {
                // Handle dynamically computed size
                outs() << "Found dynamically computed size: ";
                sizeArg->print(errs());
                outs() << "\n";

                // Analyze the type of the operand in sizeof expression
                if (auto *pointerOperand = dyn_cast<PointerType>(sizeExpr->getOperand(0)->getType())) {
                    Type *elementType = pointerOperand->getElementType();
                    if (auto *arrayType = dyn_cast<ArrayType>(elementType)) {
                        outs() << "Array type with size: " << getSizeInBytes(arrayType) << " bytes\n";
                    } else if (auto *structType = dyn_cast<StructType>(elementType)) {
                        outs() << "Struct type with size: " << getSizeOfStructInBytes(structType) << " bytes\n";
                    } else {
                        outs() << "Unknown type\n";
                    }
                }
            }
        } 
    }

    return false;
  }

void traverseAndWriteToCSV(ICFGNode *start, const std::string& filename) {
    std::unordered_set<ICFGNode *> visited;
    std::queue<ICFGNode *> worklist;
    std::ofstream outputFile(filename);

    if (!outputFile.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    worklist.push(start);
    visited.insert(start);

    // Write header
    outputFile << "instruction;opcode;instr_kind;function;type;call_to;call_params\n";

    while (!worklist.empty()) {
        ICFGNode *current = worklist.front();
        worklist.pop();

        for(const SVFStmt *stmt : current->getSVFStmts()){
            std::string name = stmt->toString();
            replace(name, "\n", "\\n");
            replace(name, "{", "\\{");
            replace(name, "}", "\\}");
            replace(name, "<", "\\<");

            outputFile << name;
            outputFile << ";"; 
            outputFile << stmt->getInst()->getOpcodeName();
            outputFile << ";"; 
            outputFile << stmt->getEdgeKind();
            outputFile << ";"; 

            if(current->getSVFStmts().size() == 0)
            outputFile << ";;;"; 

            outputFile << current->getFun()->getName();
            outputFile << ";"; 

            if(current->getNodeKind() == SVF::ICFGNode::FunEntryBlock)
                outputFile << "Function entry";
            else if(current->getNodeKind() == SVF::ICFGNode::FunExitBlock)
                outputFile << "Function exit";
            else if(current->getNodeKind() == SVF::ICFGNode::FunCallBlock)
                outputFile << "Call";
            else if(current->getNodeKind() == SVF::ICFGNode::FunRetBlock)
                outputFile << "Function Ret";
            else if(current->getNodeKind() == SVF::ICFGNode::GlobalBlock)
                outputFile << "Global";
            else if(current->getNodeKind() == SVF::ICFGNode::IntraBlock)
                outputFile << "Intra function";
            outputFile << ";"; 
            
            if(current->getNodeKind() == SVF::ICFGNode::FunCallBlock){
                CallICFGNode *callNode = (CallICFGNode *) current;
                CallBase *call = (CallBase*) callNode->getCallSite();

                if(call->getCalledFunction() != NULL){
                    outputFile << call->getCalledFunction()->getName().str();
                    outputFile << ";"; 
                    outputFile << callNode->getActualParms().size();
                    outputFile << ";";  
                }else{
                    outputFile << ";;"; 
                }
            }else{
                outputFile << ";;"; 
            }

            outputFile << "\n";
        }

        for (ICFGNode::const_iterator it = current->OutEdgeBegin(), eit = current->OutEdgeEnd(); it != eit; ++it) {
            ICFGEdge *edge = *it;
            ICFGNode *succNode = edge->getDstNode();
            if (visited.find(succNode) == visited.end()) {
                visited.insert(succNode);
                worklist.push(succNode);
            }
        }
    }

    outputFile.close();
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

    outs() << "PAG created\n";

    string ptaName = "";
    string argNameDDA = "--pta-dda";
    string argNameFS = "--pta-fs";
    string argNameAnder = "--pta-ander";

    if(argc >= 2){

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
            pta->initialize();
        }else if (argNameFS.find(string(argv[2])) != std::string::npos){
            ptaName = "fs";
            FlowSensitive *fs = FlowSensitive::createFSWPA(pag);
            pta = fs;
            pta->initialize();
        }else{
            outs() << "No PTA set, exit" << "\n";
            exit(1);
        }
        pta->analyze();
    }

    //Zbouncer *zbouncer = new Zbouncer();
    Module *m = LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule();
    //SVFValue* val2 = LLVMModuleSet::getLLVMModuleSet()->getSVFValue(v2);

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

    PAG::CallSiteSet callSite = pag->getCallSiteSet();
    std::vector<PAGNode *>* getDefiningNodes(PAGNode* pgn);

    std::set<SVF::PAGNode *> allStoreFromMalloc;
    std::set<SVF::PAGNode *> allDefFromMalloc;

    int instructionCounter = 0;
    for (Function &F : *m){
        for (BasicBlock &bb : F.getBasicBlockList()){
            instructionCounter += bb.size();
        }
    }

    string ll_name = string(argv[1]);
    std::string data = ll_name + "," + std::to_string(instructionCounter) + ",";

    outs() << "***** NEW *****\n";

    std::vector<MFPath *> MFFlow;
    std::vector<tuple<ICFGNode *, PAGNode *, const CallICFGNode *, set<PAGNode *> *>> freeList;
    std::vector<tuple<ICFGNode *, PAGNode *, const CallICFGNode *, set<PAGNode *> *>> allocList;

    ICFGNode *mainNode;

    for(PAG::CallSiteSet::iterator it = callSite.begin(), eit = callSite.end(); it != eit; ++it){
        const CallICFGNode *callBlockNode = *it;
        CallBase *call = (CallBase*) callBlockNode->getCallSite();

        //outs() << "[*] call\n";

        if(call->getCalledFunction() == NULL){
            //outs() << "[-] Dropping " << call->getName().str() << " because getCalledFunction is nullptr" << "\n";
            continue;
        }

        if (std::find(freeFunctions.begin(), freeFunctions.end(), call->getCalledFunction()->getName()) != freeFunctions.end()){
            outs() << "Call " << call->getCalledFunction()->getName().str() << "\n";

            for(const SVFVar *p : callBlockNode->getActualParms()){
                SVFVar *v = pag->getGNode(pag->getBaseValVar(p->getId()));
                ICFGNode *callICFGNode = icfg->getICFGNode(callBlockNode->getId());

                outs() << "Free node: \n";
                outs() << "[+] " << callICFGNode->toString() << "\n";
                outs() << "-------------------\n";

                outs() << "Backward DEF nodes:\n";
                std::vector<SVF::PAGNode *> *defs = getDefNodes(v);
                set<PAGNode *> *fDef = new set<PAGNode *>();
                for(auto it = defs->begin(), eit = defs->end(); it != eit; it++){
                    PAGNode *d = *it;
                    fDef->insert(d);

                    ICFGNode *nodeICFG = getICFGNodeFromPAGNode(icfg, d);
                    if(nodeICFG != NULL)
                        outs() << "[-] " << nodeICFG->toString() << "\n";
                }
                tuple<ICFGNode *, PAGNode *, const CallICFGNode *, set<PAGNode *> *> f = {callICFGNode, v, callBlockNode, fDef};
                freeList.emplace_back(f);
            }
            outs() << "-----------------------------------------------------------\n";
        }

        if (std::find(allocFunctions.begin(), allocFunctions.end(), call->getCalledFunction()->getName()) != allocFunctions.end()){
            outs() << "Call " << call->getCalledFunction()->getName().str() << "\n";

            const SVF::RetICFGNode *ret = callBlockNode->getRetICFGNode();
            const SVFVar *r = ret->getActualRet();
            SVFVar *v = pag->getGNode(pag->getBaseValVar(r->getId()));            
            ICFGNode *callICFGNode = icfg->getICFGNode(callBlockNode->getId());

            outs() << "Alloc node: \n";
            outs() << "[+] " << callICFGNode->toString() << "\n";
            outs() << "-------------------\n";

            outs() << "Backward DEF nodes:\n";
            std::vector<SVF::PAGNode *> *defs = getDefNodes(v);
            set<PAGNode *> *mDef = new set<PAGNode *>();
            for(auto it = defs->begin(), eit = defs->end(); it != eit; it++){
                PAGNode *d = *it;
                mDef->insert(d);

                ICFGNode *nodeICFG = getICFGNodeFromPAGNode(icfg, d);
                if(nodeICFG != NULL)
                    outs() << "[-] " << nodeICFG->toString() << "\n";
            }

            tuple<ICFGNode *, PAGNode *, const CallICFGNode *, set<PAGNode *> *> m = {callICFGNode, v, callBlockNode, mDef};
            allocList.emplace_back(m);
            outs() << "-----------------------------------------------------------\n";
        }

        if (callBlockNode->getCaller()->getName().find("main") != std::string::npos) 
            mainNode = icfg->getFunEntryICFGNode(callBlockNode->getCaller());
    }

    outs() << "Finding linked vars\n";

    Graph<ICFGNode *> g;
    createAllocFreeFlowGraph(&g, icfg, &freeList, &allocList);
    g.printICFGGraph();
    g.printICFGDotGraph(ll_name + ".carved.dot");

    outs() << "-----------------------------------------------------------\n";

    initMFFlow(&MFFlow, &freeList, &allocList);
    for(MFPath *p : MFFlow)
        p->printMFPathFull();

    outs() << "Total MF match: " << MFFlow.size() << "\n";

    outs() << "-----------------------------------------------------------\n";

    resolveMFMFlow(&g, &MFFlow, &allocList); // check reacability on graph
    outs() << "Total MFM complete: " << MFFlow.size() << "\n";
    for(MFPath *p : MFFlow)
        p->printMFPathFull();

    int totalPathCounter = 0;
    for(MFPath *p : MFFlow)
        totalPathCounter += p->getMFMPathCounter();
    outs() << "Total MFM counter: " << totalPathCounter << "\n";

    outs() << "-----------------------------------------------------------\n";

    outs() << "Total fun: "<< m->getFunctionList().size() << "\n";
    outs() << "Malloc counter: " << allocList.size() << "\n";
    outs() << "Free counter: " << freeList.size() << "\n";

    traverseAndWriteToCSV(mainNode, ll_name + ".csv");

    pag->dump(ll_name + ".pag");
    svfg->dump(ll_name + "." + ptaName + ".svfg");
    icfg->dump(ll_name + ".icfg");

    // clean up memory
    delete vfg;
    delete svfg;
    SVFIR::releaseSVFIR();

    LLVMModuleSet::getLLVMModuleSet()->dumpModulesToFile(".svf.bc");
    
    //std::string str;
    //raw_string_ostream rawstr(str);

	return 0;
}
