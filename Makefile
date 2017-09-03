CC = clang++
CFLAGS = -Wall -g -std=c++14

repl: repl.o parse.o logic.o
	$(CC) $(CFLAGS) repl.o parse.o logic.o -o bin/repl

repl.o: repl.cpp parse.h logic.h
	$(CC) $(CFLAGS) -c repl.cpp -o repl.o

parse.o: parse.cpp parse.h logic.h
	$(CC) $(CFLAGS) -c parse.cpp -o parse.o

logic.o: logic.cpp logic.h
	$(CC) $(CFLAGS) -c logic.cpp -o logic.o

clean:
	rm -f bin/repl repl.o parse.o logic.o