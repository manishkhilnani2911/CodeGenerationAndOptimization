/*
 * File: LICM_C.c
 *
 * Description:
 *   Stub for LICM in C. This is where you implement your LICM pass.
 */
 
//Manish Khilnani unityid mkhilna studentid: 200061966
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* LLVM Header Files */
#include "llvm-c/Core.h"
#include "dominance.h"

/* Header file global to this project */
#include "cfg.h"
#include "loop.h"
#include "worklist.h"
#include "valmap.h"

static worklist_t list;
valmap_t StoreMap;
valmap_t CallMap;
int loop_c;


static LLVMBuilderRef Builder=NULL;

unsigned int LICM_Count=0;
unsigned int LICM_NoPreheader=0;
unsigned int LICM_AfterLoop=0;
unsigned int LICM_Load=0;
unsigned int LICM_BadCall=0;
unsigned int LICM_BadStore=0;

int canMoveOutOfLoop(LLVMLoopRef L,LLVMValueRef I,LLVMValueRef LoadAddr, LLVMValueRef funs);
void LICMOnFunction(LLVMValueRef funs);
void LICM(LLVMLoopRef L,LLVMValueRef funs);

void LoopInvariantCodeMotion_C(LLVMModuleRef Module)
{
  LLVMValueRef funs;
	loop_c = 0;
  Builder = LLVMCreateBuilder();

  list = worklist_create();
	//StoreMap = valmap_create();
	//CallMap = valmap_create();

	//printf("program starting\n");
  for(funs=LLVMGetFirstFunction(Module);
      funs!=NULL;
      funs=LLVMGetNextFunction(funs))
    { 
      if (LLVMCountBasicBlocks(funs)){
		//printf("calling LICMOnFunction\n");      
		LICMOnFunction(funs);
		//printf("after calling LICMOnFunction\n"); 
	}
    }

  LLVMDisposeBuilder(Builder);
  Builder = NULL;

  fprintf(stderr,"LICM_Count      =%d\n",LICM_Count);
  fprintf(stderr,"LICM_NoPreheader=%d\n",LICM_NoPreheader);
  fprintf(stderr,"LICM_Load       =%d\n",LICM_Load);
  fprintf(stderr,"LICM_BadCall    =%d\n",LICM_BadCall);
  fprintf(stderr,"LICM_BadStore   =%d\n",LICM_BadStore);
	fprintf(stderr,"Loops   =%d\n",loop_c);
}

void LICMOnFunction(LLVMValueRef funs)
{
	LLVMLoopInfoRef LoopInfo;
	LLVMLoopRef LoopVar;
	LLVMBasicBlockRef BBRef;
	
	LoopInfo = LLVMCreateLoopInfoRef(funs); //gets info of all loops in a function
	for(LoopVar = LLVMGetFirstLoop(LoopInfo); LoopVar!=NULL; LoopVar = LLVMGetNextLoop(LoopInfo,LoopVar)){
		//printf("In LICMOnFunction : calling LICM\n");	
		StoreMap = valmap_create();
		CallMap = valmap_create();
		LICM(LoopVar,funs);
		//iterate over all loop
		//printf("After calling LICM back in LICMOnFunction\n");
		valmap_destroy(StoreMap);
		valmap_destroy(CallMap);
		loop_c++;
	}
}

void LICM(LLVMLoopRef L, LLVMValueRef funs)
{
	worklist_t BBList;	
	BBList = worklist_create();
	LLVMBasicBlockRef BB;
//	BBList = worklist_create();	
	LLVMValueRef I,LoadAddr,clone,PHLI,temp;	
	LLVMBasicBlockRef PH = LLVMGetPreheader(L);
	if(PH == NULL){
		//printf("No preheader present returning\n");
		LICM_NoPreheader++;
		return;
	}
	//getting all the basic blocks in a loop
	BBList = LLVMGetBlocksInLoop(L);
	//getting the basic blocks one by one in a loop
	BB = (LLVMBasicBlockRef)worklist_pop(BBList);
	while(BB != NULL){
		StoreMap = valmap_create();
		CallMap = valmap_create();
		//printf("In LICM : iterating through all the BB's\n");
		I = LLVMGetFirstInstruction(BB);
		while( I != NULL )
		{
			//printf("In LICM : iterating through all the instructions\n");
			if(LLVMMakeLoopInvariant(L,I)){
				//printf("In LICM : checking loop invariant cond\n");
				LICM_Count++;
				I = LLVMGetNextInstruction(I);
				continue;
			}
			else if(LLVMIsALoadInst(I)){
				//printf("In LICM : Calling canMoveOutOfLoop\n");
				LoadAddr = LLVMGetOperand(I,0);
				
				if(canMoveOutOfLoop(L,I,LoadAddr,funs)){
					//printf("In LICM : after calling canMoveOutOfLoop\n");
					LICM_Load++;
					LICM_Count++;
					clone = LLVMCloneInstruction(I);
					//getting last instruction of the preheader
					PHLI = LLVMGetLastInstruction(PH);
					//placing builder before last instruction
					LLVMPositionBuilderBefore(Builder,PHLI);
					//moving the instruction before the last instruction
					LLVMInsertIntoBuilder(Builder,clone);
					temp = I;					
					LLVMReplaceAllUsesWith(I,clone);					
					I = LLVMGetNextInstruction(I);					
					LLVMInstructionEraseFromParent(temp);
					//printf("In LICM : Instruction deleted and uses replaced, continuing\n");					
					continue;
				}
				else{
					//printf("In LICM : Load instruction but can not move out of loop\n");
					I = LLVMGetNextInstruction(I);
					continue;
				}
			}
			else{
				//printf("In LICM : No condition matched continuing\n");
				I = LLVMGetNextInstruction(I);	
				continue;
			}
		}
		BB = (LLVMBasicBlockRef)worklist_pop(BBList);				
	}
	//printf("In LICM : returning\n");
}

int canMoveOutOfLoop(LLVMLoopRef L,LLVMValueRef I,LLVMValueRef LoadAddr, LLVMValueRef funs)
{
	worklist_t BBList1;
	worklist_t ExitList;
	worklist_t ExitList1;
	worklist_t ExitList2;	
	BBList1 = worklist_create();
	ExitList = worklist_create();
	ExitList1 = worklist_create();
	ExitList2 = worklist_create();
	int flag;
	LLVMBasicBlockRef BB,ParentBlock,ExitBlock,ExitBlock1,ExitBlock2;
	BBList1 = LLVMGetBlocksInLoop(L);
	LLVMValueRef Inst,StoreAddr;
	BB = (LLVMBasicBlockRef)worklist_pop(BBList1);
	//iterating all basic blocks in the loop	
	while(BB != NULL){
		Inst = LLVMGetFirstInstruction(BB);
		//printf("In canMoveOutOfLoop : Iterating through all BB's \n");
		while(Inst){
			if(LLVMIsAConstant(LoadAddr)){
				//printf("In canMoveOutOfLoop : Iterating through all Inst \n");
				if(LLVMIsAStoreInst(Inst)){
					//printf("Checking condition 1\n");
					if(LLVMGetOperand(Inst,1) == LoadAddr){
						//if store to a same address then return zero						
						//printf("Found store to the same location returning\n");
						//condition to check if same store is not counted twice in case of bad store
						if(!valmap_check(StoreMap,LLVMGetOperand(Inst,1))){
							valmap_insert(StoreMap,LLVMGetOperand(Inst,1),LLVMGetOperand(Inst,0));	
							LICM_BadStore++;							
							return 0;
						}
						else						
		//				LICM_BadStore++;						
							return 0;
					}
					else if( LLVMIsAAllocaInst(LLVMGetOperand(Inst,1)) || LLVMIsAConstant(LLVMGetOperand(Inst,1)) ){
						//printf("Store was not at the same location but at alloca or constant location store\n");
						//ParentBlock = LLVMGetInstructionParent(I);						
						//ExitList1 = LLVMGetExitBlocks(L);
						//ExitBlock1 = (LLVMBasicBlockRef)worklist_pop(ExitList1);
						//while(ExitBlock1){
							//printf("Checking for dominance relationship\n");
						//	if(LLVMDominates(funs, ParentBlock, ExitBlock1)){
						//		flag = 0;
						//		ExitBlock1 = (LLVMBasicBlockRef)worklist_pop(ExitList1);
						//	}
							/*else{ // if store is an alloca or a store to a constant location but it does not dominate loop exit return 0
								//printf("Dominance relationship failed returning\n");	

								//check if store is already present in valmap and dont increment counter if already present
								if(!valmap_check(StoreMap,LLVMGetOperand(Inst,1))){
									valmap_insert(StoreMap,LLVMGetOperand(Inst,1),LLVMGetOperand(Inst,0));	
									LICM_BadStore++;							
									return 0;
								}
								else 
									return 0;
							}*/	
						//}
						//if(flag == 0){ // if all condition true then check next instruction
							Inst = LLVMGetNextInstruction(Inst);
							//printf("Store at alloca or constant, dominance relationship also satisfied goint to the next instruction\n");
							continue;	
						//}					
					}
					else {
						if(!valmap_check(StoreMap,LLVMGetOperand(Inst,1))){
							valmap_insert(StoreMap,LLVMGetOperand(Inst,1),LLVMGetOperand(Inst,0));	
							LICM_BadStore++;							
							return 0;
						}					
						else 
							return 0;

					}	
				}
				else if(LLVMIsACallInst(Inst)){
					if(!valmap_check(CallMap,LLVMGetOperand(Inst,0))){
						valmap_insert(CallMap,LLVMGetOperand(Inst,0),NULL);						
						LICM_BadCall++;
						return 0;
					}
					else
						return 0;
				}
				else{  //Load address is a constant looking for stores to the same address
					Inst = LLVMGetNextInstruction(Inst);
					continue;						
				}
			}
			else if(LLVMIsAAllocaInst(LoadAddr) && (!LLVMLoopContainsInst(L,LoadAddr))){
				if(LLVMIsAStoreInst(Inst)){
					if(LLVMGetOperand(Inst,1) == LoadAddr){ //only if store is at alloca
						if(!valmap_check(StoreMap,LLVMGetOperand(Inst,1))){
							valmap_insert(StoreMap,LLVMGetOperand(Inst,1),LLVMGetOperand(Inst,0));	
							LICM_BadStore++;							
							return 0;
						}
						else 
							return 0;						
			//			return 0;
					}
					else if(LLVMIsAConstant(LLVMGetOperand(Inst,1))) {// store to a different constant address 
						/*ParentBlock = LLVMGetInstructionParent(I);						
						ExitList2 = LLVMGetExitBlocks(L);
						ExitBlock2 = (LLVMBasicBlockRef)worklist_pop(ExitList2);
						while(ExitBlock2){
							if(LLVMDominates(funs, ParentBlock, ExitBlock2)){
								flag = 0;
								ExitBlock2 = (LLVMBasicBlockRef)worklist_pop(ExitList2);
							}
							else{ // if store is an alloca or a store to a constant location but it does not dominate loop exit return 0
								//condition to check if same store is not counted twice in case of bad store
								if(!valmap_check(StoreMap,LLVMGetOperand(Inst,1))){
									valmap_insert(StoreMap,LLVMGetOperand(Inst,1),LLVMGetOperand(Inst,0));	
									LICM_BadStore++;							
									return 0;
								}
								else 
									return 0;
							}	
						}
						if(flag == 0){ // if all condition true then check next instruction*/
							Inst = LLVMGetNextInstruction(Inst);
							continue;	
						//}					
					}
					else{
						if(!valmap_check(StoreMap,LLVMGetOperand(Inst,1))){
							valmap_insert(StoreMap,LLVMGetOperand(Inst,1),LLVMGetOperand(Inst,0));	
							LICM_BadStore++;							
							return 0;
						}					
						else 
							return 0;
					}
				}
				else if(LLVMIsACallInst(Inst)){
					if(!valmap_check(CallMap,LLVMGetOperand(Inst,0))){
						valmap_insert(CallMap,LLVMGetOperand(Inst,0),NULL);						
						LICM_BadCall++;
						return 0;
					}
					else
						return 0;
				}
				else{
					Inst = LLVMGetNextInstruction(Inst);
					continue;
				}
			}
			else if(!LLVMLoopContainsInst(L,LoadAddr)){
				if(!LLVMIsAStoreInst(Inst)){
					/*ParentBlock = LLVMGetInstructionParent(I);
					ExitList = LLVMGetExitBlocks(L);
					ExitBlock = (LLVMBasicBlockRef)worklist_pop(ExitList);
					while(ExitBlock){
						if(LLVMDominates(funs, ParentBlock, ExitBlock)){
							flag = 0;
							ExitBlock = (LLVMBasicBlockRef)worklist_pop(ExitList);
						}
						else{
							if(LLVMIsACallInst(Inst))
								LICM_BadCall++;
							return 0;
						}	

					}
					if(flag == 0){ */ // if all condition true then check next instruction
						Inst = LLVMGetNextInstruction(Inst);
						continue;
					//}
				}
				//condition to check if same store is not counted twice in case of bad store
				else if(LLVMIsAStoreInst(Inst)){
					if(!valmap_check(StoreMap,LLVMGetOperand(Inst,1))){
						valmap_insert(StoreMap,LLVMGetOperand(Inst,1),LLVMGetOperand(Inst,0));	
						LICM_BadStore++;										
						return 0;
					}
					else 
						return 0;
				}
				else{
					Inst = LLVMGetNextInstruction(Inst);
					continue;
				}
			}
			else{
				return 0;
			}
		}
		BB = (LLVMBasicBlockRef)worklist_pop(BBList1);
	}
	//printf("Load can be moved returning from canMoveOutOfLoop\n");
	ParentBlock = LLVMGetInstructionParent(I);
	ExitList = LLVMGetExitBlocks(L);
	ExitBlock = (LLVMBasicBlockRef)worklist_pop(ExitList);
	while(ExitBlock){
		if(LLVMDominates(funs, ParentBlock, ExitBlock)){
			flag = 0;
			ExitBlock = (LLVMBasicBlockRef)worklist_pop(ExitList);
		}
		else{
			return 0;
		}	
	}
	if(flag == 0)
		return 1;
}
