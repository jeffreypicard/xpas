//
// defs.h - global definitions for cs520 assembler
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
        int offset;
      } format7;
      struct format8 {
        unsigned int reg1;
        unsigned int reg2;
        char * addr;
      } format8;
      struct format9 {
        int constant;
      } format9;
    } u;
} INSTR;

extern void process_func( char *, char *);
extern void process_handler( char *, char *, char *);
// called to process one line of input
//   called on each pass
extern void assemble(char *, INSTR);

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

