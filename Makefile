CFLAGS=-std=c11 -Wall -Wextra -Werror 

all:
	gcc chip8.c -I src/include -L src/lib -o chip8 $(CFLAGS) -lmingw32 -lSDL2main -lSDL2