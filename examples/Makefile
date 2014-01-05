CC = m68k-amigaos-gcc -noixemul -s
CFLAGS = -Os -Wall -fomit-frame-pointer

all: hello hello-mui

hello: LDLIBS = -lnix13
hello: CC += -fbaserel 
hello: CFLAGS += -m68000 -msmall-code
hello: hello.c

hello-mui: CC += -fbaserel
hello-mui: CFLAGS += -m68020 -msmall-code
hello-mui: LDLIBS = -lmui
hello-mui: hello-mui.c

clean:
	rm -f hello hello-mui
	rm -f *.o *~
