CC = m68k-amigaos-gcc -noixemul -s
CXX = m68k-amigaos-g++ -noixemul -s
CFLAGS = -Os -Wall -fomit-frame-pointer
CXXFLAGS = -Os -Wall -fomit-frame-pointer

BINS = hello-ks13 hello-ks20 hello-stdio hello-iostream hello-mui test-mmu \
       simple.library simple.r.library simple.device

all: $(BINS) $(OBJS)

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

simple-device.o: CC += -fbaserel 
simple-device.o: simple-device.c

simple-library.o: CC += -fbaserel 
simple-library.o: simple-library.c

simple.library: simple-library.o
	$(CC) -fbaserel -amiga-lib $(CFLAGS) -o $@ $^

simple.r.library: simple-library.o
	$(CC) -fbaserel -amiga-libr $(CFLAGS) -o $@ $^

simple.device: simple-device.o
	$(CC) -fbaserel -amiga-dev $(CFLAGS) -o $@ $^

clean:
	rm -f $(BINS)
	rm -f *.o *~
