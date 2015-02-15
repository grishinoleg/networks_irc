# This makefile does NOT create the sircd for you.
# You'll want to compile debug.o into sircd to get
# the debugging functions.
# We suggest adding three targets:  all, clean, test

CFLAGS=-Wall -DDEBUG

thisdoesnothingbutbreakthismakefile:

debug.o: debug-text.h debug.c

debug-text.h: debug.h
	./dbparse.pl < debug.h > debug-text.h
