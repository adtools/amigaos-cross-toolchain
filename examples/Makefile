CC = m68k-amigaos-gcc -noixemul -s
CFLAGS = -Os -Wall -fomit-frame-pointer

all: hello

hello: LDLIBS = -lnix13
hello: CC += -fbaserel 
hello: CFLAGS += -m68000 -msmall-code
hello: hello.c

clean:
	rm -f hello
	rm -f *.o *~
