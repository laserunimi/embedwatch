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
#include "trace.h"

int TraceEntity::idCount = 0;

void printDef(TraceEntity *te){
    if(ADef *def = trace_entity_dynamic_cast<ADef>(te)){
        outs() << *def << "\n";
    }

    if(DDef *def = trace_entity_dynamic_cast<DDef>(te)){
        outs() << *def << "\n";
        outs() << "Target:\n" ;
        printDef(def->getTarget());
    }
    
    if(GDef *def = trace_entity_dynamic_cast<GDef>(te)){
        outs() << *def << "\n";
    }

    if(SDef *def = trace_entity_dynamic_cast<SDef>(te)){
        outs() << *def << "\n";
    }
}


void printIn(TraceEntity *te){
    if(In *in = trace_entity_dynamic_cast<In>(te)){
        outs() << *in << "\n";

        outs() << "--- Children ---\n";
        for(auto it = in->subObjectsBegin(), eit = in->subObjectsEnd(); it != eit; it++)
            printEntity(*it);

        outs() << "----------------\n";

    }
}

void printUse(TraceEntity *te){
    if(LUse *use = trace_entity_dynamic_cast<LUse>(te)){
        outs() << *use << "\n";
        
        outs() << "--- Children ---\n";
        for(auto it = use->targetObjBegin(), eit = use->targetObjEnd(); it != eit; it++)
            printEntity(*it);
        outs() << "----------------\n";
    }

    if(SUse *use = trace_entity_dynamic_cast<SUse>(te)){
        outs() << *use << "\n";

        outs() << "--- Children ---\n";
        for(auto it = use->targetObjBegin(), eit = use->targetObjEnd(); it != eit; it++)
            printEntity(*it);
        outs() << "----------------\n";
    }
}

void printEntity(TraceEntity *te){
    
    printIn(te);

    printUse(te);

    printDef(te);
}

template <typename B>
B* trace_entity_dynamic_cast(TraceEntity* base){
    TraceEntityType t;

    if(is_same<B,Def>::value)
        t = TraceEntityType::def;
        
    if(is_same<B,ZUse>::value)
        t = TraceEntityType::use;

    if(is_same<B,TraceEntity>::value)
        t = TraceEntityType::trace_entity;

    if(is_same<B,ADef>::value)
        t = TraceEntityType::adef;
    
    if(is_same<B,GDef>::value)
        t = TraceEntityType::gdef;
    
    if(is_same<B,DDef>::value)
        t = TraceEntityType::ddef;
    
    if(is_same<B,SDef>::value)
        t = TraceEntityType::sdef;

    if(is_same<B,SUse>::value)
        t = TraceEntityType::suse;
    
    if(is_same<B,LUse>::value)
        t = TraceEntityType::luse;
    
    if(is_same<B,In>::value)
        t = TraceEntityType::in;
    
    if(t == base->getEntityType())
        return static_cast<B*>(base);
    
    return nullptr;
}

unsigned int evaluateSizeDL(Type *t, Module *M, TargetType tt){
    #ifdef DEBUG
    outs() << "Called evaluate size on: ";
    printType(t);
    #endif  

    DataLayout dl =  DataLayout(M);

    unsigned int size;

    switch (tt)
    {
    case TargetType::store_t :
        size = dl.getTypeStoreSize(t);
        break;
    case TargetType::alloca_t :
        size = dl.getTypeAllocSize(t);
        break;
    }

    #ifdef DEBUG
    outs() << "Cost of allocating ";
    t->print(outs());
    outs() << " is " << size << " bytes\n";
    #endif

    return size;

}

Type *evaluateBaseType(Type *t){

    #ifdef DEBUG
    printType(t);
    #endif

    if(ArrayType * innerType = dyn_cast<ArrayType>(t)){
        #ifdef DEBUG
        outs() << "DynCast to ArrayType [" << innerType->getArrayNumElements()
            << " x " << innerType->getArrayElementType()->getPrimitiveSizeInBits() << "]\n";
        #endif
        return innerType;

    }else{
        if(StructType *innerType = dyn_cast<StructType>(t)){
            #ifdef DEBUG
            outs() << "DynCast to StructType\n";
            for (llvm::StructType::element_iterator it = innerType->element_begin(),
                eit = innerType->element_end(); it != eit; ++it){

                Type *tt = *it;
                outs() << "\t";
                tt->print(outs());
                outs() << " ";
            }
            #endif

            return innerType;

        }else{
            if(IntegerType *innerType = dyn_cast<IntegerType>(t)){
                #ifdef DEBUG
                outs() << "DynCast to IntegerType " << innerType->getPrimitiveSizeInBits() << "\n";
                #endif
                return innerType;
            }else{
                if(PointerType *innerType = dyn_cast<PointerType>(t)){

                    #ifdef DEBUG
                    outs() << "DynCast to PointerType\n";
                    #endif
                    if(innerType->isIntegerTy())
                        return evaluateBaseType(innerType);
                    else
                        return evaluateBaseType(innerType->getPointerElementType());

                    
                }else{
                    #ifdef DEBUG
                    outs() << "Type not handled (VoidType, FunctionType, Floating-pointType)\n";
                    #endif
                    return nullptr;
                }
            }
        }
    } 
}

bool defNodeCompare(Def *d, PAGNode *pn){
    if(ADef *def = trace_entity_dynamic_cast<ADef>(d)){
        return pn->getId() == def->getNode()->getId();
    }

    if(GDef *def = trace_entity_dynamic_cast<GDef>(d)){
        return pn->getId() == def->getNode()->getId();
    }

    if(DDef *def = trace_entity_dynamic_cast<DDef>(d)){
        return pn->getId() == def->getCallNode()->getId() || defNodeCompare(def->getTarget(),pn);
    }

    return false;
}

Def *getDefWithID(std::vector<Def*>* deflist,int id){
    for(Def *d : *deflist){
        if(d->getId() == id)
            return d;
    }

    return nullptr;
}

bool isUse(TraceEntity *te){
    return trace_entity_dynamic_cast<LUse>(te) || trace_entity_dynamic_cast<SUse>(te);
}

ZUse *asUse(TraceEntity *te){
    if(isUse(te)){
        if(LUse *u = trace_entity_dynamic_cast<LUse>(te))
            return u;
        
        if(SUse *u = trace_entity_dynamic_cast<SUse>(te))
            return u;
    }
    return nullptr;
}

bool isDef(TraceEntity *te){
    return trace_entity_dynamic_cast<ADef>(te) || \
           trace_entity_dynamic_cast<GDef>(te) || \
           trace_entity_dynamic_cast<DDef>(te) || \
           trace_entity_dynamic_cast<SDef>(te);
}

Def *asDef(TraceEntity* te) {
    if(isDef(te)){
        if(ADef *d = trace_entity_dynamic_cast<ADef>(te))
            return d;
        
        if(DDef *d = trace_entity_dynamic_cast<DDef>(te))
            return d;

        if(GDef *d = trace_entity_dynamic_cast<GDef>(te))
            return d;

        if(SDef *d = trace_entity_dynamic_cast<SDef>(te))
            return d;
    }
    return nullptr;
}

Instruction *getUseInstruction(ZUse *use){
    if(LUse *l = trace_entity_dynamic_cast<LUse>(use)){
        return l->getCall();
    } else if(SUse *s = trace_entity_dynamic_cast<SUse>(use)){
        return s->getStoreUse();
    } 

    outs() << "Use is neither store nor call, clearly an error. Skipping\n";
    return nullptr;
    
}



