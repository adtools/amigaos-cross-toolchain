CC = m68k-amigaos-gcc -noixemul -s
CXX = m68k-amigaos-g++ -noixemul -s
CFLAGS = -Os -Wall -fomit-frame-pointer
CXXFLAGS = -Os -Wall -fomit-frame-pointer

BINS = hello-ks13 hello-ks20 hello-stdio hello-iostream hello-mui test-mmu \
       simple.library simple.library_r simple.device

all: $(BINS) $(OBJS)

hello-ks13: CC += -fbaserel 
hello-ks13: CFLAGS += -m68000 -msmall-code
hello-ks13: LDLIBS = -lnix13
hello-ks13: hello-ks13.c

hello-ks20: CC += -fbaserel32
hello-ks20: CFLAGS += -m68020 -msmall-code
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

simple.library: simple-library.c
	$(CC) -ramiga-lib -fbaserel $(CFLAGS) -o $@ $^

simple.library_r: simple-library.c
	$(CC) -ramiga-libr -resident $(CFLAGS) -o $@ $^

simple.device: simple-device.c
	$(CC) -ramiga-dev -fbaserel $(CFLAGS) -o $@ $^

clean:
	rm -f $(BINS)
	rm -f *.o *~
