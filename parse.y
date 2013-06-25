//
//
// yacc input for parser for cs520 assembler
//
//

%{
#include <stddef.h>
#include <stdio.h>
#include "defs.h"

extern unsigned int parseErrorCount;

// scanner produced by flex
int yylex(void);

// forward reference
void yyerror(char *s);

%}

//
//      this types the semantic stack
//
%union  {
        char *       y_str;
        unsigned int y_reg;
        int          y_int;
        INSTR        y_instr;
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

//
//      typed non-terminal symbols
//
%type         <y_str>        opcode
%type         <y_str>        label
%type         <y_instr>      instruction

%%

program
        : stmt stmt_list
        ;

stmt_list
        : // null derive

        | stmt stmt_list

        ;

stmt
        : label instruction EOL
          {
             assemble($1, $2);
          }
        | instruction EOL
          {
             assemble(NULL, $1);
          }
        | label EOL
          {
             INSTR nullInstr;
             nullInstr.format = 0;
             assemble($1, nullInstr);
          }
        | EOL
          {
             // no action
          }
        | error EOL
          {
             // error recovery - sync with end-of-line
          }
        ;

label
        : ID COLON
          {
             $$ = $1;
          }
        ;

instruction
        : opcode
          {
             $$.format = 1;
             $$.opcode = $1;
          }
        |
          opcode ID
          {
             $$.format = 2;
             $$.opcode = $1;
             $$.u.format2.addr = $2;
          }
        |
          opcode REG
          {
             $$.format = 3;
             $$.opcode = $1;
             $$.u.format3.reg = $2;
          }
        |
          opcode REG COMMA INT_CONST
          {
             $$.format = 4;
             $$.opcode = $1;
             $$.u.format4.reg = $2;
             $$.u.format4.constant = $4;
          }
        |
          opcode REG COMMA ID
          {
             $$.format = 5;
             $$.opcode = $1;
             $$.u.format5.reg = $2;
             $$.u.format5.addr = $4;
          }
        |
          opcode REG COMMA REG
          {
             $$.format = 6;
             $$.opcode = $1;
             $$.u.format6.reg1 = $2;
             $$.u.format6.reg2 = $4;
          }
        |
          opcode REG COMMA INT_CONST LPAREN REG RPAREN
          {
             $$.format = 7;
             $$.opcode = $1;
             $$.u.format7.reg1 = $2;
             $$.u.format7.offset = $4;
             $$.u.format7.reg2 = $6;
          }
        |
          opcode REG COMMA REG COMMA ID
          {
             $$.format = 8;
             $$.opcode = $1;
             $$.u.format8.reg1 = $2;
             $$.u.format8.reg2 = $4;
             $$.u.format8.addr = $6;
          }
        |
          opcode INT_CONST
          {
             $$.format = 9;
             $$.opcode = $1;
             $$.u.format9.constant = $2;
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
