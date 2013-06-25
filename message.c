//
// message.c - display error messages for cs520 assembler
//
//

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

// file pointer to print messages to
//   set by initMessages, or defaults to stderr
//
static FILE * yyerrfp = 0;

// note: yylineno gets advanced to next line before "assemble" is called.
//       therefore, we subtract one before printing it in this module
//       except for in parseError, which is only called by the parser
//
extern int yylineno;

// for formatting messages
static char buf[1024];

// yyerrfp defaults to stderr
static void checkInitialized(void)
{
  if (yyerrfp == 0)
  {
    yyerrfp = stderr;
  }
}

//  initMessages
//
//  initialize the message module
//
void InitMessages(FILE *fp)
{
  yyerrfp = fp;
}

//  error
//
//  print error message (ie user made mistake)
//
//
void error(char * fmt, ...)
{
  checkInitialized();
  va_list ap;
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  va_end(ap);
  fprintf(yyerrfp,"[error] line %d:  %s\n", yylineno-1, buf);
}

//  parseError
//
//  print error message when parse error encountered
//  (like "error" except don't subtract one from yylineno)
//
//
void parseError(char * fmt, ...)
{
  checkInitialized();
  va_list ap;
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  va_end(ap);
  fprintf(yyerrfp,"[error] line %d:  %s\n", yylineno, buf);
}

//  fatal
//
//  print fatal assembler error message
//
//  (usually means some internal data structure overflowed)
//
void fatal(char * fmt, ...)
{
  checkInitialized();
  va_list ap;
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  va_end(ap);
  fprintf(yyerrfp,"[fatal error] line %d:  %s\n", yylineno-1, buf);
  exit(1);
}

//  bug
//
//  print assembler internal error message
//
//  (shouldn't happen?!)
//
void bug(char * fmt, ...)
{
  checkInitialized();
  va_list ap;
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  va_end(ap);
  fprintf(yyerrfp,"[compiler bug] line %d:  %s\n", yylineno-1, buf);
  exit(1);
}

