/*
 * File: SLP_C.c
 *
 * Description:
 *   Stub for SLP in C. This is where you implement your SLP pass.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* LLVM Header Files */
#include "llvm-c/Core.h"
#include "dominance.h"

/* Header file global to this project */
#include "cfg.h"
#include "loop.h"
#include "worklist.h"
#include "valmap.h"


// dom:  does a dom b?
//
//       a and b can be instructions. This is different
//       from the LLVMDominates API which requires basic
//       blocks.



  int count[7] = {0};

static int dom(LLVMValueRef a, LLVMValueRef b)
{
  if (LLVMGetInstructionParent(a)!=LLVMGetInstructionParent(b)) {
    LLVMValueRef fun = LLVMGetBasicBlockParent(LLVMGetInstructionParent(a));
    // a dom b?
    return LLVMDominates(fun,LLVMGetInstructionParent(a),
			 LLVMGetInstructionParent(b));
  }

  // a and b must be in same block
  // which one comes first?
  LLVMValueRef t = a;
  while(t!=NULL) {
    if (t==b)
      return 1;
    t = LLVMGetNextInstruction(t);
  }
  return 0;
}



static LLVMBuilderRef Builder;

typedef struct VectorPairDef {
  LLVMValueRef pair[2];
  int insertAt0;
  struct VectorPairDef *next;
  struct VectorPairDef *prev;
} VectorPair;

typedef struct  {
  VectorPair *head;
  VectorPair *tail;
  valmap_t    visited;
  valmap_t    sliceA;
  int size;  
  int score;
} VectorList;

static VectorList* VectorList_Create() {
  VectorList *new = (VectorList*) malloc(sizeof(VectorList));
  new->head = NULL;
  new->tail = NULL;
  new->visited = valmap_create();
  new->sliceA = valmap_create();
  new->size=0;
  return new;
}

static void VectorList_Destroy(VectorList *list)
{
  valmap_destroy(list->visited);
  valmap_destroy(list->sliceA);
  VectorPair *head = list->head;
  VectorPair *tmp;
  while(head) {
    tmp = head;
    head=head->next;
    free(tmp);    
  }
  free(list);
}

static void collectIsomorphicInstructions(VectorList* VecList,LLVMValueRef I, LLVMValueRef J);
//
// add a and b to the current chain of vectorizable instructions
//
static VectorPair *VectorList_AppendPair(VectorList *list, LLVMValueRef a, LLVMValueRef b)
{
  VectorPair *new = (VectorPair*) malloc(sizeof(VectorPair));
  new->pair[0] = a;
  new->pair[1] = b;
  new->insertAt0 = 1;
  valmap_insert(list->visited,a,(void*)1);
  valmap_insert(list->visited,b,(void*)1);
  new->next = NULL;
  new->prev = NULL;
  // insert at head
  if (list->head==NULL) {
    list->head = new;
    list->tail = new;
  } else {
    // find right place to insert
    VectorPair *temp = list->head;
    VectorPair *prev = NULL;

    while(temp && dom(temp->pair[0],a)) {
      prev=temp;
      temp=temp->next;   
    }
    if (prev) {
      new->next = temp;
      new->prev = prev;
      prev->next = new;
      if (temp) // if not at end
	temp->prev = new;
      else
	list->tail = new;
    } else {
      list->head->prev = new;
      new->next = list->head;
      list->head = new;
    }
  }  
  list->size++;
  return new;
}

// AssembleVector: Helper function for generating vector code.
//               It figures out how to assemble a vector from two inputs under
//               the assumption that the inputs are either constants or registers.
//               If constants, it builds a constant vector.  If registers,
//               it emits the insertelement instructions.
//  
//               This is only helpful for generating vector code, not for detecting
//               vectorization opportunities

static LLVMValueRef AssembleVector(LLVMValueRef a, LLVMValueRef b)
{
  LLVMTypeRef type = LLVMTypeOf(a);
  LLVMValueRef ret;

  // if they are constants...
  if (LLVMIsAConstant(a) && LLVMIsAConstant(b)) {
    // Build constant vector
    LLVMValueRef vec[2] = {a,b};
    ret = LLVMConstVector(vec,2);        
  }  else {
    // Otherwise, one of them is a register

    LLVMTypeRef vtype = LLVMVectorType(type,2);
    LLVMValueRef c = LLVMConstNull(vtype);
    
    // Insert a into null vector
    ret = LLVMBuildInsertElement(Builder,c,a,
				 LLVMConstInt(LLVMInt32Type(),0,0),"v.ie");

    // Insert b into ret
    ret = LLVMBuildInsertElement(Builder,ret,b,
				 LLVMConstInt(LLVMInt32Type(),1,0),"v.ie");    
  }

  // Return new vector as input for a new vector instruction
  return ret;
}


bool AdjacentStoresOrLoads(LLVMValueRef I, LLVMValueRef J)
{
	LLVMValueRef gep1; 
	LLVMValueRef gep2; 
	int NumOpgep1,NumOpgep2,constgep1,constgep2;

	if( LLVMIsALoadInst(I) && LLVMIsALoadInst(J) ) 
  	{
		gep1 = LLVMGetOperand(I, 0); 
		gep2 = LLVMGetOperand(J, 0);

	}
	if( LLVMIsAStoreInst(I) && LLVMIsAStoreInst(J) ) 
  	{
		gep1 = LLVMGetOperand(I, 1); 
		gep2 = LLVMGetOperand(J, 1);

	}


  if (!LLVMIsAGetElementPtrInst(gep1) || !LLVMIsAGetElementPtrInst(gep2))
    return false;

  if (LLVMGetOperand(gep1, 0) != LLVMGetOperand(gep2, 0))
    return false;

  NumOpgep1 = LLVMGetNumOperands(gep1);
  NumOpgep2 = LLVMGetNumOperands(gep2);


  for (int i=0; i<(NumOpgep1-1); i++)
  {
    if (LLVMGetOperand(gep1, i) != LLVMGetOperand(gep2, i)) 
      return false;
  }

  if (!LLVMIsAConstant(LLVMGetOperand(gep1, NumOpgep1-1)) || !LLVMIsAConstant(LLVMGetOperand(gep2, NumOpgep2-1)))
    return false;

  constgep1 = LLVMConstIntGetZExtValue(LLVMGetOperand(gep1, NumOpgep1-1));
  constgep2 = LLVMConstIntGetZExtValue(LLVMGetOperand(gep2, NumOpgep2-1));
  int offsetdiff = constgep1-constgep2; 
	if(constgep1-constgep2 == 1 || constgep2-constgep1== 1)
		return true;
	else 
		return false;  
  return true;
}





bool Isomorphic(LLVMValueRef I, LLVMValueRef J)
{
  
  if(!LLVMIsAInstruction(I) || !LLVMIsAInstruction(J))
    return false; 
  if(LLVMGetInstructionOpcode(I) != LLVMGetInstructionOpcode(J))
    return false;
  if(LLVMTypeOf(I) != LLVMTypeOf(J))
    return false;
  if(LLVMGetNumOperands(I) != LLVMGetNumOperands(J))
    return false;
  int i;
  int num = LLVMGetNumOperands(I);
  for(i=0;i<num;i++)
    {
	    
      			if(LLVMTypeOf(LLVMGetOperand(I,i)) != LLVMTypeOf(LLVMGetOperand(J,i)))
				return false;
		
    }

  return true;
}


static int shouldVectorize(LLVMValueRef I, LLVMValueRef J){
	
	if(!( (LLVMGetTypeKind(LLVMTypeOf(I)) == LLVMVoidTypeKind) 
	|| (LLVMGetTypeKind(LLVMTypeOf(I)) == LLVMFloatTypeKind)
	|| (LLVMGetTypeKind(LLVMTypeOf(I)) == LLVMPointerTypeKind)
	|| (LLVMGetTypeKind(LLVMTypeOf(I)) == LLVMIntegerTypeKind)
	|| LLVMGetTypeKind(LLVMTypeOf(I)) == LLVMDoubleTypeKind
    	|| LLVMGetTypeKind(LLVMTypeOf(I)) == LLVMX86_FP80TypeKind
    	|| LLVMGetTypeKind(LLVMTypeOf(I)) == LLVMFP128TypeKind
    	|| LLVMGetTypeKind(LLVMTypeOf(I)) == LLVMPPC_FP128TypeKind
	|| LLVMGetTypeKind(LLVMTypeOf(I)) == LLVMHalfTypeKind
	)){
		return  0;
	}
	
	

	if( LLVMGetInstructionParent(I) !=  LLVMGetInstructionParent(J) ){
		return 0;
	}	
	if ( LLVMIsATerminatorInst(I) ) 
		return 0;
	//if I is a volatile load or a volatile store:
	if( LLVMIsALoadInst(I) || LLVMIsAStoreInst(I) ){
		if(LLVMGetVolatile(I))
			return 0;
	}
	if( LLVMIsACallInst(I)	
	|| LLVMIsAPHINode(I) 
    	|| LLVMIsAFCmpInst(I) 
    	|| LLVMIsAExtractValueInst(I)
    	|| LLVMIsAInsertValueInst(I)
    	|| LLVMIsAICmpInst(I)
	|| LLVMGetInstructionOpcode(I) == LLVMAddrSpaceCast 
    	|| LLVMGetInstructionOpcode(I) == LLVMAtomicRMW
    	|| LLVMGetInstructionOpcode(I) == LLVMAtomicCmpXchg
    	|| LLVMGetInstructionOpcode(I) == LLVMInsertElement
    	|| LLVMGetInstructionOpcode(I) == LLVMExtractElement
    	|| LLVMGetInstructionOpcode(I) == LLVMInsertValue
    	|| LLVMGetInstructionOpcode(I) == LLVMExtractValue
    	|| LLVMGetInstructionOpcode(I) == LLVMFence 
    	|| LLVMGetInstructionOpcode(I) == LLVMGetElementPtr
    	|| LLVMGetInstructionOpcode(I) == LLVMBitCast
    	) {
		return 0;
	}
	
	if(LLVMIsAGetElementPtrInst(I) || LLVMIsAGetElementPtrInst(I))
		return 0;
	
	if(LLVMIsALoadInst(I) && LLVMIsALoadInst(J)){
		if(!AdjacentStoresOrLoads(I,J))		
			return 0;
	}
	if(LLVMIsAStoreInst(I) && LLVMIsAStoreInst(J)){
		if(!AdjacentStoresOrLoads(I,J))
			return 0;
	}
	
	
	return 1;
}


static void collectIsomorphicInstructions(VectorList* VecList,LLVMValueRef I, LLVMValueRef J){
	int i;
	//printf("In collect Isomorphic\n");	
	if( !shouldVectorize(I,J) ){
		//printf("Cannot vectorize\n");		
		return ;
	}
	if( valmap_check(VecList->visited,I) || valmap_check(VecList->visited,J)  )
		return ;
	
	VectorList_AppendPair(VecList, I , J);
	for(i=0; i<LLVMGetNumOperands(I);i++){
		if(Isomorphic( LLVMGetOperand(I,i),LLVMGetOperand(J,i) ) ){
			collectIsomorphicInstructions( VecList,LLVMGetOperand(I,i),LLVMGetOperand(J,i) );
		}		
	}
	return;
}

bool IsPresent(VectorList *List,LLVMValueRef I){

	VectorPair *traverse = List->head;
	while(traverse != NULL){
		if(traverse -> pair[0] == I || traverse -> pair[1] == I)	return false;
		traverse = traverse->next;
	}	
	return true;
}


static void SLPOnBasicBlock(LLVMBasicBlockRef BB)
{
  LLVMValueRef I;
  int changed;
  int i=0;
  VectorList *LIST;
  LIST = VectorList_Create();
  for(I=LLVMGetLastInstruction(BB);
      I!=NULL;
      I=LLVMGetPreviousInstruction(I))
    {
	//printf("1\n");
	VectorList *VecList;
	VecList =  VectorList_Create();
	//if(!valmap_check(LIST->visited,I)){
	//	VectorList_AppendPair(LIST,I,I);		
		if(LLVMIsAStoreInst(I) && IsPresent(LIST,I)){
			//printf("2\n");
		
			LLVMValueRef J = LLVMGetPreviousInstruction(I);
			while(J != NULL){
				if(LLVMIsAStoreInst(J)){
					//if(AdjacentStores(I,J)){
					if(AdjacentStoresOrLoads(I,J)){
						if(Isomorphic(I,J)){
							if(IsPresent(LIST,J)){
								
								collectIsomorphicInstructions(VecList,I,J);
								if(VecList->size >= 2){
									break;
								} 
							}
						}
					}
				}
				
				J = LLVMGetPreviousInstruction(J);
			}
			int k;
			if(VecList->head != NULL){
				VectorPair *temp = VecList->head;
				while(temp!=NULL){
					VectorList_AppendPair(LIST,temp->pair[0],temp->pair[1]);
					temp = temp->next;
				}
			}
			

				if(VecList->size == 1)	count[6]++;

				if(VecList->size == 2){
					count[0]++;
				}
				if(VecList->size == 3){
					count[1]++;
				}
				if(VecList->size == 4){
					count[2]++;
				}
				if(VecList->size >= 5){
					count[3]++;
				}
				if(VecList->size > 0){
					count[4]++;
					
				}
		}
	//}
	if(LLVMIsALoadInst(I) && IsPresent(LIST,I)){
		LLVMValueRef J = LLVMGetNextInstruction(I);
		while(J != NULL){
			if(LLVMIsALoadInst(J)){
				//if(AdjacentLoads(I,J)){
				if(AdjacentStoresOrLoads(I,J)){
					if(Isomorphic(I,J)){
						if(IsPresent(LIST,J)){
						
							collectIsomorphicInstructions(VecList,I,J);
							if(VecList->size >= 2){
								break;
							} 
						}
					}
				}
			}
			
			J = LLVMGetNextInstruction(J);
		}
		int k;
		if(VecList->head != NULL){
			VectorPair *temp = VecList->head;
			while(temp!=NULL){
				VectorList_AppendPair(LIST,temp->pair[0],temp->pair[1]);
				temp = temp->next;
			}
		}
			if(VecList->size == 1)	count[6]++;		
			if(VecList->size == 2) 
				count[0]++;
			if(VecList->size == 3){
				count[1]++;
			}
			if(VecList->size == 4){
				count[2]++;
			}
			if(VecList->size >= 5){
				count[3]++;
			}
			if(VecList->size > 0){
				count[5]++;
				
			}
    }
   }
}

static void SLPOnFunction(LLVMValueRef F) 
{
  LLVMBasicBlockRef BB;
  for(BB=LLVMGetFirstBasicBlock(F);
      BB!=NULL;
      BB=LLVMGetNextBasicBlock(BB))
    {
      SLPOnBasicBlock(BB);
    }
}

void SLP_C(LLVMModuleRef Module)
{
  	LLVMValueRef F;
  	for(F=LLVMGetFirstFunction(Module); 
      	F!=NULL;
      	F=LLVMGetNextFunction(F))
    	{
      		SLPOnFunction(F);
   	}
  	printf("Histogram:\n");
  	
  	printf(" 1: %d\n",count[6]);
  	printf(" 2: %d\n",count[0]);
  	printf(" 3: %d\n",count[1]);
  	printf(" 4: %d\n",count[2]);
  	printf(">5: %d\n",count[3]);
  
  	printf("nLoad: %d\n",count[5]);
	printf("nStore: %d\n",count[4]);
}
