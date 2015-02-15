# This makefile does NOT create the sircd for you.
# You'll want to compile debug.o into sircd to get
# the debugging functions.
# We suggest adding three targets:  all, clean, test

CFLAGS=-Wall -DDEBUG -I.
CC=gcc
OBJECTS=debug.o irc_proto.o sircd.o

all: clean sircd

debug-text.h: debug.h
	./dbparse.pl < debug.h > debug-text.h

debug.o: debug-text.h debug.h debug.c
	$(CC) $(CFLAGS) -c debug.c -o debug.o

irc_proto.o: irc_proto.c irc_proto.h
	$(CC) $(CFLAGS) -c irc_proto.c -o irc_proto.o

sircd.o: sircd.c sircd.h
	$(CC) $(CFLAGS) -c sircd.c -o sircd.o

sircd: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o sircd

.PHONY : clean
clean:
	-rm -f sircd
	-rm -f *.o