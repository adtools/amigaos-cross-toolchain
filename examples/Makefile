CC = m68k-amigaos-gcc -s
CXX = m68k-amigaos-g++ -s
CFLAGS = -Os -Wall -fomit-frame-pointer
CXXFLAGS = -Os -Wall -fomit-frame-pointer

BINS = hello-ks13 hello-ks20 hello-ks20.clib2 hello-stdio hello-stdio.clib2 \
       hello-iostream hello-mui test-mmu \
       simple.library simple.library_r simple.device

all: $(BINS) $(OBJS)

hello-ks13: hello-ks13.c
	$(CC) -noixemul -fbaserel -m68000 -msmall-code $(CFLAGS) -o $@ $< -lnix13

hello-ks20: hello-ks20.c
	$(CC) -noixemul -fbaserel32 -m68020 -msmall-code $(CFLAGS) -o $@ $<

hello-ks20.clib2: hello-ks20.c
	$(CC) -mcrt=clib2 -fbaserel32 -m68020 -msmall-code $(CFLAGS) -o $@ $<

hello-stdio: hello-stdio.c
	$(CC) -noixemul -m68020 $(CFLAGS) -o $@ $<

hello-stdio.clib2: hello-stdio.c
	$(CC) -mcrt=clib2 -m68020 $(CFLAGS) -o $@ $<

hello-iostream: hello-iostream.cpp
	$(CXX) -noixemul -m68020 $(CXXFLAGS) -o $@ $<

hello-mui: hello-mui.c
	$(CC) -noixemul -m68020 -msmall-code $(CFLAGS) -o $@ $< -lmui

test-mmu: test-mmu.c
	$(CC) -noixemul -m68060 -msmall-code $(CFLAGS) -o $@ $<

simple.library: simple-library.c
	$(CC) -noixemul -ramiga-lib -fbaserel $(CFLAGS) -o $@ $<

simple.library_r: simple-library.c
	$(CC) -noixemul -ramiga-libr -resident $(CFLAGS) -o $@ $<

simple.device: simple-device.c
	$(CC) -noixemul -ramiga-dev -fbaserel $(CFLAGS) -o $@ $<

clean:
	rm -f $(BINS)
	rm -f *.o *~
