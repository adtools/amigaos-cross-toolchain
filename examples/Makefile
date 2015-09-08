CC = m68k-amigaos-gcc -noixemul -s
CXX = m68k-amigaos-g++ -noixemul -s
CFLAGS = -Os -Wall -fomit-frame-pointer
CXXFLAGS = -Os -Wall -fomit-frame-pointer

BINS = hello-ks13 hello-ks20 hello-stdio hello-iostream hello-mui test-mmu

all: $(BINS)

hello-ks13: CC += -fbaserel 
hello-ks13: CFLAGS += -m68000 -msmall-code
hello-ks13: LDLIBS = -lnix13
hello-ks13: hello-ks13.c

# MacOSX BUG: hello-ks20: CC += -fbaserel 
hello-ks20: CFLAGS += -m68000 -msmall-code
hello-ks20: hello-ks20.c

hello-stdio: CFLAGS += -m68020
hello-stdio: hello-stdio.c

hello-iostream: CXXFLAGS += -m68020
hello-iostream: hello-iostream.cpp

hello-mui: CFLAGS += -m68020 -msmall-code
hello-mui: LDLIBS = -lmui
hello-mui: hello-mui.c

test-mmu: CFLAGS += -m68060 -msmall-code
test-mmu: test-mmu.c

clean:
	rm -f $(BINS)
	rm -f *.o *~
