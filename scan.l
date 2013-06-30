%{
//
// lex input for scanner for cs520 assembler
//
//

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include "defs.h"
#include "y.tab.h"

#ifndef DEBUG
extern unsigned int scanErrorCount;
#else
unsigned int scanErrorCount = 0;
#endif

#undef yywrap
int yywrap (void);

// forward references
static char * stashStr(char*);
static unsigned int getRegNum(char*);
static int a2int(char *tptr);

#ifdef        DEBUG
        main()
        {
                char *p;

                while (p = (char *) yylex())
                        printf("%-10.10s is \"%s\"\n",p,yytext);
        }
#        define token(x)        (int) # x
        YYSTYPE yylval;

#else

#        define token(x) x

#endif

%}

%option nounput
%option noinput

letter                    [a-zA-Z]

digit                     [0-9]

hexdigit                  [0-9A-Fa-f]

whitespace                [ \t]

newline                   [\n]

register                  (([r][0-9])|([r][1][0-5])|(sp)|(fp)|(pc))

id                        ({letter}({letter}|{digit})*)

int_const                 (({digit}+)|(-({digit}+)))

hex_int_const             (0x(({hexdigit}+)|(-({hexdigit}+))))

comment                   [#](.)*[\n]

other                     .

%%

"func"                    return token(FUNC);

"end"                     return token(END);

"exception"               return token(EXCEPTION);

"("                       return token(LPAREN);

")"                       return token(RPAREN);

":"                       return token(COLON);

","                       return token(COMMA);

{register}                {
                            yylval.y_reg = getRegNum(yytext); 
                            return token(REG);
                          }

{id}                      { 
                            yylval.y_str = stashStr(yytext); 
                            return token(ID);
                          }

{int_const}               { 
                            yylval.y_int = a2int(yytext); 
                            return token(INT_CONST); 
                          }

{hex_int_const}           { 
                            yylval.y_int = a2int(yytext); 
                            return token(INT_CONST); 
                          }

{whitespace}+             ;

{newline}                 {
                            yylineno++;
                            //return token(EOL);
                          }

{comment}                 {
                            yylineno++;
                            //return token(EOL);
                          }

{other}                   return token(yytext[0]);

%%

// stashStr
//
// copy token string to safe place; return addr of safe place
//
static
char * stashStr(char *s)
{
  char *ret;
  int len;

  len = strlen(s);
  ret = (char *) malloc(len+1);
  if (ret == NULL)
  {
    fatal("out of memory in stash_str");
  }
  strcpy(ret, s);
  return ret;
}

// getRegNum
//
// convert register as string to its integer encoding
//
static
unsigned int getRegNum(char *s)
{
  // first handle the three special cases (fp, sp, pc)
  if (strcmp(s, "fp") == 0)
  {
    return 13;
  }
  if (strcmp(s, "sp") == 0)
  {
    return 14;
  }
  if (strcmp(s, "pc") == 0)
  {
    return 15;
  }

  // now handle the remaining two character cases (r0, ..., r9)
  if (strlen(s) == 2)
  {
    return s[1] - '0';
  }

  // finally handle the three character cases (r10, ..., r15)
  if (strlen(s) == 3)
  {
    return (s[2] - '0') + 10;
  }

  bug("getRegNum reaches end of function");
  return 0;
}

//
// Convert from ascii hex or decimal to an integer.
//
static int a2int(char *tptr)
{
  unsigned long long unsigned_long_long_tmp;
  int int_tmp;
  unsigned long long unsigned_long_long_tmp2;

  // errno used to detect overflow of long long
  errno = 0;
  unsigned_long_long_tmp = strtoull(tptr, NULL, 0);
  if (errno)
  {
    scanErrorCount += 1;
    error("integer constant too large");
    return 1;
  }
  // check now if value will fit in int
  int_tmp = unsigned_long_long_tmp;
  unsigned_long_long_tmp2 = int_tmp;
  if (unsigned_long_long_tmp != unsigned_long_long_tmp2)
  {
    scanErrorCount += 1;
    error("integer constant too large");
    return 1;
  }

  return int_tmp;
}


int yywrap (void)
{
    return 1;
}

