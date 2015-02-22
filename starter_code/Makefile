# This makefile does NOT create the sircd for you.
# You'll want to compile debug.o into sircd to get
# the debugging functions.
# We suggest adding three targets:  all, clean, test

CFLAGS	= -Wall -DDEBUG -O3 -std=gnu11 -I.
CC		= gcc
OBJECTS = debug.o irc_proto.o sircd.o rtlib.o
# csapp.o
LIB		= -lpthread

all: clean sircd

debug-text.h: debug.h
	./dbparse.pl < debug.h > debug-text.h

debug.o: debug-text.h debug.h debug.c
	$(CC) $(CFLAGS) -c debug.c -o debug.o

irc_proto.o: irc_proto.c irc_proto.h
	$(CC) $(CFLAGS) -c irc_proto.c -o irc_proto.o

sircd.o: sircd.c sircd.h
	$(CC) $(CFLAGS) -c sircd.c -o sircd.o

rtlib.o: rtlib.c rtlib.h
	$(CC) $(CFLAGS) -c rtlib.c -o rtlib.o

# csapp.o: csapp.c csapp.h
# 	$(CC) $(CFLAGS) -c csapp.c -o csapp.o

sircd: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o sircd $(LIB)

.PHONY : clean
clean:
	-rm -f sircd
	-rm -f *.o