//
// defs.h - global definitions for the xpvm assembler.
//
//

#include <stdio.h>

////////////////////////////////////////////////////////////////////////////
// struct for communication between parser and assembler guts
//
// the parser will pass this struct to the assemble function for
// each line of input that contains a label, instruction, or directive
//
// the struct contains three members:
//   1. format number
//        0 indicates there is no instruction, only a label on the line
//        1-8 indicate the eight instruction formats for vm520
//        9 indicates that it is the "word" or "alloc" directive
//   2. opcode
//   3. union
//        the union has a member for formats 2-8, which contain the
//          particular components required for each format
//
// note that at the point that the parser is communicating with the assembler:
//   labels are strings
//   registers are represented by their register number as an int
//     sp, fp and pc has been converted already to 13, 14 and 15 respectively
//   constants and offsets have been converted from ASCII and have been
//     checked to be sure they fit in an int, but they have not been
//     checked to see if they fit in either the 20 bits required by
//     format 4 or the 16 bits required by format 7.
//
typedef struct instruction {
    unsigned int format;
    char * opcode;
    union {
      struct format2 {
        char * addr;
      } format2;
      struct format3 {
        unsigned int reg;
      } format3;
      struct format4 {
        unsigned int reg;
        int constant;
      } format4;
      struct format5 {
        unsigned int reg;
        char * addr;
      } format5;
      struct format6 {
        unsigned int reg1;
        unsigned int reg2;
      } format6;
      struct format7 {
        unsigned int reg1;
        unsigned int reg2;
        int const8;
      } format7;
      struct format8 {
        unsigned int reg1;
        unsigned int reg2;
        char * addr;
      } format8;
      struct format9 {
        int constant;
      } format9;
      struct format10 {
        unsigned int reg1;
        unsigned int reg2;
        unsigned int reg3;
      } format10;
    } u;
} INSTR;

struct stmt_node {
  char *label;
  INSTR *instr;
  struct stmt_node *link;
} typedef stmt_node;

struct handler_node {
  char  *handle_lbl;
  char  *start_lbl;
  char  *end_lbl;
  int   handle_addr;
  int   start_addr;
  int   end_addr;
  struct handler_node *link;
} typedef handler_node;

struct func_node {
  char  *name;
  unsigned int length;
  int   addr;
  handler_node *handler_list;
  unsigned int num_handlers;
  stmt_node *stmt_list;
  struct func_node *link;
} typedef func_node;

void encode_funcs( func_node * );

extern func_node *func_list;
extern func_node *process_func( char *, char *, handler_node *, stmt_node * );
extern func_node *process_func_list( func_node *, func_node * );
extern handler_node *process_handler( char *, char *, char *);
extern handler_node *process_handler_list( handler_node *, handler_node *);
extern stmt_node *process_stmt( char *, INSTR * );
extern stmt_node *process_stmt_list( stmt_node *, stmt_node * );
extern int verify_handler_list( handler_node * );
// called to process one line of input
//   called on each pass
extern void assemble(char *, INSTR);
extern void initAssemble(void);

// called between passes
//   returns number of errors detected during the first pass
extern int betweenPasses(FILE *);

////////////////////////////////////////////////////////////////////////////
// error message routines (error.c)

// called when some resource is fully depleted
extern void fatal(char *fmt, ...);

// called when there is an internal, unexpected problem
extern void bug(char *fmt, ...);

// called for user semantic error
extern void error(char *fmt, ...);

// called for user syntax error
extern void parseError(char *fmt, ...);

