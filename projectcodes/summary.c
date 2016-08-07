/*
 * File: summary.c
 *
 * Description:
 *   This is where you implement your project 3 support.
 */
// submitted by manish khilnani unityid: mkhilna
//programming language used is C
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* LLVM Header Files */
#include "llvm-c/Core.h"
#include "dominance.h"

/* Header file global to this project */
#include "summary.h"

typedef struct Stats_def {
  int functions;
  int globals;
  int bbs;
  int insns;
  int insns_bb_deps;
  int insns_g_deps;
  int branches;
  int loads;
  int stores;
  int calls;
  int loops; //approximated by backedges
} Stats;

void pretty_print_stats(FILE *f, Stats s, int spaces)
{
  char spc[128];
  int i;

  // insert spaces before each line
  for(i=0; i<spaces; i++)
    spc[i] = ' ';
  spc[i] = '\0';
    
  fprintf(f,"%sFunctions.......................%d\n",spc,s.functions);
  fprintf(f,"%sGlobal Vars.....................%d\n",spc,s.globals);
  fprintf(f,"%sBasic Blocks....................%d\n",spc,s.bbs);
  fprintf(f,"%sInstructions....................%d\n",spc,s.insns);
  fprintf(f,"%sInstructions (bb deps)..........%d\n",spc,s.insns_bb_deps);
  fprintf(f,"%sInstructions (g/c deps).........%d\n",spc,s.insns_g_deps);
  fprintf(f,"%sInstructions - Branches.........%d\n",spc,s.branches);
  fprintf(f,"%sInstructions - Loads............%d\n",spc,s.loads);
  fprintf(f,"%sInstructions - Stores...........%d\n",spc,s.stores);
  fprintf(f,"%sInstructions - Calls............%d\n",spc,s.calls);
  fprintf(f,"%sInstructions - Other............%d\n",spc,
	  s.insns-s.branches-s.loads-s.stores);
  fprintf(f,"%sLoops...........................%d\n",spc,s.loops);
}

void print_csv_file(const char *filename, Stats s, const char *id)
{
  FILE *f = fopen(filename,"w");
  fprintf(f,"id,%s\n",id);
  fprintf(f,"functions,%d\n",s.functions);
  fprintf(f,"globals,%d\n",s.globals);
  fprintf(f,"bbs,%d\n",s.bbs);
  fprintf(f,"insns,%d\n",s.insns);
  fprintf(f,"insns_bb_deps,%d\n",s.insns_bb_deps);
  fprintf(f,"insns_g_deps,%d\n",s.insns_g_deps);
  fprintf(f,"branches,%d\n",s.branches);
  fprintf(f,"loads,%d\n",s.loads);
  fprintf(f,"stores,%d\n",s.stores);
  fprintf(f,"calls,%d\n",s.calls);
  fprintf(f,"loops,%d\n",s.loops);
  fclose(f);
}

Stats MyStats;

struct node
{
	LLVMBasicBlockRef header;
	struct node* next;
};

struct node *head = NULL;

void insert(LLVMBasicBlockRef loop_head)  // linklist to store header
{
	struct node* temp ;
	temp = (struct node*)malloc(sizeof(struct node));
	temp->header = loop_head;
	if(head == NULL ){
		temp->next = head; 
		head = temp; // if list is empty enter at head
		return;
	}
	else{
		struct node* temp1 = head;
		while(temp1->next!=NULL){
			temp1 = temp1->next;
		}
		temp1->next = temp;
		temp->next = NULL;
	}
}	

int find(LLVMBasicBlockRef val)
{
	struct node* temp ;
	temp = head;
	int return_val = 0;
	while(temp!=NULL)
	{
		if(temp->header == val)
		{
			return_val = 1;
			break;
		}
		temp = temp->next;
	//printf("In find\n");	
	}
	return return_val;
}

void delete_list()
{
	head = NULL;	
}
void count_loops()
{
	struct node* temp ;
        temp = head;
	while(temp!=NULL)
	{
		MyStats.loops++;
		temp=temp->next;
	}

}

void Summarize(LLVMModuleRef Module, const char *id, const char* filename)
{
	//Stats MyStats;
	int num_of_operands,flag,defined_function_flag,i,flag_bb_deps,list_find;
	LLVMValueRef fn_iter,global_iter;
	//loop to count total number of global variables
	for(global_iter = LLVMGetFirstGlobal(Module); global_iter!=NULL;global_iter = LLVMGetNextGlobal(global_iter))
	{	
		if(LLVMGetNumOperands(global_iter)>0)
		MyStats.globals++;
	}
	for(fn_iter = LLVMGetFirstFunction(Module); fn_iter!=NULL;fn_iter = LLVMGetNextFunction(fn_iter))
	{ 
		defined_function_flag = 0;
		LLVMBasicBlockRef bb_iter;
		for(bb_iter = LLVMGetFirstBasicBlock(fn_iter);bb_iter!=NULL;bb_iter = LLVMGetNextBasicBlock(bb_iter))
		{
			defined_function_flag++;
			//counting number of basic blocks
			MyStats.bbs++;
			LLVMValueRef inst_iter;
			for(inst_iter = LLVMGetFirstInstruction(bb_iter);inst_iter != NULL; inst_iter = LLVMGetNextInstruction(inst_iter))
			{
				//counting  umber of instructions
				MyStats.insns++;			
				LLVMOpcode opcode = LLVMGetInstructionOpcode(inst_iter);
				switch(opcode)
				{
					case LLVMStore: 
						//number of stores
						MyStats.stores++;
						break;
					case LLVMLoad: 
						//number of loads
						MyStats.loads++;
						break;		
					case LLVMCall: 
						//number of function calls
						MyStats.calls++;
						break;
					case LLVMBr:
						// checking for conditional branches
						num_of_operands = LLVMGetNumOperands(inst_iter);
						if(num_of_operands>1)
							MyStats.branches++;
						if(num_of_operands == 1)
						{
							LLVMBasicBlockRef block_a = LLVMGetInstructionParent(inst_iter);
							LLVMValueRef target = LLVMGetOperand(inst_iter,0);
							LLVMBasicBlockRef block_b = LLVMValueAsBasicBlock(target);
							if(LLVMDominates(fn_iter,block_b,block_a))
								//MyStats.loops++;
							{
								
								list_find = find(block_b);
								if(!list_find)
									insert(block_b);

							}
								
						}
						if(num_of_operands == 3)
						{
							LLVMBasicBlockRef block_a = LLVMGetInstructionParent(inst_iter);
							LLVMValueRef target = LLVMGetOperand(inst_iter,1);
							LLVMValueRef target1 = LLVMGetOperand(inst_iter,2);
							LLVMBasicBlockRef block_b = LLVMValueAsBasicBlock(target);
							LLVMBasicBlockRef block_c = LLVMValueAsBasicBlock(target1);
							if(LLVMDominates(fn_iter,block_b,block_a))
								//MyStats.loops++;	
							{
                                                                list_find = find(block_b);
								//list_find1 = find(block_c);
                                                                if(!list_find)
                                                                        insert(block_b);

                                                        }
							if(LLVMDominates(fn_iter,block_c,block_a))
							{
								 list_find = find(block_c);
                                                                //list_find1 = find(block_c);
                                                                if(!list_find)
                                                                        insert(block_c);

							}
						}
						break;
					default: break;
				}
				// checking for instructions having constant or global operands
				if(LLVMGetNumOperands(inst_iter) > 0){
					flag = 1;
					for(i = 0;i<LLVMGetNumOperands(inst_iter);i++)
					{
						LLVMValueRef operand = LLVMGetOperand(inst_iter,i);
						if(LLVMIsAGlobalValue(operand) || LLVMIsAConstant(operand))
							flag = flag & 1;
						else
							flag = flag & 0;
					
					}
					if (flag == 1)
						MyStats.insns_g_deps++;
				}
				flag_bb_deps = 0;
				for(i=0;i<LLVMGetNumOperands(inst_iter);i++)
				{
					LLVMValueRef operand = LLVMGetOperand(inst_iter,i);
					if(LLVMIsAInstruction(operand))
					{
						if(LLVMGetInstructionParent(operand) == LLVMGetInstructionParent(inst_iter))
							flag_bb_deps = 1;
					}
				
				}
				if(flag_bb_deps == 1)
					MyStats.insns_bb_deps++;
			}
		}
		//function is defined if only it has atleast one basic block
		if(defined_function_flag>0)
			MyStats.functions++;
		count_loops();
		delete_list();
	}
	print_csv_file(filename, MyStats, id);
	//pretty_print_stats(f, MyStats,0);			
}

