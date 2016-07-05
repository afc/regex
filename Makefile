CC = gcc
CFLAGS = -g -DDEBUG -Wall -std=c99
DEPS = globals.h parse.h compile.h vm.h
OBJS =    main.o parse.o compile.o vm.o
BIN = regex

%.o: %.c $(DEPS)
	$(CC) -c $(CFLAGS) -o $@ $<

all: build

build: $(OBJS)
	$(CC) $(CFLAGS) -o $(BIN) $^
	rm -f *.o *~

rebuild: clean all

.PHONY: clean

clean:
	rm -f *.o *~ $(BIN)
