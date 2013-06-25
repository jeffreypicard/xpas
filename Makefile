#
# Makefile for xpas assembler for xpvm
#

CC = gcc
CFLAGS = -g -Wall

YACC = bison

LEX = flex

xpas: scan.o main.o parse.o message.o assemble.o
	$(CC) $(CFLAGS) scan.o main.o parse.o message.o assemble.o -o xpas

scan.o: y.tab.h defs.h

scan.c:  scan.l
	$(LEX) scan.l
	mv lex.yy.c scan.c

y.tab.h parse.c: parse.y
	$(YACC) -d -y parse.y
	mv y.tab.c parse.c
	$(CC) $(CFLAGS) -c parse.c

main.o: 

parse.o: defs.h 

message.o: 

assemble.o: defs.h 

lexdbg: scan.l y.tab.h
	$(LEX) scan.l
	$(CC) -DDEBUG lex.yy.c message.c -lfl -o lexdbg
	rm lex.yy.c

y.output: parse.y
	$(YACC) -v -y parse.y

clean:
	-rm *.o parse.c scan.c y.tab.h lexdbg
	-rm xpas y.output

