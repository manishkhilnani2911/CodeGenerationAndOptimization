%{
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "llvm-c/Core.h"
#include "llvm-c/BitReader.h"
#include "llvm-c/BitWriter.h"

#include "list.h"
#include "symbol.h"

int num_errors;


extern int yylex();   /* lexical analyzer generated from lex.l */

int yyerror();
int parser_error(const char*);

void minic_abort();
char *get_filename();
int get_lineno();

int loops_found=0;

extern LLVMModuleRef Module;
extern LLVMContextRef Context;
 LLVMBuilderRef Builder;

LLVMValueRef phi_val;
LLVMValueRef incoming[2] = {NULL};
LLVMValueRef Function=NULL;
LLVMValueRef BuildFunction(LLVMTypeRef RetType, const char *name, 
			   paramlist_t *params);

%}

/* Data structure for tree nodes*/

%union {
  int num;
  char * id;
  LLVMTypeRef  type;
  LLVMValueRef value;
  LLVMBasicBlockRef bb;
  paramlist_t *params;
}

/* these tokens are simply their corresponding int values, more terminals*/

%token SEMICOLON COMMA COLON
%token LBRACE RBRACE LPAREN RPAREN LBRACKET RBRACKET
%token ASSIGN PLUS MINUS STAR DIV MOD 
%token LT GT LTE GTE EQ NEQ NOT
%token LOGICAL_AND LOGICAL_OR
%token BITWISE_OR BITWISE_XOR LSHIFT RSHIFT BITWISE_INVERT

%token DOT ARROW AMPERSAND QUESTION_MARK

%token FOR WHILE IF ELSE DO STRUCT SIZEOF RETURN 
%token BREAK CONTINUE
%token INT VOID

/* no meaning, just placeholders */
%token STATIC AUTO EXTERN TYPEDEF CONST VOLATILE ENUM UNION REGISTER
/* NUMBER and ID have values associated with them returned from lex*/

%token <num> NUMBER /*data type of NUMBER is num union*/
%token <id>  ID

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

/* values created by parser*/

%type <id> declarator
%type <params> param_list param_list_opt
%type <value> expression
%type <value> assignment_expression
%type <value> conditional_expression
%type <value> constant_expression
%type <value> logical_OR_expression
%type <value> logical_AND_expression
%type <value> inclusive_OR_expression
%type <value> exclusive_OR_expression
%type <value> AND_expression
%type <value> equality_expression
%type <value> relational_expression
%type <value> shift_expression
%type <value> additive_expression
%type <value> multiplicative_expression
%type <value> cast_expression
%type <value> unary_expression
%type <value> lhs_expression
%type <value> postfix_expression
%type <value> primary_expression
%type <value> constant
%type <type>  type_specifier
%type <value> statement
%type <value> compound_stmt
%type <value> statement_list_opt
%type <value> statement_list
%type <value> expr_stmt
%type <value> selection_stmt
%type <value> iteration_stmt
%type <value> jump_stmt
%type <value> break_stmt
%type <value> expr_opt
%type <value> continue_stmt

%left PLUS MINUS
%left STAR DIV
/* 
   The grammar used here is largely borrowed from Kernighan and Ritchie's "The C
   Programming Language," 2nd Edition, Prentice Hall, 1988. 

   But, some modifications have been made specifically for MiniC!
 */

%%

/* 
   Beginning of grammar: Rules
*/

translation_unit:external_declaration
{
	printf("Final rule\n");
}
| translation_unit external_declaration
;

external_declaration:function_definition
{
  /* finish compiling function */
	printf("In external_declaration:function_definition\n");
  	if(num_errors>100)
    	{
      		minic_abort();
    	}
  	else if(num_errors==0)
    	{
      
    	}
}
| declaration 
{ 
  /* nothing to be done here */
}
;

function_definition:type_specifier ID LPAREN param_list_opt RPAREN 
{
	printf("In function_definition Int return type\n");
  	symbol_push_scope();
  	/* This is a mid-rule action */
  	BuildFunction($1,$2,$4);  
} 
compound_stmt 
{ 
  /* This is the rule completion */
  	LLVMBasicBlockRef BB = LLVMGetInsertBlock(Builder);
  	if(!LLVMGetBasicBlockTerminator(BB))
    	{
      		LLVMBuildRet(Builder,LLVMConstInt(LLVMInt32TypeInContext(Context),
					0,(LLVMBool)1));
    	}

  	symbol_pop_scope();
  /* make sure basic block has a terminator (a return statement) */
}
| type_specifier STAR ID LPAREN param_list_opt RPAREN 
{
	printf("In function definition Int pointer return type\n");
  	symbol_push_scope();
  	BuildFunction(LLVMPointerType($1,0),$3,$5);
} 
compound_stmt 
{ 
  /* This is the rule completion */
  /* make sure basic block has a terminator (a return statement) */

  	LLVMBasicBlockRef BB = LLVMGetInsertBlock(Builder);
  	if(!LLVMGetBasicBlockTerminator(BB))
    	{
      		LLVMBuildRet(Builder,LLVMConstPointerNull(LLVMPointerType(LLVMInt32TypeInContext(Context),0)));
    	}

  	symbol_pop_scope();
}
;

declaration:type_specifier STAR declarator SEMICOLON
{
	printf("In declaration:type_specifier STAR declarator SEMICOLON\n");
	int isArg =0;
        LLVMValueRef val = symbol_find($3,&isArg);
  	if (is_global_scope())
    	{
      		LLVMAddGlobal(Module,LLVMPointerType($1,0),$3);
    	} 
  	else
    	{
		if(val == NULL)
      		symbol_insert($3,  /* map name to alloca */
		LLVMBuildAlloca(Builder,LLVMPointerType($1,0),$3), /* build alloca */
		    0);  /* not an arg */
		else
		return(internal_error("Multiple declaration\n"));
    	}	

} 
| type_specifier declarator SEMICOLON
{
	printf("In type_specifier declarator SEMICOLON\n");
  	int isArg =0;
	LLVMValueRef val = symbol_find($2,&isArg);
	
	if (is_global_scope())
    	{
      		LLVMAddGlobal(Module,$1,$2);
    	}
  	else
    	{
		if(val == NULL)
      		symbol_insert($2,  /* map name to alloca */
		LLVMBuildAlloca(Builder,$1,$2), /* build alloca */
		    0);  /* not an arg */
		else
		return(internal_error("Multiple declaration\n"));
    	}
} 
;

declaration_list:declaration
{

}
|declaration_list declaration  
{

}
;

type_specifier:INT 
{
	printf("In type_specifier:INT\n");
  	$$ = LLVMInt32TypeInContext(Context);
}
;
declarator:ID
{
	printf("In declarator:ID\n");
	//int isArg=0;
	//LLVMValueRef Val = symbol_find($1,&isArg);
	//if(isArg)
	//return(internal_error("M\n"));
	//else
  	$$ = $1;
}
;

param_list_opt:           
{ 
  	$$ = NULL;
}
| param_list
{ 
  	$$ = $1;
}
;

param_list:param_list COMMA type_specifier declarator
{
  	$$ = push_param($1,$4,$3);
}
|param_list COMMA type_specifier STAR declarator
{
  	$$ = push_param($1,$5,LLVMPointerType($3,0));
}
|param_list COMMA type_specifier
{
  	$$ = push_param($1,NULL,$3);
}
|type_specifier declarator
{
  /* create a parameter list with this as the first entry */
  	$$ = push_param(NULL, $2, $1);
}
|type_specifier STAR declarator
{
  /* create a parameter list with this as the first entry */
	printf("In param_list:type_specifier STAR declarator\n");
  	$$ = push_param(NULL, $3, LLVMPointerType($1,0));
}
|type_specifier
{
  /* create a parameter list with this as the first entry */
  	$$ = push_param(NULL, NULL, $1);
}
;
statement:expr_stmt    
	{
		printf("In statement:expr_stmt\n");
                //$$ = $1;
	}        
	|compound_stmt 
	{
		 printf("In statement:compound_stmt\n");
                //$$ = $1;
	}       
	|selection_stmt
	{
		 printf("In statement:selection_stmt\n");
               // $$ = $1;
	}       
	|iteration_stmt 
	{
		printf("In statement:iteration_stmt\n");
                //$$ = $1;
	}      
	|jump_stmt            
	{
		printf("In statement:jump_stmt\n");
		//$$ = $1;
	}
        |break_stmt
	{
		printf("In statement:break_stmt\n");
		//$$ = $1;
	}
        |continue_stmt
	{	
		printf("In statement:continue_stmt\n");
		//$$ = $1;
	}
;

expr_stmt:SEMICOLON            
{ 

}
|expression SEMICOLON       
{ 

}
;

compound_stmt:LBRACE declaration_list_opt statement_list_opt RBRACE 
{
	//code added might be wrong
	printf("In compound_stmt:LBRACE declaration_list_opt statement_list_opt RBRACE\n");
	$$ = $3;

}
;

declaration_list_opt:	
{

}
|declaration_list
{

}
;

statement_list_opt:	
{

}
|statement_list
{
	printf("In statement_list_opt:statement_list\n");
	$$ = $1;
}
;

statement_list:statement
{
	//code added
	printf("In statement_list:statement\n");
	$$ = $1;
}
|statement_list statement
{
	//code added
	printf("In statement_list statement\n");
	$$ = $2;
}
;

break_stmt:BREAK SEMICOLON
{
	printf("In BREAK\n");
	loop_info_t info;
    	info = get_loop();
	if (info.expr == NULL)
	{
		return(internal_error("illegal assignment for break"));
	}
	else
	{
		LLVMBuildBr(Builder,info.exit);
		LLVMBasicBlockRef new_block = LLVMAppendBasicBlock(Function,"new.block");
		LLVMPositionBuilderAtEnd(Builder, new_block);
	}
};	

continue_stmt:CONTINUE SEMICOLON
{
	loop_info_t temp;
        temp = get_loop();
	if(temp.expr == NULL)
       		return(internal_error("BREAK without loop!!\n"));
	else if(temp.body == temp.reinit)
	{
        	LLVMBuildBr(Builder,temp.expr);
        	LLVMBasicBlockRef new_block = LLVMAppendBasicBlock(Function,"new.block");
        	LLVMPositionBuilderAtEnd(Builder,new_block);
	}
	else
	{
		LLVMBuildBr(Builder,temp.reinit);
                LLVMBasicBlockRef new_block = LLVMAppendBasicBlock(Function,"new.block");
                LLVMPositionBuilderAtEnd(Builder,new_block);
	}
};

selection_stmt:IF LPAREN expression 
{ 
	printf("In IF condition\n");
	//Creating then and else block
	LLVMBasicBlockRef then = LLVMAppendBasicBlock(Function,"then.block");
	LLVMBasicBlockRef ielse = LLVMAppendBasicBlock(Function,"else.block");
	//LLVMBasicBlockRef join = LLVMAppendBasicBlock(Function,"else.join");	

	//checking for $3 to be true and then deciding control flow accordingly
	LLVMValueRef zero = LLVMConstInt(LLVMTypeOf($3),0,1);
	LLVMValueRef cond = LLVMBuildICmp(Builder,LLVMIntNE,$3,zero,"cond");
	LLVMBuildCondBr(Builder,cond,then,ielse);
	LLVMPositionBuilderAtEnd(Builder,then);
	$<bb>$ = ielse;
	//$<bb>$ = join;
	
}
RPAREN statement 
{
	LLVMBasicBlockRef join = LLVMAppendBasicBlock(Function,"join.block");
	$<bb>$ = join;
	LLVMBasicBlockRef ielse = $<bb>4;
	LLVMBuildBr(Builder,join);
	LLVMPositionBuilderAtEnd(Builder,ielse); 
}
ELSE statement
{
	LLVMBasicBlockRef join = $<bb>7;
	LLVMBuildBr(Builder,join);
	LLVMPositionBuilderAtEnd(Builder,join);
}
;

iteration_stmt:WHILE LPAREN { 
  	/* set up header basic block
     	make it the new insertion point */
	printf("In while\n");
	LLVMBasicBlockRef cond = LLVMAppendBasicBlock(Function,"while.cond");
	LLVMBuildBr(Builder,cond);
	LLVMPositionBuilderAtEnd(Builder,cond);
	$<bb>$ = cond;

} expression RPAREN { 
  /* set up loop body */
	LLVMBasicBlockRef body = LLVMAppendBasicBlock(Function,"while.body");
  /* create new body and exit blocks */
	LLVMBasicBlockRef exit = LLVMAppendBasicBlock(Function,"while.exit");
	LLVMValueRef zero = LLVMConstInt(LLVMTypeOf($4),0,1);
	LLVMValueRef cond = LLVMBuildICmp(Builder,LLVMIntNE,$4,zero,"cond");
	LLVMValueRef br = LLVMBuildCondBr(Builder,cond,body,exit);
	LLVMPositionBuilderAtEnd(Builder,body);
  /* to support nesting:*/ 
  	push_loop($<bb>3,body,body,exit);
	$<bb>$ = exit;
} 
  statement
{
  /*finish loop */
  	loop_info_t info = get_loop();
  	//pop_loop();
	LLVMBuildBr(Builder,$<bb>3);
	LLVMPositionBuilderAtEnd(Builder,$<bb>6);
	pop_loop();
}
| FOR LPAREN expr_opt 
{
  // build a new basic block for the cond expression (LLVMAppendBasicBlock)
	LLVMBasicBlockRef forcond = LLVMAppendBasicBlock(Function,"for.cond");
  // remember the cond block for this for loop (stack of nested loops)
	$<bb>$ = forcond;
  // insert a branch from the current basic to the cond basic block (LLVMBuildBr)
	LLVMBuildBr(Builder,forcond);
  // set builder to insert inside the cond block LLVMPositionBuilderAtEnd(Builder,cond block)
	LLVMPositionBuilderAtEnd(Builder,forcond);
  //$$ = initblock;
 } 
SEMICOLON expr_opt 
{
  // build a new block
	LLVMBasicBlockRef forinc = LLVMAppendBasicBlock(Function,"for.inc");
	LLVMBasicBlockRef forbody = LLVMAppendBasicBlock(Function,"for.body");
	LLVMBasicBlockRef forexit = LLVMAppendBasicBlock(Function,"for.exit");
  // position builder in this block
	//LLVMPositionBuilderAtEnd(Builder,forinc);
  // add the branch back to the cond block	
	LLVMValueRef zero = LLVMConstInt(LLVMTypeOf($6),0,1);
        LLVMValueRef cond = LLVMBuildICmp(Builder,LLVMIntNE,$6,zero,"cond");
	LLVMBuildCondBr(Builder,cond,forbody,forexit);
	//LLVMPositionBuilderAtEnd(Builder,forinc);
	$<bb>$ = forbody;
  //$$ = condblock;
	// to remember the forinc basic block
	push_loop($<bb>4,forbody,forinc,forexit);
	//if($6 == zero)
	//pop_loop();
	LLVMPositionBuilderAtEnd(Builder,forinc);
} 
SEMICOLON expr_opt 
{
  // build a new block for the beginning of the statement 
	// positioning builder in the body block
	LLVMPositionBuilderAtEnd(Builder,$<bb>7);
	// transferring from body to condition block
	LLVMBuildBr(Builder,$<bb>4);
  // add a branch from the cond block to the statement block
  //LLVMPositionBuilder(Builder,$6);
  //stateblock = LLVMAppendBasicBlock(Function,"for-statement");
  //LLVMBuildBr(Builder,statementblock);
  // set insert piont int the new statement block  
}
RPAREN statement
{
	loop_info_t temp;
	temp = get_loop();
	LLVMBuildBr(Builder,temp.reinit);
	LLVMPositionBuilderAtEnd(Builder,temp.exit);
        pop_loop();
  /* 566: add mid-rule actions to support for loop */
  // connect current block ($12) the re-init block 
}
;

expr_opt:		
{ 

}
|expression
{ 
	printf("In expr_opt:expression\n");
	$$ = $1;
}
;

jump_stmt:RETURN SEMICOLON
{ 
	printf("In jump_stmt:RETURN SEMICOLN\n");
      	LLVMBuildRetVoid(Builder);
	LLVMBasicBlockRef new = LLVMAppendBasicBlock(Function,"new");
	LLVMPositionBuilderAtEnd(Builder,new);

}
|RETURN expression SEMICOLON
{
	printf("In jump_stmt:RETURN expression SEMICOLON\n");
  	LLVMBuildRet(Builder,$2);
	LLVMBasicBlockRef new = LLVMAppendBasicBlock(Function,"new");
        LLVMPositionBuilderAtEnd(Builder,new);
}
;

expression:assignment_expression
{ 
	//code added
	printf("In expression:assignment_expression\n");
  	//$$=$1;
}
;

assignment_expression:conditional_expression
{
	printf("In assignment_expression:conditional_expression\n");
  	$$=$1;
}
|lhs_expression ASSIGN assignment_expression
{
  /* Implement */
	char *string1 = "i32**";
	char *string2 = "i32*";
	char *str = LLVMPrintTypeToString(LLVMTypeOf($1));
	 char *str1 = LLVMPrintTypeToString(LLVMTypeOf($3));
	printf("%s\n",str);
	int val = strcmp(str,string2);
	printf("In lhs_expression ASSIGN assignment_expression\n");
	//*string1 = LLVMPrintTypeToString(LLVMTypeOf($1));
	printf("%d\n",strcmp((LLVMPrintTypeToString(LLVMTypeOf($1))),string1));
	//printf("%s\n",LLVMPrintTypeToString(LLVMPointerType((LLVMTypeOf($3)),0)));
        printf("%s\n",LLVMPrintTypeToString(LLVMTypeOf($1)));
	printf("%s\n",LLVMPrintTypeToString(LLVMTypeOf($3)));
	printf("%s\n",LLVMPrintTypeToString(LLVMInt32TypeInContext(Context)));
	printf("%s\n",LLVMPrintTypeToString(LLVMPointerType(LLVMInt32TypeInContext(Context),0)));
	if(strcmp((LLVMPrintTypeToString(LLVMTypeOf($1))),string1)==0)
	{
		if(LLVMIsAConstantInt($3))
		{
			if(LLVMConstIntGetSExtValue($3)==0)
			{
				printf("HERE\n");
				//LLVMConstPointerNull (LLVMTypeRef Ty)
				$$ = LLVMBuildStore(Builder,(LLVMConstPointerNull(LLVMPointerType(LLVMInt32TypeInContext(Context),0))),$1);
				//$$ = LLVMBuildStore(Builder,(LLVMConstPointerNull(LLVMInt32TypeInContext(Context))),$1);
				//$$ = LLVMConstNull(LLVMTypeOf($1));
				printf("generated null pointer\n");
			}
		}
		else if(strcmp((LLVMPrintTypeToString(LLVMTypeOf($3))),string1)==0)
		$$ = LLVMBuildStore(Builder,$3,$1);
		else if(strcmp((LLVMPrintTypeToString(LLVMTypeOf($3))),string2)==0)
		$$ = LLVMBuildStore(Builder,$3,$1);
		else 
		return(parser_error("illegal assignment\n"));
	//		if(LLVMConstIntGetSExtValue($3)==0)
	} 
	//printf("%s\n",LLVMPrintTypeToString(LLVMTypeOf($1)));
	else if((strcmp((LLVMPrintTypeToString(LLVMTypeOf($1))),string2)==0) && (strcmp((LLVMPrintTypeToString(LLVMTypeOf($3))),string1)==0))
	return(parser_error("illegal assignment\n"));
	else
	{
		if((strcmp(str1,string2))==0)
		return(internal_error("ILLEGAL\n"));
		else
		{	
			printf("In non pointer assignment\n");
			$$ = LLVMBuildStore(Builder,$3,$1);
		}
	}
}
;

conditional_expression:logical_OR_expression
{
	printf("In conditional_expression:logical_OR_expression\n");
  	$$=$1;
}
|logical_OR_expression QUESTION_MARK
{
  /* Implement */
	printf("%s\n",LLVMPrintTypeToString(LLVMTypeOf($1)));
        //printf("%s\n",LLVMPrintTypeToString(LLVMTypeOf($3)));
	printf("In question mark\n");
//	phi_val = LLVMBuildPhi(Builder,LLVMTypeOf($1),"");
	LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(Context),0,0);
	LLVMValueRef cond = LLVMBuildICmp(Builder,LLVMIntNE,$1,zero,"");
	//LLVMValueRef temp1 = LLVMBuildICmp(Builder,LLVMIntEQ,$1,one,"");
	LLVMBasicBlockRef Qif = LLVMAppendBasicBlock(Function,"Question.true");
	LLVMBasicBlockRef Qelse = LLVMAppendBasicBlock(Function,"Question.false");
	LLVMBasicBlockRef Qjoin = LLVMAppendBasicBlock(Function,"Question.join");
	LLVMBuildCondBr(Builder,cond,Qif,Qelse);
	LLVMPositionBuilderAtEnd(Builder,Qif);
	push_loop(Qif,Qif,Qelse,Qjoin);
}
expression COLON
{
	LLVMValueRef temp1 = $4;
	incoming[0] = temp1;
	loop_info_t temp;
        temp = get_loop();
	LLVMBuildBr(Builder,temp.exit);
	LLVMPositionBuilderAtEnd(Builder,temp.reinit);
}
conditional_expression
{
	LLVMValueRef temp2 = $7;
	incoming[1] = temp2;
	loop_info_t temp;
        temp = get_loop();
	LLVMBasicBlockRef BasicBlocks[2] = {temp.expr,temp.reinit};
        //LLVMAddIncoming(phi_val,incoming,BasicBlocks,2);
	LLVMBuildBr(Builder,temp.exit);
	LLVMPositionBuilderAtEnd(Builder,temp.exit);
	phi_val = LLVMBuildPhi(Builder,LLVMTypeOf($1),"");
	LLVMAddIncoming(phi_val,incoming,BasicBlocks,2);
	$$ = phi_val;
	pop_loop();
}
;

constant_expression:conditional_expression
{ 	
	printf("In constant_expression:conditional_expression\n")
	$$ = $1; }
;

logical_OR_expression:logical_AND_expression
{
	printf("In logical_OR_expression:logical_AND_expression\n");
  	$$ = $1;
}
|logical_OR_expression LOGICAL_OR logical_AND_expression
{
  /* Implement */
	printf("In logical OR\n");
        LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(Context),0,0);
        //printf("HERE\n");
	
        LLVMValueRef temp1 = LLVMBuildZExt(Builder,(LLVMBuildICmp(Builder,LLVMIntNE,$1,zero,"")),LLVMInt32TypeInContext(Context),"");
        LLVMValueRef temp2 =LLVMBuildZExt(Builder,(LLVMBuildICmp(Builder,LLVMIntNE,$3,zero,"")),LLVMInt32TypeInContext(Context),"");
        //printf("HERE\n");
        //printf("HERE\n");
        $$ = LLVMBuildOr(Builder,temp1,temp2,"");
};

logical_AND_expression:inclusive_OR_expression
{
  	printf("In logical_AND_expression:inclusive_OR_expression\n");
	$$ = $1;
}
|logical_AND_expression LOGICAL_AND inclusive_OR_expression
{
  /* Implement */
	printf("In logical And\n");
	LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(Context),0,0);
	printf("HERE\n");
	LLVMValueRef temp1 = LLVMBuildZExt(Builder,(LLVMBuildICmp(Builder,LLVMIntNE,$1,zero,"")),LLVMInt32TypeInContext(Context),"");
        LLVMValueRef temp2 =LLVMBuildZExt(Builder,(LLVMBuildICmp(Builder,LLVMIntNE,$3,zero,"")),LLVMInt32TypeInContext(Context),"");
	printf("HERE\n");
	printf("HERE\n");
	$$ = LLVMBuildAnd(Builder,temp1,temp2,"");
}
;

inclusive_OR_expression:exclusive_OR_expression
{
	printf("In inclusive_OR_expression:exclusive_OR_expression\n");
    	$$=$1;
}
|inclusive_OR_expression BITWISE_OR exclusive_OR_expression
{
  /* Implement */
	printf("In: inclusive_OR_expression BITWISE_OR exclusive_OR_expression\n");
	$$ = LLVMBuildOr(Builder,$1,$3,"");
}
;

exclusive_OR_expression:  AND_expression
{
	printf("In exclusive_OR_expression:AND_expression\n");
  	$$ = $1;
}
|exclusive_OR_expression BITWISE_XOR AND_expression
{
  /* Implement */
	printf("In: exclusive_OR_expression BITWISE_XOR AND_expression\n");
	$$ = LLVMBuildXor(Builder,$1,$3,"");
}
;

AND_expression:equality_expression
{
	printf("In AND_expression:equality_expression\n");
  	$$ = $1;
}
|AND_expression AMPERSAND equality_expression
{
  /* Implement */
	printf("In AND_expression AMPERSAND equality_expression\n");
	$$ = LLVMBuildAnd(Builder,$1,$3,"");
}
;

equality_expression:relational_expression
{
	printf("In equality_expression:relational_expression\n");
  	$$ = $1;
}
|equality_expression EQ relational_expression
{
  /* Implement: use icmp */
	$$ = LLVMBuildZExt(Builder,(LLVMBuildICmp(Builder,LLVMIntEQ,$1,$3,"")),LLVMInt32TypeInContext(Context),"");
}
|equality_expression NEQ relational_expression
{
  /* Implement: use icmp */
	LLVMBuildZExt(Builder,(LLVMBuildICmp(Builder,LLVMIntNE,$1,$3,"")),LLVMInt32TypeInContext(Context),"");
	//$$ = LLVMBuildICmp(Builder,LLVMIntNE,$1,$3,"");
}
;

relational_expression:shift_expression
{
	printf("In relational_expression:shift_expression\n");
    	$$=$1;
}
|relational_expression LT shift_expression
{
  /* Implement: use icmp */
	printf("In LESS THAN\n");
	$$ = LLVMBuildZExt(Builder,(LLVMBuildICmp(Builder,LLVMIntSLT,$1,$3,"")),LLVMInt32TypeInContext(Context),"");
}
|relational_expression GT shift_expression
{
  /* Implement: use icmp */
	printf("In GREATER THAN\n");
	$$ = LLVMBuildZExt(Builder,(LLVMBuildICmp(Builder,LLVMIntSGT,$1,$3,"")),LLVMInt32TypeInContext(Context),"");
}
|relational_expression LTE shift_expression
{
  /* Implement: use icmp */
	printf("In LESS THAN OR EQUAL TO\n");
        $$ = LLVMBuildZExt(Builder,(LLVMBuildICmp(Builder,LLVMIntSLE,$1,$3,"")),LLVMInt32TypeInContext(Context),"");

}
|relational_expression GTE shift_expression
{
  /* Implement: use icmp */
	printf("In GREATER THAN OR EQUAL TO\n");
        $$ = LLVMBuildZExt(Builder,(LLVMBuildICmp(Builder,LLVMIntSGE,$1,$3,"")),LLVMInt32TypeInContext(Context),"");
}
;

shift_expression:additive_expression
{
	printf("In shift_expression:additive_expression\n");
    	$$=$1;
}
|shift_expression LSHIFT additive_expression
{
  /* Implement */
	$$ = LLVMBuildShl(Builder,$1,$3,"");
}
|shift_expression RSHIFT additive_expression
{
  /* Implement */
	$$ = LLVMBuildAShr(Builder,$1,$3,"");
}
;

additive_expression:multiplicative_expression
{
	printf("In additive_expression:multiplicative_expression\n");
  	$$ = $1;
}
|additive_expression PLUS multiplicative_expression
{
  /* Implement */
	char *string = "i32*";
	printf("In additive_expression PLUS multiplicative_expression\n");
	//printf("%s\n",LLVMPrintTypeToString(LLVMTypeOf($1)));
	//printf("%d\n",(strcmp(LLVMPrintTypeToString(LLVMTypeOf($1))),string));
        //printf("%d\n",(strcmp(LLVMPrintTypeToString(LLVMTypeOf($1))),string));
	//if((strcmp(LLVMPrintTypeToString(LLVMTypeOf($1)),string))==0 && LLVMIsAConstantInt($3))
        //{
                //LLVMValueRef temp = LLVMBuildNeg(Builder,$3,"");
         //       LLVMValueRef indices[1] = {LLVMConstInt(LLVMInt32TypeInContext(Context),LLVMConstIntGetSExtValue($3),1)};
	//	printf("Here\n");
          //      $$ = LLVMBuildGEP(Builder,$1,indices,1,"");
        //}
        //else
        //$$ = LLVMBuildSub(Builder,$1,$3,"");
	//printf("In additive_expression PLUS multiplicative_expression\n");
	//printf("%s\n",LLVMPrintTypeToString(LLVMTypeOf($1)));
	char* str = LLVMPrintTypeToString(LLVMTypeOf($1));
	int x = strcmp(str,string);
	printf("%d\n",x);
	//printf("%d\n",(strcmp(LLVMPrintTypeToString(LLVMTypeOf($1))),string));
	if(x==0)
	{
		printf("LHS is a pointer\n");	
		if(LLVMIsAConstantInt($3))
		{
			LLVMValueRef indices[1] = {LLVMConstInt(LLVMInt32TypeInContext(Context),LLVMConstIntGetSExtValue($3), 1)};
			$$ = LLVMBuildGEP(Builder,$1,indices,1,"");
		}
		else
		{
			printf("Here\n");
			return(internal_error("illegal assignment to pointer\n"));
		}
	}
	else
		$$ = LLVMBuildAdd(Builder,$1,$3,"");
}
|additive_expression MINUS multiplicative_expression
{
  /* Implement */
	char* string = "i32*";
	printf("In additive_expression MINUS multiplicative_expression\n");
	char* str = LLVMPrintTypeToString(LLVMTypeOf($1));
        int x = strcmp(str,string);
        printf("%d\n",x);
        //printf("%d\n",(strcmp(LLVMPrintTypeToString(LLVMTypeOf($1))),string));
        if(x==0)
        {
                printf("LHS is a pointer\n");
                if(LLVMIsAConstantInt($3))
                {
			 LLVMValueRef temp = LLVMBuildNeg(Builder,$3,"");
                        LLVMValueRef indices[1] = {LLVMConstInt(LLVMInt32TypeInContext(Context),LLVMConstIntGetSExtValue(temp), 1)};
                        $$ = LLVMBuildGEP(Builder,$1,indices,1,"");
                }
                else
                {
                        printf("Here\n");
                        return(internal_error("illegal assignment to pointer\n"));
                }
        }
        else
                $$ = LLVMBuildSub(Builder,$1,$3,"");
}
;

multiplicative_expression:  cast_expression
{
	printf("In multiplicative_expression:cast_expression\n");
  	$$ = $1;
}
|multiplicative_expression STAR cast_expression
{
	/* Implement */
	char* string ="i32*"; 
	printf("In multiplicative_expression: multiplicative_expression STAR cast_expression\n");
	printf("%s\n",(LLVMPrintTypeToString(LLVMTypeOf($1))));
	printf("%d\n",(strcmp(LLVMPrintTypeToString(LLVMTypeOf($1))),string));
	if((strcmp((LLVMPrintTypeToString(LLVMTypeOf($1))),string)==0) || (strcmp((LLVMPrintTypeToString(LLVMTypeOf($1))),string)==0))
	return(internal_error("Pointer Multiplication not allowed!!\n"));
	
	$$ = LLVMBuildMul(Builder,$1,$3,"");
}
|multiplicative_expression DIV cast_expression
{
  /* Implement */
		char* string = "i32*";
	 printf("In multiplicative_expression: multiplicative_expression DIV cast_expression\n");
	 if((strcmp((LLVMPrintTypeToString(LLVMTypeOf($1))),string)==0) || (strcmp((LLVMPrintTypeToString(LLVMTypeOf($1))),string)==0))
        return(internal_error("Pointer Division not allowed!!\n"));
	if(LLVMIsAConstantInt($3))
	{
	//printf("Here!!\n");
	 if(LLVMConstIntGetSExtValue($3)==0)
                {
                        return (internal_error("Divider cannot be 0!!\n"));
                        //yyerror("Divider cannot be 0\n");
                }
	}
	//printf("In multiplicative_expression: multiplicative_expression DIV cast_expression\n");
	$$ = LLVMBuildSDiv(Builder,$1,$3,"");
}
|multiplicative_expression MOD cast_expression
{
  /* Implement */
	char* string = "i32*";
	printf("In multiplicative_expression MOD cast_expression\n");
	if((strcmp((LLVMPrintTypeToString(LLVMTypeOf($1))),string)==0) || (strcmp((LLVMPrintTypeToString(LLVMTypeOf($1))),string)==0))
        return(internal_error("Pointer Modulus Illegal!!\n"));
	$$ = LLVMBuildSRem(Builder,$1,$3,"");
}
;

cast_expression:unary_expression
{ 
	printf("In cast_expression:unary_expression\n");
	$$ = $1; }
;

lhs_expression:ID 
{
	printf("In lhs_expression:ID\n");
  	int isArg=0;
  	LLVMValueRef val = symbol_find($1,&isArg);
  	if (isArg)
    	{
      	// error 
    	}
  	else
    	$$ = val;
}
|STAR ID
{
  	printf("In lhs_expression:STAR ID\n");
	int isArg=0;
  	LLVMValueRef val = symbol_find($2,&isArg);
  	if (isArg)
    	{
      	// error
    	}
  	else
    	$$ = LLVMBuildLoad(Builder,val,"");
}
;

unary_expression:postfix_expression
{
	printf("In unary_expression:postfix_expression\n");
  	$$ = $1;
}
|AMPERSAND primary_expression
{
	printf("In: AMPERSAND primary_expression\n");
  /* Implement */
	char* string = "i32*";
        char* str = LLVMPrintTypeToString(LLVMTypeOf($2));
	printf("%s\n",LLVMPrintTypeToString(LLVMTypeOf($2)));
        int x = strcmp(str,string);
	printf("%d\n",x);
	if(x == 0)
        return(internal_error("ERROR!\n"));
	if ( LLVMIsALoadInst($2) ) 
	{
 		 $$ = LLVMGetOperand($2,0);
	}
	else if(x == 0)
	return(internal_error("ERROR!\n"));
	else
	return(internal_error("Zero ERROR!\n"));
	//$$ = LLVMBuildLoad(Builder,$2,"");
}
|STAR primary_expression
{
  /* FIXME */
	char* string = "i32*";
	char* str = LLVMPrintTypeToString(LLVMTypeOf($2));
        int x = strcmp(str,string);
	if(x==0)
	{
	printf("In STAR primary_expression\n");
	$$ = LLVMBuildLoad(Builder,$2,"");
	}
	else
	return(internal_error("ERROR in derefrencing variable!\n"));
}
|MINUS unary_expression
{
  /* Implement */
	printf("inside MINUS unary_expression\n");
        $$ = LLVMBuildNeg(Builder,$2,"");
}
|PLUS unary_expression
{
  	$$ = $2;
}
|BITWISE_INVERT unary_expression
{
  /* Implement */
	$$ = LLVMBuildNot(Builder,$2,"");
}
|NOT unary_expression
{
  /* Implement */
	LLVMValueRef zero = LLVMConstInt(LLVMTypeOf($2),0,1); 
  	LLVMValueRef icmp = LLVMBuildICmp(Builder, LLVMIntEQ, $2,
                                  zero,"logical.neg");
  	$$ = LLVMBuildZExt(Builder, icmp,LLVMInt32TypeInContext(Context),"logical.neg");
}
;

postfix_expression:primary_expression
{
	printf("In postfix_expression:primary_expression\n");
  	$$ = $1;
}
;

primary_expression:ID 
{ 
  	printf("In primary expression:ID\n");
	int isArg=0;
  	LLVMValueRef val = symbol_find($1,&isArg);
  	if (isArg)
    	$$ = val;
  	else
    	$$ = LLVMBuildLoad(Builder,val,"");
}
|constant
{
	printf("In constant\n");
  	$$ = $1;
}
|LPAREN expression RPAREN
{
	printf("In LPAREN expression LPAREN\n");
  	$$ = $2;
}
;

constant:NUMBER  
{ 
  /* Implement */
	$$ = LLVMConstInt(LLVMInt32TypeInContext(Context),$1,0);
} 
;

%%

LLVMValueRef BuildFunction(LLVMTypeRef RetType, const char *name, 
			   paramlist_t *params)
{
  int i;
  int size = paramlist_size(params);
  LLVMTypeRef *ParamArray = malloc(sizeof(LLVMTypeRef)*size);
  LLVMTypeRef FunType;
  LLVMBasicBlockRef BasicBlock;

  paramlist_t *tmp = params;
  /* Build type for function */
  for(i=size-1; i>=0; i--) 
    {
      ParamArray[i] = tmp->type;
      tmp = next_param(tmp);
    }
  
  FunType = LLVMFunctionType(RetType,ParamArray,size,0);

  Function = LLVMAddFunction(Module,name,FunType);
  
  /* Add a new entry basic block to the function */
  BasicBlock = LLVMAppendBasicBlock(Function,"entry");

  /* Create an instruction builder class */
  Builder = LLVMCreateBuilder();

  /* Insert new instruction at the end of entry block */
  LLVMPositionBuilderAtEnd(Builder,BasicBlock);

  tmp = params;
  for(i=size-1; i>=0; i--)
    {
      LLVMValueRef alloca = LLVMBuildAlloca(Builder,tmp->type,tmp->name);
      LLVMBuildStore(Builder,LLVMGetParam(Function,i),alloca);
      symbol_insert(tmp->name,alloca,0);
      tmp=next_param(tmp);
    }

  return Function;
}

extern int line_num;
extern char *infile[];
static int   infile_cnt=0;
extern FILE * yyin;

int parser_error(const char *msg)
{
  printf("%s (%d): Error -- %s\n",infile[infile_cnt-1],line_num,msg);
  return 1;
}

int internal_error(const char *msg)
{
  printf("%s (%d): Internal Error -- %s\n",infile[infile_cnt-1],line_num,msg);
  return 1;
}

int yywrap() {
  static FILE * currentFile = NULL;

  if ( (currentFile != 0) ) {
    fclose(yyin);
  }
  
  if(infile[infile_cnt]==NULL)
    return 1;

  currentFile = fopen(infile[infile_cnt],"r");
  if(currentFile!=NULL)
    yyin = currentFile;
  else
    printf("Could not open file: %s",infile[infile_cnt]);

  infile_cnt++;
  
  return (currentFile)?0:1;
}

int yyerror()
{
  parser_error("Un-resolved syntax error.");
  return 1;
}

char * get_filename()
{
  return infile[infile_cnt-1];
}

int get_lineno()
{
  return line_num;
}


void minic_abort()
{
  parser_error("Too many errors to continue.");
  exit(1);
}
