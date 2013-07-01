/*
 * assemble.c - handles the details of assembly for the cs520 assembler
 *
 *              This is Phil Hatcher's implementation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"

// enable debugging printout?
#define DEBUG 1

// have betweenPasses print defined labels and their addresses to stdout?
#define PRINT_DEFINED_LABELS 1

/*
 * Globals
 */

/* List of all declared exception handlers */
//static handler_node *handler_list = NULL;

/* List of all declared functions */
func_node *func_list = NULL;

// track which pass we are on
static int currentPass = 1;

// tracks how many user errors have been seen
// pass2 is not performed if a user error is detected during pass1
static int errorCount = 0;

// file pointer to use for outputing byte code
static FILE *fp;

// running count for number of words to output for object code
static unsigned int currentLength = 0;

/* number of block the object file will contain */
static int num_blocks = 0;

// forward references for private symbol table routines
static void *symtabLookup(char *id);
static int symtabInstallDefinition(char *id, unsigned int addr);
static void symtabInstallReference(char *id, unsigned int addr,
                                   unsigned int format);
static void symtabInstallExport(char *id);
static void symtabInstallImport(char *id);
static void *symtabInitIterator(void);
static void *symtabNext(void *inIter);
static void *referenceInitIterator(void *symRec);
static unsigned int referenceNext(void *inIter, unsigned int *outFormat);

// forward reference to private debug routines
static void dump_stmt_list( stmt_node *);
#if DEBUG
static void dumpInstrStruct(INSTR instr);
static void dumpSymbolTable(void);
#endif
#if PRINT_DEFINED_LABELS
static void printDefinedLabels(void);
#endif

// forward reference to the private assemble routines
static int verifyOpcode(char *opcode);
static int getOpcodeEncoding(char *opcode);
static void outputWord(int value);
static unsigned int checkForImportExportErrors(void);
static void checkForAddressErrors(void);
static void output_header(void);
static int encodeAddr20(char*, unsigned int);
static int encodeAddr16(char*, unsigned int);
static unsigned int fit_in_8(int value);
static unsigned int fitIn16(int value);
static unsigned int fitIn20(int value);
static void checkAddr(char*, unsigned int def, unsigned int ref,
                     unsigned int format);

static func_node *func_pass1( char *, handler_node *, stmt_node * );
//static void func_pass2( char * );
static handler_node *handler_pass1( char *, char *, char * );
static handler_node *handler_pass2( char *, char *, char * );
static void dump_funcs( func_node * );
static void dump_handler_list( handler_node * );

static void verify_handler_list( handler_node * );
//////////////////////////////////////////////////////////////////////////
// public entry points

// this is called once so that the assembler can initialize any internal
// data structures.
void initAssemble(void)
{
#if DEBUG
  fprintf(stderr, "initAssemble called\n");
#endif
}

/*
 * get_blk_id
 *
 * Takes a block name as string and return the block id.
 * FIXME: This should not rely on the global variable, preferably.
 */
static unsigned int get_blk_id( const char *blk_name )
{
  unsigned int id = 0;
  func_node *walk = func_list;
  while (walk)
  {
    if (!strcmp(blk_name, walk->name))
      return id;
    id += 1; 
    walk = walk->link;
  }
  return 0;
}

static void encode_stmt( stmt_node *stmt )
{
  // if there is not an instruction then we are done
  if (stmt->instr->format == 0)
  {
    return;
  }

  if (!strcmp(stmt->instr->opcode, "ldblkid") && stmt->instr->format == 5 )
  {
    stmt->instr->format = 4;
    /* This is a wicked hack :):):) */
    stmt->instr->u.format4.constant = get_blk_id( stmt->instr->u.format5.addr );
  }

  // also can skip the import and export directives
  if (!strcmp(stmt->instr->opcode, "export") || !strcmp(stmt->instr->opcode, "import"))
  {
    return;
  }

  // now handle the remaining directives
  if (!strcmp(stmt->instr->opcode, "alloc"))
  {
    // need to add to currentLength
    int len = (stmt->instr->u.format9.constant);
    currentLength += len;
    int i;
    for (i = 0; i < len; i += 1)
    {
      outputWord(0);
    }
    return;
  }
  if (!strcmp(stmt->instr->opcode, "word"))
  {
    currentLength += 1;
    outputWord(stmt->instr->u.format9.constant);
    return;
  }

  // now handle the instructions
  // go ahead and count the word to be encoded
  //   so currentLength will be equal to what PC will be when it executes
  currentLength += 1;

  // get the opcode encoding
  char encodedOpcode = getOpcodeEncoding(stmt->instr->opcode);

  // now handle the different instruction formats
  int encodedAddr;
  switch (stmt->instr->format)
  {
    case 1:
      outputWord(encodedOpcode);
      break;
    case 2:
      encodedAddr = encodeAddr20(stmt->instr->u.format2.addr, currentLength);
      outputWord((encodedAddr << 12) |
                 (encodedOpcode));
      break;
    case 3:
      outputWord((stmt->instr->u.format3.reg << 16) |
                 (encodedOpcode << 24));
      break;
    case 4:
      outputWord((stmt->instr->u.format4.constant & 0xFFFF) |
                 (stmt->instr->u.format4.reg << 16) |
                 (encodedOpcode << 24));
      break;
    case 5:
      encodedAddr = encodeAddr20(stmt->instr->u.format5.addr, currentLength);
      outputWord((encodedAddr << 12) |
                 (stmt->instr->u.format5.reg << 8) |
                 (encodedOpcode));
      break;
    case 6:
      outputWord((encodedOpcode << 24) |
                 (stmt->instr->u.format6.reg1 << 16) |
                 (stmt->instr->u.format6.reg2 << 8));
      break;
    case 7:
      outputWord((encodedOpcode << 24) |
                 (stmt->instr->u.format7.reg1 << 16) |
                 (stmt->instr->u.format7.reg2 << 8) |
                 (stmt->instr->u.format7.const8));
      break;
    case 8:
      encodedAddr = encodeAddr16(stmt->instr->u.format8.addr, currentLength);
      outputWord((encodedOpcode << 24) |
                 (stmt->instr->u.format8.reg1 << 16) |
                 (stmt->instr->u.format8.reg2 << 8) |
                 (encodedAddr));
      break;
    case 10:
      outputWord((encodedOpcode << 24) |
                 (stmt->instr->u.format10.reg1 << 16) |
                 (stmt->instr->u.format10.reg2 << 8) |
                 (stmt->instr->u.format10.reg3));
      break;
    default:
      bug("unexpected format (%d) seen in encode_stmt", stmt->instr->format);
  }

}

void encode_stmt_list( stmt_node *stmt_list )
{
  stmt_node *walk = stmt_list;
  while (walk)
  {
    encode_stmt( walk );
    walk = walk->link;
  }
}

void encode_handler( handler_node *handler )
{
  outputWord( handler->start_addr*4 );
  outputWord( handler->end_addr*4 );
  outputWord( handler->handle_addr*4 );
}

void encode_handler_list( handler_node *handler_list )
{
  handler_node *walk = handler_list;
  while (walk)
  {
    encode_handler(walk);
    walk = walk->link;
  }
}

void encode_func( func_node *func )
{
  char *name = func->name;
  while( putc( *name++, fp ) );
  /* annotations */
  outputWord( 0 );
  outputWord( 2 );
  /* frame size */
  outputWord( 0 );
  /* contents length */
  outputWord( func->length*4 );
  encode_stmt_list( func->stmt_list );
  /* number exception handlers */
  outputWord( func->num_handlers );
  encode_handler_list( func->handler_list );
  /* number outsymbol references */
  outputWord( 0 );
  /* number native functions references */
  outputWord( 0 );
  /* auxiliary data length */
  outputWord( 0 );
}

void encode_funcs( func_node *func_list )
{
  func_node *walk = func_list;
  while (walk)
  {
    encode_func( walk );
    walk = walk->link;
  }
}

func_node *process_func_list( func_node *node, func_node *list )
{
  if (node)
  {
    node->link = list;
    return node;
  }
  else
    return NULL;
}

func_node *process_func( char *id1, char *id2, handler_node *handler_list, 
                   stmt_node *stmt_list )
{
  currentLength = 0; 
  if ( strcmp( id1, id2) )
  {
    error("start and end ids for functions must match.");
    errorCount += 1;
  }
  switch ( currentPass )
  {
    case 1:
      return func_pass1( id1, handler_list, stmt_list );
      break;
    case 2:
      return NULL;
      //func_pass2( id1 );
      break;
    default:
      bug("unexpected current pass number (%d) in process_func\n", 
          currentPass);
  }
  return NULL;
}

handler_node *process_handler( char *handle, char *start, char *end )
{
  switch ( currentPass )
  {
    case 1:
      return handler_pass1( handle, start, end );
      break;
    case 2:
      return handler_pass2( handle, start, end );
      break;
    default:
      bug("unexpected current pass number (%d) in process_handler\n", 
          currentPass);
  }
  return NULL;
}

handler_node *process_handler_list( handler_node *node, handler_node *list )
{
  if (node)
  {
    node->link = list;
    return node;
  }
  else
    return NULL;
}

// this is called between passes and provides the assembler the file
// pointer to use for outputing the object file
//
// it returns the number of errors seen on pass1
//
int betweenPasses(FILE *outf)
{
#if DEBUG
  fprintf(stderr, "betweenPasses called\n");
  dumpSymbolTable();
  dump_funcs( func_list );
  //dump_handler_list( handler_list );
#endif
  //verify_handler_list( handler_list );

#if PRINT_DEFINED_LABELS
  printDefinedLabels();
#endif

  // remember the file pointer to use
  fp = outf;

  // update the pass number
  currentPass = 2;

  // check if memory will overflow
  if (currentLength > 0xFFFFF)
  {
    error("program consumes more than 2^20 words");
    errorCount += 1;
  }

  // check for errors concerning addresses
  checkForAddressErrors();

  // check for errors concerning import and export
  errorCount += checkForImportExportErrors();

  // if no errors, output headers and then insymbol and outsymbol section
  if (!errorCount)
  {
    output_header();
    //outputInsymbols();
    //outputOutsymbols();
  }

  // reset currentLength for pass2
  currentLength = 0;

  return errorCount;
}

/********************************************************************
 * function processing routines                                     *
 ********************************************************************/

unsigned int stmt_list_length( stmt_node *stmt_list )
{
  unsigned int length = 0;
  stmt_node *walk = stmt_list;

  while (walk)
  {
    if (walk->instr->format != 0 )
      length += 1;
    walk = walk->link;
  }

  return length;
}

unsigned int handler_list_length( handler_node *handler_list )
{
  unsigned int length = 0;
  handler_node *walk = handler_list;

  while (walk)
  {
    length += 1;
    walk = walk->link;
  }

  return length;
}

/*
 * func_pass1
 *
 * processing a function declaration on pass 1
 */
static func_node *func_pass1( char *id, handler_node *handler_list, 
                              stmt_node *stmt_list )
{
  func_node *new = calloc( 1, sizeof *new );
  if ( !new )
    fatal("malloc failed in func_pass1");
  /* FIXME: should functions and labels be separate? */
  if (!symtabInstallDefinition(id, currentLength))
  {
    error("label %s already defined", id);
    errorCount += 1;
  }
  new->name = id;
  new->handler_list = handler_list;
  new->stmt_list = stmt_list;
  new->length = stmt_list_length( stmt_list );
  new->num_handlers = handler_list_length( handler_list );
  num_blocks += 1;
  return new;
}

/*
 * func_pass2
 *
 * processing a function declaration on pass 2
 */
/*
static void func_pass2( char *id )
{
}
*/

static void dump_stmt_list( stmt_node *stmt_list )
{
  stmt_node *walk = stmt_list;
  while (walk)
  {
    if( walk->instr->format != 0 )
      dumpInstrStruct( *walk->instr );
    else
      fprintf( stderr, "%s:\n", walk->label );
    walk = walk->link;
  }
}

/*
 * dump_funcs
 *
 * Print out information about all declared functions.
 */
static void dump_funcs( func_node *root )
{
  func_node *walk = root;
  fprintf(stderr, "function list dump==================================\n");
  while ( walk )
  {
    fprintf( stderr, "%s\n", walk->name );
    dump_handler_list( walk->handler_list );
    dump_stmt_list( walk->stmt_list );
    walk = walk->link;
  }
  fprintf(stderr, "====================================================\n");
}

/********************************************************************
 * exception handler processing routines                            *
 ********************************************************************/

/*
 * handler_pass1
 *
 * processing an exception handler  declaration on pass 1
 */
static handler_node *handler_pass1( char *handle, char *start, char *end )
{
  handler_node *new = calloc( 1, sizeof *new );
  if ( !new )
    fatal("malloc failed in handler_pass1");
  new->handle_lbl = handle;
  new->start_lbl = start;
  new->end_lbl = end;
  //handler_push( &handler_list, new );
  //num_handlers += 1;
  return new;
}

/*
 * handler_pass2
 *
 * processing an exception handler declaration on pass 2
 */
static handler_node *handler_pass2( char *handle, char *start, char *end )
{
  return NULL;
}

/*
 * dump_handler_list
 *
 * Print out information about all declared exception handlers.
 */
static void dump_handler_list( handler_node *root )
{
  handler_node *walk = root;
  fprintf(stderr, "handler list dump===================================\n");
  while ( walk )
  {
    fprintf( stderr, "%s, %s, %s\n", walk->handle_lbl
                                   , walk->start_lbl
                                   , walk->end_lbl );
    walk = walk->link;
  }
  fprintf(stderr, "====================================================\n");
}

static stmt_node *assemble_pass1( char *, INSTR * );

stmt_node *process_stmt( char *label, INSTR *instr )
{
  switch ( currentPass )
  {
    case 1:
      return assemble_pass1( label, instr );
      break;
    case 2:
      return NULL;
      break;
    default:
      bug("unexpected current pass number (%d) in process_stmt\n", 
          currentPass);
  }
  return NULL;
}

stmt_node *process_stmt_list( stmt_node *node, stmt_node *list )
{
  if (node)
  {
    node->link = list;
    return node;
  }
  else
    return NULL;
}

//////////////////////////////////////////////////////////////////////////
// the two main assembly routines, one for each pass

// assemblePass1
//
// process a line during pass 1
//
static stmt_node *assemble_pass1( char *label, INSTR *instr )
{
  stmt_node *new = calloc( 1, sizeof *new );
  new->label = label;
  new->instr = instr;
  // first handle the label, if one
  if (label && !symtabInstallDefinition(label, currentLength))
  {
    error("label %s already defined", label);
    errorCount += 1;
  }

  // if there is not an instruction then we are done
  if (instr->format == 0)
  {
    return new;
  }

  // sanity check for instruction format
  if (instr->format > 10)
  {
    bug("bogus format (%d) seen in assemblePass1", instr->format);
  }

  // if there is an instruction, go ahead and count its word
  //   so currentLength will be equal to what PC will be when it executes
  currentLength += 1;

  // verify the opcode
  int format = verifyOpcode(instr->opcode);
  if (format == 0)
  {
    error("unknown opcode");
    errorCount += 1;
    return NULL;
  }

  // does the opcode match the structure of the line?
  if (format != instr->format)
  {
    error("opcode does not match the given operands");
    errorCount += 1;
    return NULL;
  }

  // first handle the directives which have the special encoding of 0xFF
  if(getOpcodeEncoding(instr->opcode) == 0xFF)
  {
    if (!strcmp(instr->opcode, "alloc"))
    {
      // need to verify its constant is greater than zero
      if (instr->u.format9.constant <= 0)
      {
        error("constant must be greater than zero");
        instr->u.format9.constant = 0; // squash other errors
        errorCount += 1;
      }

      // need to add to currentLength, remember one has already been added
      currentLength += (instr->u.format9.constant - 1);
    }
    else if (!strcmp(instr->opcode, "word"))
    {
      // actually nothing to do here!
      //   constant has already been verified to fit in 32 bits
    }
    else if (!strcmp(instr->opcode, "export"))
    {
      // this directive takes no space
      currentLength -= 1;
      symtabInstallExport(instr->u.format2.addr);
    }
    else if (!strcmp(instr->opcode, "import"))
    {
      // this directive takes no space
      currentLength -= 1;
      symtabInstallImport(instr->u.format2.addr);
    }
    else
    {
      bug("bogus encoding 0xFF for opcode %s", instr->opcode);
    }
  }
  else
  {
    // now process the instructions
    // stash the references to symbols for later processing
    // and check the constants and offsets to see if they will fit 
    switch (instr->format)
    {
      case 2:
        symtabInstallReference(instr->u.format2.addr, currentLength - 1, 2);
        break;
      case 4:
        if (!fitIn20(instr->u.format4.constant))
        {
          error("constant %d will not fit in 20 bits",
            instr->u.format4.constant);
          errorCount += 1;
        }
        break;
      case 5:
        symtabInstallReference(instr->u.format5.addr, currentLength - 1, 5);
        break;
      case 7:
        if (!fit_in_8(instr->u.format7.const8))
        {
          error("constant %d will not fit in 8 bits",
                instr->u.format7.const8);
          errorCount += 1;
        }
        break;
      case 8:
        symtabInstallReference(instr->u.format8.addr, currentLength - 1, 8);
        break;
    }
  }
  return new;
}

//////////////////////////////////////////////////////////////////////////
// support for outputing to the object file

// outputWord
//
// puts a word out one byte at a time in little Endian format
// Switched to big endian for xpvm.
//
static void outputWord(int value)
{
#if 0
  putc(value & 0xFF, fp);
  putc((value >> 8) & 0xFF, fp);
  putc((value >> 16) & 0xFF, fp);
  putc((value >> 24) & 0xFF, fp);
#endif
  putc((value >> 24) & 0xFF, fp);
  putc((value >> 16) & 0xFF, fp);
  putc((value >> 8) & 0xFF, fp);
  putc(value & 0xFF, fp);
}

//////////////////////////////////////////////////////////////////////////
// process opcodes
//
// this array defines the opcodes and the directives, providing their
// instruction format and their encoding. Of course, only instructions
// have encodings.
//
#if 0
static struct opcodeInfo
{
   char*          opcode;
   int            format;
   unsigned char  encoding;
}
opcodes[] =
{
{"halt",    1, 0x00},
{"load",    5, 0x01},
{"store",   5, 0x02},
{"ldimm",   4, 0x03},
{"ldaddr",  5, 0x04},
{"ldind",   7, 0x05},
{"stind",   7, 0x06},
{"addf",    6, 0x07},
{"subf",    6, 0x08},
{"divf",    6, 0x09},
{"mulf",    6, 0x0A},
{"addi",    6, 0x0B},
{"subi",    6, 0x0C},
{"divi",    6, 0x0D},
{"muli",    6, 0x0E},
{"call",    2, 0x0F},
{"ret",     1, 0x10},
{"blt",     8, 0x11},
{"bgt",     8, 0x12},
{"beq",     8, 0x13},
{"jmp",     2, 0x14},
{"cmpxchg", 8, 0x15},
{"getpid",  3, 0x16},
{"getpn",   3, 0x17},
{"push",    3, 0x18},
{"pop",     3, 0x19},
{"word",    9, 0xFF},
{"alloc",   9, 0xFF},
{"import",  2, 0xFF},
{"export",  2, 0xFF}
};
#endif
//
// xpvm opcodes
//
#if 1
static struct opcodeInfo
{
   char*          opcode;
   int            format;
   unsigned char  encoding;
}
opcodes[] =
{
{"ldb",                   0, 0x02},
{"ldb",                   0, 0x03},
{"lds",                   0, 0x04},
{"lds",                   0, 0x05},
{"ldi",                   0, 0x06},
{"ldi",                   0, 0x07},
{"ldl",                   0, 0x08},
{"ldl",                   0, 0x09},
{"ldf",                   0, 0x0A},
{"ldf",                   0, 0x0B},
{"ldd",                   0, 0x0C},
{"ldd",                   0, 0x0D},
{"ldimm",                 4, 0x0E},
{"ldimm2",                0, 0x0F},
{"stb",                   0, 0x10},
{"stb",                   0, 0x11},
{"sts",                   0, 0x12},
{"sts",                   0, 0x13},
{"sti",                   0, 0x14},
{"sti",                   0, 0x15},
{"stl",                   0, 0x16},
{"stl",                   0, 0x17},
{"stf",                   0, 0x18},
{"stf",                   0, 0x19},
{"std",                   0, 0x1A},
{"std",                   0, 0x1B},
{"ldblkid",               5, 0x1C}, /* pseudo instruction */
{"ldnative",              0, 0x1D},
{"addl",                  0, 0x20},
{"addl",                  0, 0x21},
{"subl",                  0, 0x22},
{"subl",                  0, 0x23},
{"mull",                  0, 0x24},
{"mull",                  0, 0x25},
{"divl",                 10, 0x26},
{"divl",                  7, 0x27},
{"reml",                  0, 0x28},
{"reml",                  0, 0x29},
{"negl",                  0, 0x2A},
{"addd",                  0, 0x2B},
{"subd",                  0, 0x2C},
{"muld",                  0, 0x2D},
{"divd",                 10, 0x2E},
{"negd",                  6, 0x2F},
{"cvtld",                 6, 0x30},
{"cvtdl",                 0, 0x31},
{"lshift",                0, 0x32},
{"lshift",                0, 0x33},
{"rshift",                0, 0x34},
{"rshift",                0, 0x35},
{"rshiftu",               0, 0x36},
{"rshiftu",               0, 0x37},
{"and",                   0, 0x38},
{"or",                    0, 0x39},
{"xor",                   0, 0x3A},
{"ornot",                 0, 0x3B},
{"cmpeq",                 0, 0x40},
{"cmpeq",                 0, 0x41},
{"cmple",                 0, 0x42},
{"cmple",                 0, 0x43},
{"cmplt",                 0, 0x44},
{"cmplt",                 0, 0x45},
{"cmpule",                0, 0x46},
{"cmpule",                0, 0x47},
{"cmpult",                0, 0x48},
{"cmpult",                0, 0x49},
{"fcmpeq",                0, 0x4A},
{"fcmple",                0, 0x4B},
{"fcmplt",                0, 0x4C},
{"jmp",                   0, 0x50},
{"jmp",                   0, 0x51},
{"btrue",                 0, 0x52},
{"bfalse",                0, 0x53},
{"alloc_blk",             0, 0x60},
{"alloc_private_blk",     0, 0x61},
{"aquire_blk",            0, 0x62},
{"release_blk",           0, 0x63},
{"set_volatile",          0, 0x64},
{"get_owner",             0, 0x65},
{"call",                  6, 0x72},
{"calln",                 7, 0x73},
{"ret",                   3, 0x74},
{"throw",                 0, 0x80},
{"retrieve",              0, 0x81},
{"init_proc",             0, 0x90},
{"join",                  0, 0x91},
{"join2",                 0, 0x92},
{"whoami",                0, 0x93},
};
#endif


// verifyOpcode
//
// given an opcode string get its instruction format
//
// returns 0 if opcode is not found
//
static int verifyOpcode(char *opcode)
{
    int i;

    i = 0;
    while (opcodes[i].opcode)
    {
        if (!strcmp(opcode, opcodes[i].opcode))
        {
            return opcodes[i].format;
        }
        i++;
    }
    return 0;
}

// getOpcodeEncoding
//
// given an opcode string get its encoding
//
// returns -1 if opcode is not found
//
static int getOpcodeEncoding(char *opcode)
{
    int i;

    i = 0;
    while (opcodes[i].opcode)
    {
        if (!strcmp(opcode, opcodes[i].opcode))
        {
            return opcodes[i].encoding;
        }
        i++;
    }
    return -1;
}

//////////////////////////////////////////////////////////////////////////
// debugging routines

#if DEBUG
// dumpInstrStruct
//
// dump to stderr an INSTR struct
//
static void dumpInstrStruct(INSTR instr)
{
  if (instr.format != 0)
  {
    //fprintf(stderr, "  instruction is %s", instr.opcode);
    fprintf(stderr, "\t%s", instr.opcode);
    switch(instr.format)
    {
      case 1:
        fprintf(stderr, "\n");
        break;
      case 2:
        fprintf(stderr, " %s\n", instr.u.format2.addr);
        break;
      case 3:
        fprintf(stderr, " r%d\n", instr.u.format3.reg);
        break;
      case 4:
        fprintf(stderr, " r%d,%d\n", instr.u.format4.reg,
          instr.u.format4.constant);
        break;
      case 5:
        fprintf(stderr, " r%d,%s\n", instr.u.format5.reg,
          instr.u.format5.addr);
        break;
      case 6:
        fprintf(stderr, " r%d,r%d\n", instr.u.format6.reg1,
          instr.u.format6.reg2);
        break;
      case 7:
        fprintf(stderr, " r%d,r%d,%d\n", instr.u.format7.reg1,
          instr.u.format7.reg2, instr.u.format7.const8);
        break;
      case 8:
        fprintf(stderr, " r%d,r%d,%s\n", instr.u.format8.reg1,
          instr.u.format8.reg2, instr.u.format8.addr);
        break;
      case 9:
        fprintf(stderr, " %d\n", instr.u.format9.constant);
        break;
      case 10:
        fprintf(stderr, " r%d,r%d,r%d\n", instr.u.format10.reg1,
                                          instr.u.format10.reg2,
                                          instr.u.format10.reg3 );
        break;
      default:
        bug("unexpected instruction format (%d) in dumpInstrStruct",
          instr.format);
        break;
    }
  }
}
#endif

//////////////////////////////////////////////////////////////////////////
// symbol table implementation

// struct to track a reference to a symbol
typedef struct reference {
  unsigned int addr;         // address of instruction referencing symbol
  unsigned int format;       // format of instruction referencing symbol
  struct reference *next;
} REFERENCE_REC;

// symbol table record itself
typedef struct symtab {
  char *id;
  int isDefined;      // appears in a label definition?
  int isReferenced;   // is referenced as an operand of an instruction?
  int isExported;     // named in export directive?
  int isImported;     // named in import directive?
  unsigned int addr;  // address, if defined
  struct reference *references; // list of references to the id
  struct symtab *next;
} SYMTAB_REC;

// symbol table is just a list
static struct symtab * symtab = 0;   // symbol table initially empty

// symtabMakeRecord
//
// for internal use: allocate symbol table record for id
//
// returns pointer to record
//
static struct symtab * symtabMakeRecord(void)
{
  SYMTAB_REC *st = (SYMTAB_REC *) malloc(sizeof(SYMTAB_REC));
  if (st == NULL)
  {
    fatal("out of memory in symtabMakeRecord");
  }
  return st;
}

// symtabInstallRecord
//
// for internal use: insert record into symbol table
//
static void symtabInstallRecord(struct symtab *rec)
{
  // put it on front of linked list
  rec->next = symtab;
  symtab = rec;
}

// symtabLookup
//
// returns abstract pointer to record if id found and 0 otherwise
//
static void * symtabLookup(char *id)
{
  SYMTAB_REC *st=symtab;  // begin search at begininng of linked list

  while (st)     // ie while 'st' is not NULL pointer
  {
    if (!strcmp(id,st->id))
    {
      return st;
    }
    st = st->next;
  }
  return 0;
}

//  symtabInstallDefinition
//
//  install (id,addr) definition into symbol table
//
//  returns 1 if successful and 0 if id is already defined
//
static int symtabInstallDefinition(char *id, unsigned int addr)
{
  SYMTAB_REC *st = symtabLookup(id);  // is id already in table?

  if (st)
  {
    if (st->isDefined)
    {
      return 0;
    }
    st->isDefined = 1;
    st->addr = addr;
  }
  else
  {
    // make new record
    st = symtabMakeRecord();
    st->id = id;
    st->addr = addr;
    st->isDefined = 1;
    st->isReferenced = 0;
    st->isExported = 0;
    st->isImported = 0;
    st->references = NULL;

    // install it into table
    symtabInstallRecord(st);
  }
  return 1;
}

//  symtabInstallReference
//
//  install id which is not yet defined into the symbol table
//
//  this routine will update an existing record for the id or
//  it will create a new record if there is none for the id
//
static void symtabInstallReference(char *id, unsigned int addr,
                                   unsigned int format)
{
  SYMTAB_REC *st = symtabLookup(id);  // is id already in table?

  REFERENCE_REC *p = malloc(sizeof(REFERENCE_REC));
  if (p == NULL)
  {
    fatal("out of memory in symtabInstallReference");
  }
  p->addr = addr;
  p->format = format;
  p->next = NULL;

  if (st)
  {
    st->isReferenced = 1;
    p->next = st->references;
    st->references = p;
  }
  else
  {
    // allocate new record
    st = symtabMakeRecord();
    st->id = id;
    st->addr = 0;
    st->isDefined = 0;
    st->isReferenced = 1;
    st->isExported = 0;
    st->isImported = 0;
    st->references = p;

    // install it into the table
    symtabInstallRecord(st);
  }
  return;
}

//  symtabInstallExport
//
//  install id which is being exported
//
//  this routine will update an existing record for the id or
//  it will create a new record if there is none for the id
//
static void symtabInstallExport(char *id)
{
  SYMTAB_REC *st = symtabLookup(id);  // is id already in table?

  if (st)
  {
    // it is an error if this symbol is already exported
    if (st->isExported == 1)
    {
      error("symbol %s exported more than once", id);
      errorCount += 1;
    }
    else
    {
      st->isExported = 1;
    }
  }
  else
  {
    // allocate new record
    st = symtabMakeRecord();
    st->id = id;
    st->addr = 0;
    st->isDefined = 0;
    st->isReferenced = 0;
    st->isExported = 1;
    st->isImported = 0;
    st->references = NULL;

    // install it into the table
    symtabInstallRecord(st);
  }
  return;
}

//  symtabInstallImport
//
//  install id which is being imported
//
//  this routine will update an existing record for the id or
//  it will create a new record if there is none for the id
//
static void symtabInstallImport(char *id)
{
  SYMTAB_REC *st = symtabLookup(id);  // is id already in table?

  if (st)
  {
    // it is an error if this symbol is already imported
    if (st->isImported == 1)
    {
      error("symbol %s imported more than once", id);
      errorCount += 1;
    }
    else
    {
      st->isImported = 1;
    }
  }
  else
  {
    // allocate new record
    st = symtabMakeRecord();
    st->id = id;
    st->addr = 0;
    st->isDefined = 0;
    st->isReferenced = 0;
    st->isExported = 0;
    st->isImported = 1;
    st->references = NULL;

    // install it into the table
    symtabInstallRecord(st);
  }
  return;
}

struct iteratorSym
{
  SYMTAB_REC *next;
};

//  symtabInitIter
//

//
//  NOTE: symbol table should not be modified during an iterator
//        sequence!
//
static void *symtabInitIterator(void)
{
  struct iteratorSym *ret = (struct iteratorSym *)
    malloc(sizeof(struct iteratorSym));
  if (ret == NULL)
  {
    fatal("out of memory in symtabInitIter");
  }
  ret->next = symtab;
  return (void *) ret;
}

//
//  symtabNext
//
//  gets next symbol table record for an iterator sequence
//  returns NULL when sequence is done
//
static void *symtabNext(void *inIter)
{
  void *ret;
  struct iteratorSym *iter = (struct iteratorSym *) inIter;
  if (!iter->next)
  {
      free(iter);
      return NULL;
  }
  ret = iter->next;
  iter->next = iter->next->next;
  return ret;
}

struct iteratorRef
{
  REFERENCE_REC *next;
};

//  referenceInitIter
//
//  initializes an iterator to traverse reference list for a symbol
//
//  NOTE: reference list should not be modified during an iterator
//        sequence!
//
static void *referenceInitIterator(void *symRec)
{
  SYMTAB_REC *p = symRec;
  struct iteratorRef *ret = (struct iteratorRef *)
    malloc(sizeof(struct iteratorRef));
  if (ret == NULL)
  {
    fatal("out of memory in referenceInitIter");
  }
  ret->next = p->references;
  return (void *) ret;
}

//  referenceNext
//
//  returns next (addr,format) pair
//    the addr is the return value
//    the format is returned through the second parameter
//
//  returns -1 when no more (addr,format) pairs
//
static unsigned int referenceNext(void *inIter, unsigned int *outFormat)
{
  unsigned int ret;
  struct iteratorRef *iter = (struct iteratorRef *) inIter;
  if (!iter->next)
  {
      free(iter);
      return -1;
  }
  ret = iter->next->addr;
  *outFormat = iter->next->format;
  iter->next = iter->next->next;
  return ret;
}

//////////////////////////////////////////////////////////////////////////
// support routines that need to know the symbol table details

// checkForAddressErrors
//
// iterate over symbols to do error checking for addresses
//   for each reference, check to see if the symbol is defined or imported
//   if defined, be sure that the PC-relative address will fit
//
// uses error() function to report errors and increments global errorCount
//
void checkForAddressErrors(void)
{
  void *iter = symtabInitIterator();
  SYMTAB_REC *p = symtabNext(iter);
  while (p)
  {
    if (p->isReferenced)
    {
      if (!p->isDefined && !p->isImported)
      {
        error("label %s is referenced but not defined or imported", p->id);
        errorCount += 1;
      }
      else
      {
        if (p->isDefined)
        {
          // iterate over all references
          void *iter2 = referenceInitIterator(p);
          unsigned int format;
          unsigned int addr = referenceNext(iter2, &format);
          while (addr != -1)
          {
            checkAddr(p->id, p->addr, addr + 1, format);
            addr = referenceNext(iter2, &format);
          }
        }
      }
    }
    p = symtabNext(iter);
  }
}

//
// checkForImportExportErrors
//
// iterate over symbols to do error checking for imports and exports:
//   1. no symbol can be both imported and exported
//   2. if a symbol is imported it should not be defined
//   3. if a symbol is imported it should be referenced
//   4. if a symbol is exported it should be defined
//   5. if a symbol is referenced but not defined then it should be imported
//        actually this error is checked in checkForAddressErrors()
//   6. a symbol that is being imported or exported must be 16 chars or less
//
static unsigned int checkForImportExportErrors(void)
{
  void *iter = symtabInitIterator();
  SYMTAB_REC *p = symtabNext(iter);
  int ret = 0;
  while (p)
  {
    if (p->isImported && p->isExported)
    {
      error("symbol %s is both imported and exported", p->id);
      ret += 1;
    }
    if (p->isImported && p->isDefined)
    {
      error("symbol %s is both imported and defined", p->id);
      ret += 1;
    }
    if (p->isImported && !p->isReferenced)
    {
      error("symbol %s is imported but not referenced", p->id);
      ret += 1;
    }
    if (p->isExported && !p->isDefined)
    {
      error("symbol %s is exported but not defined", p->id);
      ret += 1;
    }
    if (p->isImported && (strlen(p->id) > 16))
    {
      error("symbol %s is imported and longer than 16 characters", p->id);
      ret += 1;
    }
    if (p->isExported && (strlen(p->id) > 16))
    {
      error("symbol %s is exported and longer than 16 characters", p->id);
      ret += 1;
    }
    p = symtabNext(iter);
  }
  return ret;
}

#if PRINT_DEFINED_LABELS
//  printDefinedLabels
//
//  Print the defined labels and their addresses to stdout.
//
static void printDefinedLabels(void)
{
  void *iter = symtabInitIterator();
  SYMTAB_REC *p = symtabNext(iter);
  while (p)
  {
    if (p->isDefined)
    {
      printf("%s %d\n", p->id, p->addr);
    }
    p = symtabNext(iter);
  }
}
#endif

/*
 * output_header
 *
 * Output the header information necessary for the xpvm object file format.
 * Currently just the magic number and the number of blocks.
 */
static void 
output_header( void )
{
  const int MAGIC = 0x31303636; 
  outputWord( MAGIC );
  outputWord( num_blocks );
}

#if DEBUG
// dumpSymbolTable
//
// dump the symbol table for debugging
//
static void dumpSymbolTable(void)
{
  fprintf(stderr, "symbol table dump===================================\n");
  unsigned int outFormat;
  void *iter = symtabInitIterator();
  SYMTAB_REC *p = symtabNext(iter);
  while (p)
  {
    fprintf(stderr, "%s:\n", p->id);
    fprintf(stderr, "  addr %d\n", p->addr);
    fprintf(stderr, "  isDefined %d\n", p->isDefined);
    fprintf(stderr, "  isReferenced %d\n", p->isReferenced);
    fprintf(stderr, "  isExported %d\n", p->isExported);
    fprintf(stderr, "  isImported %d\n", p->isImported);
    fprintf(stderr, "  references:\n");
    void *iter2 = referenceInitIterator(p);
    unsigned int addr = referenceNext(iter2, &outFormat );
    while (addr != -1)
    {
      fprintf(stderr, "    %d\n", addr);
      addr = referenceNext(iter2, &outFormat );
    }
    p = symtabNext(iter);
  }
  fprintf(stderr, "====================================================\n");
}
#endif

/*
 * get_symbol_addr
 *
 * Takes a symbol as a string and returns its address in the
 * file. Returns -1 if the symbol is not defined.
 */
static int get_symbol_addr( const char *symbol )
{
  void *iter = symtabInitIterator();
  SYMTAB_REC *p = symtabNext(iter);
  while (p)
  {
    if (!strcmp(p->id, symbol))
      return p->addr;
    p = symtabNext(iter);
  }
  return -1;
}

/*
 * populate_handler_addrs
 *
 * Takes a handler and populates the addresses of the labels.
 * If any labels are not defined the address if set to -1.
 * Returns 0 on success, -1 if any labels are not defined.
 */
static void populate_handler_addrs( handler_node *handler )
{
  if ( (handler->handle_addr = get_symbol_addr( handler->handle_lbl )) < 0 )
  {
    error("handle symbol '%s' in handler declaration not defined", 
          handler->handle_lbl );
    errorCount += 1;
  }
  if ( (handler->start_addr = get_symbol_addr( handler->start_lbl )) < 0 )
  {
    error("start symbol '%s' in handler declaration not defined", 
          handler->start_lbl );
    errorCount += 1;
  }
  if ( (handler->end_addr = get_symbol_addr( handler->end_lbl )) < 0 )
  {
    error("end symbol '%s' in handler declaration not defined", 
          handler->end_lbl );
    errorCount += 1;
  }
}

/*
 * verify_handler_list
 *
 * Takes the head of a list of handlers and verifies each one.
 * This involves verifying the symbols are defined and filling
 * in their address in the handler structs.
 */
static void verify_handler_list( handler_node *root )
{
  handler_node *walk = root;
  while( walk )
  {
    populate_handler_addrs(walk);
    walk = walk->link;
  }
}

/*
 * verify_handlers
 *
 * Takes a list of functions and for each function
 * verifies the labels in the exception handler
 * declarations exist and populates their addresses.
 */
void verify_handlers( func_node *root )
{
  func_node *walk = root;
  while (walk)
  {
    verify_handler_list( walk->handler_list );
    walk = walk->link;
  }
}

// encodeAddr20
//
// given a symbol and the current location, encode the reference to
// the symbol
//
static int encodeAddr20(char* id, unsigned int pc)
{
  SYMTAB_REC *p = symtabLookup(id);

  if (p == NULL)
  {
    bug("encodeAddr20: %s not found in symtab", id);
  }

  // if the symbol is not defined, then just return 0
  if (!p->isDefined)
  {
    if (!p->isImported)
    {
      bug("encodeAddr20: %s not defined and not imported", id);
    }
    return 0;
  }

  int ret = (p->addr - pc);

  // check if PC-relative address will fit in 20 bits
  if (!fitIn20(ret))
  {
    bug("encodeAddr20: address will not fit in 20 bits for %s", id);
  }

  // return the PC-relative address
  return ret;
}

// encodeAddr16
//
// given a symbol and the current location, encode the reference to
// the symbol
//
static int encodeAddr16(char* id, unsigned int pc)
{
  SYMTAB_REC *p = symtabLookup(id);

  if (p == NULL)
  {
    bug("encodeAddr16: %s not found in symtab", id);
  }

  // if the symbol is not defined, then just return 0
  if (!p->isDefined)
  {
    if (!p->isImported)
    {
      bug("encodeAddr16: %s not defined and not imported", id);
    }
    return 0;
  }

  int ret = (p->addr - pc);

  // check if PC-relative address will fit in 16 bits
  if (!fitIn16(ret))
  {
    bug("encodeAddr16: address will not fit in 16 bits for %s", id);
  }

  // return the PC-relative address
  return ret;
}

/*
 * fit_in_8
 *
 * check if a signed value will fit in 8 bits
 */
static unsigned int fit_in_8(int value)
{
  if ((value >> 7) & 0x01)
  {
    if (((value >> 8) & 0xFFFF) != 0xFFFF)
    {
      return 0;
    }
  }
  else
  {
    if (((value >> 8) & 0xFFFF) != 0)
    {
      return 0;
    }
  }
  return 1;
}

// fitIn16
//
// check if signed value will fit in 16 bits
//
static unsigned int fitIn16(int value)
{
  if ((value >> 15) & 0x01)
  {
    if (((value >> 16) & 0xFFFF) != 0xFFFF)
    {
      return 0;
    }
  }
  else
  {
    if (((value >> 16) & 0xFFFF) != 0)
    {
      return 0;
    }
  }
  return 1;
}

// fitIn20
//
// check if signed value will fit in 20 bits
//
static unsigned int fitIn20(int value)
{
  if ((value >> 19) & 0x01)
  {
    if (((value >> 20) & 0xFFF) != 0xFFF)
    {
      return 0;
    }
  }
  else
  {
    if (((value >> 20) & 0xFFF) != 0)
    {
      return 0;
    }
  }
  return 1;
}

//
// check to see if a reference to defined label with fit in the given format
//
// calls error to report errors and increments global errorCount
//
static void checkAddr(char *id, unsigned int def, unsigned int ref,
                     unsigned int format)
{
  if (format == 8)
  {
    if (!fitIn16(def - ref))
    {
      error("reference to label %s at address %d won't fit in 16 bits", id);
      errorCount += 1;
    }
  }
  else if ((format == 2) || (format == 5))
  {
    if (!fitIn20(def - ref))
    {
      error("reference to label %s at address %d won't fit in 20 bits", id);
      errorCount += 1;
    }
  }
  else
  {
    bug("unexpected format (%d) in checkAddr for label %s", format, id);
  }
}
