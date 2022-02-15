CC = clang
COpts = -std=c99 -Wall -g

default: lizp

lizp: main.c lizp_string.o cell.o function.o reader.o printer.o lizp.o
	$(CC) $(COpts) -o $@ $^

%.c: %.h

%.o: %.c
	$(CC) $(COpts) -c $^

clean:
	rm *.o
	rm lizp

