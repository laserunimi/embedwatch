#include "SVF-FE/LLVMUtil.h"
#include "Graphs/SVFG.h"
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
#include <assert.h>
#include "visits.h"

using namespace llvm;
using namespace std;
using namespace SVF;

enum class TargetType {store_t, alloca_t};
unsigned int evaluateSizeDL(Type *t, Module *M, TargetType tt);
Type *evaluateBaseType(Type *t);

enum class TraceEntityType {
	trace_entity,
	def,
	use,
	adef,
	gdef,
	ddef,
	sdef,
	suse,
	luse,
	in
};

class TraceEntity {
	private:
		static int idCount;
		int entityId;
	public:

		TraceEntity() {
			entityId = idCount++;
		}

		virtual ~TraceEntity(){}

		int getId() {
			return this->entityId;
		}

		virtual string traceLine(int father){
			stringstream buffer;
			buffer << this->getId() << ";TRACE.ENTITY;" << father << "\n";
			return buffer.str();
		}

		virtual TraceEntityType getEntityType() {
			return  TraceEntityType::trace_entity;
		}

		friend SVF::raw_fd_ostream& operator<<(SVF::raw_fd_ostream& _stream, const TraceEntity& mc) {
			TraceEntity &te = const_cast<TraceEntity&>(mc);
			_stream << "[" << te.getId() << ".TRACE]";
			return _stream;
		}
};

class Def : public TraceEntity {
	public:
		/*postcondition: 
		 * 	- return == -1 => Size cannot be defined at static time
		 * 	- return >= 0  => Size of the definiztion
		 */
		virtual int getBytes() = 0; 
		virtual Type* getType() = 0;

		
		virtual bool isDominator() {
			return false;
		}

		virtual TraceEntityType getEntityType() {
			return  TraceEntityType::def;
		}

		virtual string traceLine(int father){
			stringstream buffer;
			buffer << this->getId() << ";DEF;" << father << "\n";
			return buffer.str();
		}

		friend SVF::raw_fd_ostream & operator<<(SVF::raw_fd_ostream & _stream, Def const & mc) {
			Def &def = const_cast<Def&>(mc);
			_stream << "[" << def.getId() << ".Def] ";
			

			return _stream;
		}
};

class SDef : public Def {
	private:
		PAGNode *defNode;
		vector<int> *field_vec;
		int offset;
		Type *field_type;
		bool dominator;

	public:	
		SDef(PAGNode *n,Type *t, vector<int> *field_list, int offset) {
			this->defNode = n;
			this->field_vec = new vector<int>(*field_list);
			this->offset = offset;
			this->field_type = t;
			this->dominator = isDominatorNode(n);
		}

		int getOffset() {
			return this->offset;
		}

		int getBytes() {
			return evaluateSizeDL(this->getType(),\
				LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule(),TargetType::alloca_t);
		}

		Type* getType() {
			return this->field_type;
		}

		PAGNode *getNode(){
			return this->defNode;
		}

		bool isDominator() {
			return this->dominator;
		}
	
		virtual TraceEntityType getEntityType() {
			return  TraceEntityType::sdef;
		}

		virtual string traceLine(int father){
			stringstream buffer;
			std::string out = this->getNode()->toString();
            std::replace( out.begin(), out.end(), '\n', ' '); 
			
			string s;
			s.append("[");
			for(auto it = this->field_vec->begin(), eit = this->field_vec->end(); it != eit ; it++) {
				s.append(to_string(*it));
				if(it + 1  != eit)
					s.append(",");
			}
			s.append("]");

			buffer << this->getId() << ";SDEF;" << father << (this->isDominator() ? ";DOMINATOR;" : ";BASE;") << this->getBytes() << ";" \
			<< this->offset << ";" << s << ";" << out << "\n";
			return buffer.str();
		}

		friend SVF::raw_fd_ostream & operator<<(SVF::raw_fd_ostream & _stream, SDef const & mc) {
			SDef &adef = const_cast<SDef&>(mc);
			_stream << "[" << adef.getId() << ".SDEF" << (adef.isDominator() ? " - DOMINATOR " : "") <<"] (PAGNode: " << mc.defNode->getId() << ") " << mc.defNode->toString();
        	return _stream;
		}

};

class ADef : public Def {
	private:
		PAGNode *defNode;
		AllocaInst *defAlloca;
		bool dominator;
	public:
		ADef(PAGNode *n,AllocaInst *a) {
			this->defNode = n;
			this->defAlloca = a;
			this->dominator = isDominatorNode(n);
		}

		AllocaInst *getDefAlloca() {
			return	this->defAlloca;
		}

		int getBytes() {
			return evaluateSizeDL(this->getType(),\
				LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule(),TargetType::alloca_t);
		}

		Type* getType() {
			return this->getDefAlloca()->getAllocatedType();
		}	

		PAGNode *getNode(){
			return this->defNode;
		}

		bool isDominator() {
			return this->dominator;
		}

		virtual TraceEntityType getEntityType() {
			return  TraceEntityType::adef;
		}

		virtual string traceLine(int father){
			stringstream buffer;
			std::string out = this->getNode()->toString();
            std::replace( out.begin(), out.end(), '\n', ' '); 
			buffer << this->getId() << ";ADEF;" << father << (this->isDominator() ? ";DOMINATOR;" : ";BASE;") << this->getBytes() << ";" << out << "\n";
			return buffer.str();
		}

		friend SVF::raw_fd_ostream & operator<<(SVF::raw_fd_ostream & _stream, ADef const & mc) {
			ADef &adef = const_cast<ADef&>(mc);
			_stream << "[" << adef.getId() << ".ADEF" << (adef.isDominator() ? " - DOMINATOR " : "") <<"] (PAGNode: " << mc.defNode->getId() << ") " << mc.defNode->toString();
        	return _stream;
		}
};

class GDef : public Def {
	private:
		PAGNode *defNode;
		GlobalValue *defGlobal;
		bool dominator;
	
	public:
		GDef(PAGNode *n,GlobalValue *g) {
			this->defNode = n;
			this->defGlobal = g;
			this->dominator = isDominatorNode(n);
		}

		GlobalValue *getDefValue() {
			return	this->defGlobal;
		}

		int getBytes() {
			return evaluateSizeDL(this->getType(),\
				LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule(),TargetType::alloca_t);
		}

		Type* getType() {
			return this->getDefValue()->getValueType();
		}	

		PAGNode *getNode(){
			return this->defNode;
		}

		bool isDominator() {
			return this->dominator;
		}

		virtual TraceEntityType getEntityType() {
			return  TraceEntityType::gdef;
		}

		virtual string traceLine(int father){
			stringstream buffer;
			std::string out = this->getNode()->toString();
            std::replace( out.begin(), out.end(), '\n', ' ');
			
			buffer << this->getId() << ";GDEF;" << father << (this->isDominator() ? ";DOMINATOR;" : ";BASE;") << this->getBytes() << ";" << out << "\n";
			return buffer.str();
		}

		friend SVF::raw_fd_ostream & operator<<(SVF::raw_fd_ostream & _stream, GDef const & mc) {
			GDef &gdef = const_cast<GDef&>(mc);
			_stream << "[" << gdef.getId() << ".GDEF" <<  (gdef.isDominator() ? " - DOMINATOR " : "") << "] (PAGNode: " << mc.defNode->getId() << ") " << mc.defNode->toString();
        	return _stream;
		}
};

class DDef : public Def {
	private:
		PAGNode *defNode;
		CallInst *call;

		Def *targetDef;
	
	public:
		DDef(PAGNode *n,CallInst *c, Def *target) {
			this->defNode = n;
			this->call = c;
			this->targetDef = target; 
		}

		CallInst *getCall() {
			return	this->call;
		}

		PAGNode *getCallNode(){
			return this->defNode;
		}

		Def *getTarget() {
			return this->targetDef;
		}

		int getBytes() {
			return -1;
		}

		Type* getType() {
			return this->targetDef->getType();
		}

		virtual TraceEntityType getEntityType() {
			return  TraceEntityType::ddef;
		}

		virtual string traceLine(int father){
			stringstream buffer;
			std::string out = this->getCallNode()->toString();
            std::replace( out.begin(), out.end(), '\n', ' '); 
			buffer << this->getId() << ";DDEF;" << father << ";BASE;" << this->getBytes() << ";" << out << "\n";
			return buffer.str();
		}

		friend SVF::raw_fd_ostream & operator<<(SVF::raw_fd_ostream & _stream, DDef const & mc) {
			DDef &ddef = const_cast<DDef&>(mc);
			_stream << "[" << ddef.getId() << ".DDef] " << "\n" <<
				" call: " << *mc.call;
			return _stream;
   		}			
};

class ZUse : public TraceEntity {
	private:
		std::vector<Def *> targetDefs;
    public:
        void addTargetObject(Def *d){
            targetDefs.emplace_back(d);
        }

        int countTargetObject(){ return targetDefs.size(); }

        std::vector<Def *>::iterator targetObjBegin(){ return targetDefs.begin(); }
        std::vector<Def *>::iterator targetObjEnd(){ return targetDefs.end(); }

		Type *getValueType(Value *v){

			#ifdef DEBUG
			outs() << "CallTargetObject: ";
			//callTargetObjects
			outs() << "\n";
			#endif

			if (llvm::GetElementPtrInst *gep = dyn_cast<llvm::GetElementPtrInst>(v)) {
				#ifdef DEBUG
				outs() << "GEP, getSourceElementType(): ";
				gep->getSourceElementType()->print(outs());
				outs() << "\n";
				#endif

				Type *baseType = evaluateBaseType(gep->getSourceElementType());
				#ifdef DEBUG
				outs() << "BaseType: ";
				baseType->print(outs());
				outs() << "\n";
				#endif

				return baseType;
				
			}else{
				if (llvm::AllocaInst *a = dyn_cast<llvm::AllocaInst>(v)) {
					Type *baseType = evaluateBaseType(a->getAllocatedType());
					#ifdef DEBUG
					outs() << "BaseType: ";
					baseType->print(outs());
					outs() << "\n";
					#endif

					return baseType;

				}else{
					outs() << "Strange situation, the storeParam is not GEP nor Alloca." << "\n";
					errs() << "Cannot fill the size of memory object: yield!" << "\n";
					return v->getType();
				}
			}
    	}

		virtual Value* getTarget() = 0;

		virtual TraceEntityType getEntityType() {
			return  TraceEntityType::use;
		}
};

class SUse : public ZUse {
    private:
	    SVFGNode *use;
    	StoreInst *storeUse;
   		Value *storeParamValue;
    	std::vector<Def *> targetObjects;

    public:
		SUse(SVFGNode *n, StoreInst *i){
			use = n;
			storeUse = i;
		}

		SVFGNode *getUseVFGNode(){ return use; }

		StoreInst *getStoreUse(){ return storeUse; }

		Value *getStoreParamSVFGNode(){ return storeParamValue; }
		void setStoreParamSVFGNode(Value *v){ storeParamValue = v; }

		Type *getStoreTargetType(){

			Value *ptrStoreOperand = storeUse->getPointerOperand();

			return ZUse::getValueType(ptrStoreOperand);
		
		}

		unsigned int getTotalBytesOfBaseType(){
			return evaluateSizeDL(getStoreTargetType(),LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule(),TargetType::store_t);// / 8;
		}

		virtual Value* getTarget() {
			return this->storeParamValue;
		}

		virtual TraceEntityType getEntityType() {
			return  TraceEntityType::suse;
		}

		virtual string traceLine(int father){
			stringstream buffer;
			std::string out = this->getUseVFGNode()->toString();
            std::replace( out.begin(), out.end(), '\n', ' '); 
			buffer << this->getId() << ";SUSE;" << father << ";" << string(this->getStoreUse()->getFunction()->getName()) << ";" << out << "\n";
			return buffer.str();
		}

		friend SVF::raw_fd_ostream & operator<<(SVF::raw_fd_ostream & _stream, SUse const & mc) {
			SUse &suse = const_cast<SUse&>(mc);
			_stream << "[" << suse.getId() << ".STOREUSE] (VFGNode: " << mc.use->toString() << ") " << "\n" <<
				" storeParam:" << *mc.storeParamValue << "\n" << "Count target object: " << mc.targetObjects.size();
			return _stream;
    	}

};

class LUse : public ZUse {
	private:
		CallICFGNode *callBlockNode;
		CallBase *call;
		Value *callTargetParam;
		std::vector<Def *> callTargetObjects; // must be set by the analyzer

    public:
    LUse(CallICFGNode *n){
        callBlockNode = n;
        call = (CallBase *) callBlockNode->getCallSite();
    }

    CallBase *getCall(){ return call; }

    CallICFGNode *getCallBlockNode(){ return callBlockNode; }

    void setCallTargetParam(Value *i){ callTargetParam = i; }

    Type *getCallTargetType(){
        return ZUse::getValueType(callTargetParam);
    }

    unsigned int getTotalBytesOfBaseType(){
        return evaluateSizeDL(getCallTargetType(),LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule(),TargetType::store_t);// / 8;
    }

	virtual Value* getTarget() {
		return this->callTargetParam;
	}

	virtual TraceEntityType getEntityType() {
		return  TraceEntityType::luse;
	}

	virtual string traceLine(int father){
			stringstream buffer;
			std::string out = this->getCallBlockNode()->toString();
            std::replace( out.begin(), out.end(), '\n', ' '); 
			buffer << this->getId() << ";LUSE;" << father << ";" << string(this->getCall()->getFunction()->getName()) << ";" << out << "\n";
			return buffer.str();
	}

	friend SVF::raw_fd_ostream & operator<<(SVF::raw_fd_ostream & _stream, LUse const & mc) {
		LUse &luse = const_cast<LUse&>(mc);
        _stream << "[" << luse.getId() << ".LIBUSE] " << " Call:" << *mc.call;
        return _stream;
    }
};

class In: public TraceEntity {
    private:
        PAGNode *targetPAGNode;
        CallICFGNode *blacklistedCall;
        std::vector<TraceEntity *> subObjects;
    public:
        In(PAGNode *n){
            targetPAGNode = n;
        }

        PAGNode *getInPAGNode(){ return targetPAGNode; }

        CallICFGNode *getBlacklistedCall(){ return blacklistedCall; }
        void setBlacklistedCall(CallICFGNode *node){ blacklistedCall = node; }

        CallBase *getCallInputFunction(){ return (CallBase *)blacklistedCall->getCallSite(); }

        void entityInsert(TraceEntity *te){ subObjects.emplace_back(te); }
        std::vector<TraceEntity *>::iterator subObjectsBegin(){ return subObjects.begin(); }
        std::vector<TraceEntity *>::iterator subObjectsEnd(){ return subObjects.end(); }

		virtual TraceEntityType getEntityType() {
			return  TraceEntityType::in;
		}


		virtual string traceLine(int father){
			stringstream buffer;
			std::string out = this->getBlacklistedCall()->toString(); 
        	std::replace( out.begin(), out.end(), '\n', ' ');
			buffer << this->getId() << ";IN;" << father << ";" << out << "\n";
			return buffer.str();
		}


		friend SVF::raw_fd_ostream& operator<<(SVF::raw_fd_ostream& _stream, const In& mc) {
			CallBase *call = (CallBase*) mc.blacklistedCall->getCallSite();
			In &in = const_cast<In&>(mc);
			_stream << "[" << in.getId() << ".IN] (PAGNode: " << in.targetPAGNode->getId()
				<< ") " << call->getCalledFunction()->getName().str();
			return _stream;
		}
};

class DefBuilder {
	private:
		const std::vector<std::string> dynamicFunctions = {"malloc", "calloc"};

		PAG *pag;

		Def *buildADef(PAGNode *pn, AllocaInst *alloca){
			return new ADef(pn,alloca);
		}

		Def *buildGlobalDef(PAGNode *pn, GlobalValue *glob){
			return new GDef(pn,glob);
		}

		Def *buildDynDef(PAGNode *pn, CallInst *call){
			Def *target = buildDef(findTargetOfCall(pn,pag));

			return new DDef(pn,call,target);
		}

	public:
		DefBuilder(PAG *pag){
			this->pag = pag;
		}
		Def *buildDef(PAGNode *defNode) {
			Value *v = const_cast<Value *>(defNode->getValue());

			if(AllocaInst *alloca = dyn_cast<AllocaInst>(v)){
				outs() << "[DEFBUILDER] Building ADef\n";
				return buildADef(defNode,alloca);
			}

			if(GlobalValue *gv = dyn_cast<GlobalValue>(v)){
				outs() << "[DEFBUILDER] Building GDef\n";
				return buildGlobalDef(defNode,gv);
			}

			if(CallInst *ci = dyn_cast<CallInst>(v)){
				if(std::find(dynamicFunctions.begin(), dynamicFunctions.end(), ci->getCalledFunction()->getName()) != dynamicFunctions.end()){
					outs() << "[DEFBUILDER] Building DDef\n";
					return buildDynDef(defNode,ci);
				} else {
					outs() << "[ERROR] Definition is not a call to a dynamic function, building definition of target\n";
					return nullptr;
				}
			}

			//Should not reach
			assert(0);

			return nullptr;
		}
};

template <typename B>
B* trace_entity_dynamic_cast(TraceEntity*);//Necessary since rtti disabled in svf
void printEntity(TraceEntity *);
void printIn(TraceEntity *);
void printUse(TraceEntity *);
void printDef(TraceEntity*);
bool defNodeCompare(Def *, PAGNode *);
Def *getDefWithID(std::vector<Def*>*,int);
bool isUse(TraceEntity *);
ZUse *asUse(TraceEntity *);
bool isDef(TraceEntity *);
Def *asDef(TraceEntity * );
Instruction *getUseInstruction(ZUse *);

