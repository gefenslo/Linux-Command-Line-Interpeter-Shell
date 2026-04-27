CC=gcc
CFLAGS=-Wall -g -m32

all: myshell mypipe looper


myshell: myShell.c lineParser.c looper
	$(CC) $(CFLAGS) -o myshell myShell.c lineParser.c


mypipe: myPipe.c
	$(CC) $(CFLAGS) -o mypipe myPipe.c


looper: looper.c
	$(CC) $(CFLAGS) -o looper looper.c

.PHONY: clean all


clean:
	rm -f myshell mypipe looper
