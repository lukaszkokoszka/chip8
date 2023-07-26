CC=gcc
LIBS=`pkg-config --libs sdl2`
CFLAGS=`pkg-config --cflags sdl2`

all:
	$(CC) chip8.c $(LIBS) $(CFLAGS) -o chip8 
