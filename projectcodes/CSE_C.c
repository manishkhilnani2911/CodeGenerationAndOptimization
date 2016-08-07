/*
 * File: CSE_C.c
 *
 * Description:
 *   This is where you implement the C version of project 4 support.
 */
// Manish Khilnani unityid: mkhilna
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* LLVM Header Files */
#include "llvm-c/Core.h"
#include "dominance.h"
#include "transform.h"

/* Header file global to this project */
#include "cfg.h"
#include "CSE.h"
#include "worklist.h"

int CSE_Basic = 0;
int CSE_Dead = 0;
int CSE_Simplify = 0;
int CSE_RLoads = 0;
int CSE_Store2Load = 0;
int CSE_RStore = 0;
int flag,ICmp_flag;
static int commonSubexpression(LLVMValueRef I, LLVMValueRef J) {

	int i;
  	// return 1 if I and J are common subexpressions
  	// check these things:
	//   - they have the same opcode
  	//   - they have the same type
  	//   - they have the same number of operands
  	//   - all operands are the same (pointer equivalence) LLVMValueRef
  	// if any are false, return 0
	//printf("In commonSubexpression\n");
	
	if(LLVMIsAICmpInst(I) && LLVMIsAICmpInst(J)){
	
		if( (LLVMGetICmpPredicate(I) == LLVMGetICmpPredicate(J)) && (LLVMGetInstructionOpcode(I) == LLVMGetInstructionOpcode(J)) && (LLVMGetNumOperands(I) == LLVMGetNumOperands(J)) && (LLVMTypeOf(I) == LLVMTypeOf(J)) ){
			for(i=0;i<LLVMGetNumOperands(I);i++){
				if(LLVMGetOperand(I,i) == LLVMGetOperand(J,i))
					ICmp_flag = 1;				
				else{
					//even if 1 operands does not match come out of the loop
					ICmp_flag = 0;
					break;
				}					
			}		

		}
	
	}
	else if((LLVMGetInstructionOpcode(I) == LLVMGetInstructionOpcode(J)) && (LLVMGetNumOperands(I) == LLVMGetNumOperands(J)) && (LLVMTypeOf(I) == LLVMTypeOf(J)) ){
		for(i=0;i<LLVMGetNumOperands(I);i++){
			if(LLVMGetOperand(I,i) == LLVMGetOperand(J,i))
				flag = 1;				
			else{
				//even if 1 operands does not match come out of the loop
				flag = 0;
				break;
			}					
		}		
	}
	else{
		flag = 0;
		ICmp_flag = 0;
	}
	
	if(flag || ICmp_flag)
		return 1;
	else
		return 0;
	//printf("Leaving commonSubexpression\n");
  // return 0 is always conservative, so by default return 0
  return 0;
}

static int canHandle(LLVMValueRef I) 
{
  return ! 
    	(LLVMIsALoadInst(I) ||
      	LLVMIsAStoreInst(I) ||
      	LLVMIsATerminatorInst(I) ||
      	LLVMIsACallInst(I) ||
      	LLVMIsAPHINode(I) ||
      	LLVMIsAAllocaInst(I) || 
    	 LLVMIsAFCmpInst(I) ||
    	 LLVMIsAVAArgInst(I) ||
	LLVMIsAExtractValueInst(I) || 
	LLVMIsAExtractElementInst(I) ||
	LLVMIsAInsertElementInst(I) ||
	LLVMIsAInsertValueInst(I));
}

//check if an instruction is dead
int isDead(LLVMValueRef I)
{
	// Are there uses, if so not dead!
  	if (LLVMGetFirstUse(I)!=NULL)
  	return 0;

  	LLVMOpcode opcode = LLVMGetInstructionOpcode(I);
  	switch(opcode) {
  		// when in doubt, keep it! add opcode here to keep:
		case LLVMRet:
		case LLVMBr:
		case LLVMSwitch:
		case LLVMIndirectBr:
		case LLVMInvoke: 	
		case LLVMUnreachable:
		case LLVMFence:
		case LLVMStore:
		case LLVMCall:
		case LLVMAtomicCmpXchg:
		case LLVMAtomicRMW:
		case LLVMResume:	
		case LLVMLandingPad: 
		case LLVMExtractValue:
		case LLVMInsertValue:
		case LLVMExtractElement:
		case LLVMInsertElement:
		return 0;

  		case LLVMLoad: if(LLVMGetVolatile(I)) return 0;
  		// all others can be removed
  		default:
    		break;
  	}

  	// All conditions passed
  	return 1;
}

// Perform CSE on I for BB and all dominator-tree children
static void processInst(LLVMBasicBlockRef BB, LLVMValueRef I) 
{
  	// do nothing if trivially dead

  	// bale out if not an optimizable instruction
	LLVMValueRef J,Temp;
  	if(!canHandle(I)) return;

  	// CSE w.r.t. to I on BB
	if(LLVMGetInstructionParent(I) == BB)
		J = LLVMGetNextInstruction(I);
	else
		J = LLVMGetFirstInstruction(BB);
	for(; J != NULL; ){
		//getting all instructions following I in the same basic block and checking for CSE
		//Temp = J;
		if(commonSubexpression(I,J)){
			CSE_Basic++;
			LLVMReplaceAllUsesWith(J,I);
			Temp = J;
			J = LLVMGetNextInstruction(J);			
			//Temp = J;
			LLVMInstructionEraseFromParent(Temp);
			
		}
		else
		J = LLVMGetNextInstruction(J);
	}
	LLVMBasicBlockRef child = LLVMFirstDomChild(BB); 
	while (child!=NULL) {
		processInst(child,I);
   		child = LLVMNextDomChild(BB,child);  // get next child of BB	:
	}
}

static void FunctionCSE(LLVMValueRef Function) 
{
  	// for each bb:
  	//   for each isntruction
  	//       processInst
  	//
  	//   process memory instructions
	LLVMBasicBlockRef bb;
	LLVMValueRef temp,SI;
	int Iterator_flag = 0;
	//printf("In Function CSE\n");
	for(bb = LLVMGetFirstBasicBlock(Function); bb != NULL ; bb = LLVMGetNextBasicBlock(bb)){
		LLVMValueRef I;
		I = LLVMGetFirstInstruction(bb);
		//for(I = LLVMGetFirstInstruction(bb); I != NULL; ){
		while(I != NULL){
			Iterator_flag = 0;
			
			
			SI = InstructionSimplify(I);
			if(SI != NULL){
				temp = I;
				
				LLVMReplaceAllUsesWith(I,SI);
				I = LLVMGetNextInstruction(I);
				LLVMInstructionEraseFromParent(temp);
				CSE_Simplify++;
				continue;
			}
			//processInst(bb,I);
			
			//SI = InstructionSimplify(I);
			//if(SI != NULL){
			//	LLVMReplaceAllUsesWith(I,SI);
			//	CSE_Simplify++;
			//}
			//checking for redundant loads
			LLVMOpcode opcode,inst_opcode;
			LLVMOpcode opcode_load = LLVMLoad;
			LLVMOpcode opcode_store = LLVMStore;
			opcode = LLVMGetInstructionOpcode(I);
			LLVMValueRef inst_iter;
			if(opcode == opcode_load){     //if a load instruction
				inst_iter = LLVMGetNextInstruction(I);  //get the next instruction
				while(inst_iter){
					SI = InstructionSimplify(inst_iter);
					if(SI != NULL){
						temp = inst_iter;
						
						LLVMReplaceAllUsesWith(inst_iter,SI);
						inst_iter = LLVMGetNextInstruction(inst_iter);
						LLVMInstructionEraseFromParent(temp);
						CSE_Simplify++;
						continue;
					}
					inst_opcode = LLVMGetInstructionOpcode(inst_iter);
					if((inst_opcode == opcode_load) && (!LLVMGetVolatile(inst_iter)) && (LLVMTypeOf(I) == LLVMTypeOf(inst_iter)) && (LLVMGetOperand(I,0) == LLVMGetOperand(inst_iter,0)) ){ //check if there are redundant loads
						LLVMReplaceAllUsesWith(inst_iter,I);
						temp = inst_iter;
						inst_iter = LLVMGetNextInstruction(inst_iter);			
						LLVMInstructionEraseFromParent(temp);
						CSE_RLoads++;	
					}
					else if(inst_opcode == opcode_store /*&& LLVMGetOperand(I,0) == LLVMGetOperand(inst_iter,1)*/)
						break;
					else{
						inst_iter = LLVMGetNextInstruction(inst_iter);
						//Iterator_flag = 1;
					}
				}
			}
			if(opcode == opcode_store){
				//printf("Here1\n");
				inst_iter = LLVMGetNextInstruction(I);
				while(inst_iter != NULL){
					//printf("Here2\n");
					SI = InstructionSimplify(inst_iter);
					if(SI != NULL){
						temp = inst_iter;
						
						LLVMReplaceAllUsesWith(inst_iter,SI);
						inst_iter = LLVMGetNextInstruction(inst_iter);
						LLVMInstructionEraseFromParent(temp);
						CSE_Simplify++;
						continue;
					}
					inst_opcode = LLVMGetInstructionOpcode(inst_iter);
					if(inst_opcode == opcode_load && (!LLVMGetVolatile(inst_iter)) && (LLVMGetOperand(I,1) == LLVMGetOperand(inst_iter,0)) && LLVMTypeOf(inst_iter) == LLVMTypeOf(LLVMGetOperand(I,0))){
						//printf("Here3\n");
						temp = inst_iter;
						LLVMReplaceAllUsesWith(inst_iter,LLVMGetOperand(I,0));
						//temp = inst_iter;
						inst_iter = LLVMGetNextInstruction(inst_iter);
						LLVMInstructionEraseFromParent(temp);
						CSE_Store2Load++;
					}
			//	}
		//	}
					else if(inst_opcode == opcode_store && LLVMGetOperand(I,1)== LLVMGetOperand(inst_iter,1) && (!LLVMGetVolatile(I)) && LLVMTypeOf(LLVMGetOperand(I,0))== LLVMTypeOf(LLVMGetOperand(inst_iter,0))){
						temp = I;
						I = LLVMGetNextInstruction(I);
						LLVMInstructionEraseFromParent(temp);
						Iterator_flag = 1;
						CSE_RStore++;
						break;
					}
					else if(inst_opcode == opcode_load || inst_opcode == opcode_store)
						break;
					else
						 inst_iter = LLVMGetNextInstruction(inst_iter);
				}
			}
			processInst(bb,I);
			if(isDead(I)){
				CSE_Dead++;
				temp = I;
				I = LLVMGetNextInstruction(I);
				//Iterator_flag = 1;
				LLVMInstructionEraseFromParent(temp);
				continue;
			}
			if(!Iterator_flag)		
			I = LLVMGetNextInstruction(I);		
		}
	}
}


void LLVMCommonSubexpressionElimination(LLVMModuleRef Module)
{
  // Loop over all functions
  LLVMValueRef Function;
  for (Function=LLVMGetFirstFunction(Module);Function!=NULL;
       Function=LLVMGetNextFunction(Function))
    {
	//printf("Calling FunctionCSE\n");
      FunctionCSE(Function);
	//printf("Back in Function after CSE\n");
    }
	printf("%d %d %d %d %d %d\n",CSE_Basic,CSE_Dead,CSE_Simplify,CSE_RLoads,CSE_Store2Load,CSE_RStore);
  // print out summary of results
}

