CC = m68k-amigaos-gcc -noixemul -s
CFLAGS = -Os -Wall -fomit-frame-pointer

all: hello hello-mui test-mmu

hello: CC += -fbaserel 
hello: CFLAGS += -m68000 -msmall-code
hello: LDLIBS = -lnix13
hello: hello.c

hello-mui: CC += -fbaserel
hello-mui: CFLAGS += -m68020 -msmall-code
hello-mui: LDLIBS = -lmui
hello-mui: hello-mui.c

test-mmu: CC += -fbaserel
test-mmu: CFLAGS += -m68060 -msmall-code
test-mmu: test-mmu.c

clean:
	rm -f hello hello-mui test-mmu
	rm -f *.o *~
