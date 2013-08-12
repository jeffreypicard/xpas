//
//
// yacc input for parser for cs520 assembler
//
//

%{
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "defs.h"

extern unsigned int parseErrorCount;

int yydebug=1;

// scanner produced by flex
int yylex(void);

// forward reference
void yyerror(char *s);

%}

//
//      this types the semantic stack
//
%union  {
        char          *y_str;
        unsigned int  y_reg;
        int           y_int;
        INSTR         *y_instr;
        handler_node  *y_handle;
        stmt_node     *y_stmt;
        func_node     *y_func;
        }

//
//        terminal symbols
//
%token <y_str> ID
%token <y_int> INT_CONST
%token <y_reg> REG
%token EOL
%token COLON
%token COMMA
%token LPAREN
%token RPAREN
%token FUNC
%token END
%token EXCEPTION

//
//      typed non-terminal symbols
//
%type         <y_str>        opcode
%type         <y_str>        label
%type         <y_instr>      instruction
%type         <y_handle>     handler
%type         <y_handle>     handler_list
%type         <y_stmt>       stmt
%type         <y_stmt>       stmt_list
%type         <y_func>       func
%type         <y_func>       func_list

%%

program
        : func_list
        {
          if ( $1 )
          {
            func_list = $1;
            verify_handlers( func_list );
          }
        }
        ;

func_list
        : /* null derive */
        {
          $$ = NULL;
        }
        | func func_list
        {
          $$ = process_func_list( $1, $2 );
        }
        ;

func
        : FUNC ID handler_list stmt_list END ID 
          {
            $$ = process_func( $2, $6, $3, $4 );
            if ($$) {
              $$->native_ref_list = native_ref_list;
              $$->num_native_refs = native_ref_list_length(native_ref_list);
            }
            /* Reset global variable to NULL, which was filled in
             * as native references were found during instruction parsing. */
            native_ref_list = NULL;
          }
        ;

handler_list
        : /* null derive */
          {
            $$ = NULL;
          }
        | handler handler_list
          {
            $$ = process_handler_list( $1, $2 );
          }
        ;

handler
        : EXCEPTION ID COMMA ID COMMA ID
          {
            $$ = process_handler( $2, $4, $6 );
          }
        ;

stmt_list
        : // null derive
          {
            $$ = NULL;
          }
        | stmt stmt_list
          {
            $$ = process_stmt_list( $1, $2 );
          }
        ;

stmt
        /*: label instruction
          {
            $$ = process_stmt( $1, $2 );
            // assemble($1, $2);
          }*/
        : instruction
          {
            $$ = process_stmt( NULL, $1 );
            // assemble(NULL, $1);
          }
        | label
          {
             //INSTR nullInstr;
             //nullInstr.format = 0;
             //assemble($1, nullInstr);
             INSTR *null_instr = calloc( 1, sizeof *null_instr );
             null_instr->format = 0;
             $$ = process_stmt( $1, null_instr );
          }
        | error
          {
             $$ = NULL;// error recovery - sync with end-of-line
          }
        ;

label
        : ID COLON
          {
             $$ = $1;
          }
        ;

instruction
        /*: opcode
          {
            $$ = calloc( 1, sizeof(INSTR) );
            $$->format = 1;
            $$->opcode = $1;
          }*/
        :
          opcode ID
          {
            $$ = calloc( 1, sizeof(INSTR) );
            $$->format = 2;
            $$->opcode = $1;
            $$->u.format2.addr = $2;
          }
        |
          opcode REG
          {
            $$ = calloc( 1, sizeof(INSTR) );
            $$->format = 3;
            $$->opcode = $1;
            $$->u.format3.reg = $2;
          }
        |
          opcode REG COMMA INT_CONST
          {
            $$ = calloc( 1, sizeof(INSTR) );
            $$->format = 4;
            $$->opcode = $1;
            $$->u.format4.reg = $2;
            $$->u.format4.constant = $4;
          }
        |
          opcode REG COMMA ID
          {
            $$ = calloc( 1, sizeof(INSTR) );
            $$->format = 5;
            $$->opcode = $1;
            $$->u.format5.reg = $2;
            $$->u.format5.addr = $4;
          }
        |
          opcode REG COMMA REG
          {
            $$ = calloc( 1, sizeof(INSTR) );
            $$->format = 6;
            $$->opcode = $1;
            $$->u.format6.reg1 = $2;
            $$->u.format6.reg2 = $4;
          }
        |
          opcode REG COMMA REG COMMA INT_CONST
          {
            $$ = calloc( 1, sizeof(INSTR) );
            $$->format = 7;
            $$->opcode = $1;
            $$->u.format7.reg1 = $2;
            $$->u.format7.reg2 = $4;
            $$->u.format7.const8 = $6;
          }
        |
          opcode REG COMMA REG COMMA ID
          {
            $$ = calloc( 1, sizeof(INSTR) );
            $$->format = 8;
            $$->opcode = $1;
            $$->u.format8.reg1 = $2;
            $$->u.format8.reg2 = $4;
            $$->u.format8.addr = $6;
          }
        |
          opcode INT_CONST
          {
            $$ = calloc( 1, sizeof(INSTR) );
            $$->format = 9;
            $$->opcode = $1;
            $$->u.format9.constant = $2;
          }
        |
          opcode REG COMMA REG COMMA REG
          {
            $$ = calloc( 1, sizeof(INSTR) );
            $$->format = 10;
            $$->opcode = $1;
            $$->u.format10.reg1 = $2;
            $$->u.format10.reg2 = $4;
            $$->u.format10.reg3 = $6;
          }
        ;

opcode
        : ID
          {
             $$ = $1;
          }
        ;

%%

// yyerror
//
// yacc created parser will call this when syntax error occurs
// (to get line number right we must call special "message" routine)
//
void yyerror(char *s)
{
  parseErrorCount += 1;
  parseError(s); 
}
