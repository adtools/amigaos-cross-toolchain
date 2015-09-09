CC = m68k-amigaos-gcc -noixemul -s
CXX = m68k-amigaos-g++ -noixemul -s
CFLAGS = -Os -Wall -fomit-frame-pointer
CXXFLAGS = -Os -Wall -fomit-frame-pointer

BINS = hello-ks13 hello-ks20 hello-stdio hello-iostream hello-mui test-mmu
OBJS = simple-device.o simple-library.o

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

# normal library
lib: CC = m68k-amigaos-gcc -nostdlib -fbaserel
lib: libinit.o simple-library.o

# multibase library
libr: CC = m68k-amigaos-gcc -nostdlib -fbaserel
libr: libinitr.o simple-library.o

# device
dev: CC = m68k-amigaos-gcc -nostdlib -fbaserel
dev: devinit.o simple-device.o

clean:
	rm -f $(BINS)
	rm -f *.o *~
