CC = gcc -m32
CFLAGS = -g -O2 -Wall -I../target/os-include

BINS = GccFindHit hunk2aout elf2hunk

all: $(BINS)

GccFindHit.o: GccFindHit.c defs.h a.out.h
hunk2aout.o: hunk2aout.c a.out.h
elf2hunk.o: elf2hunk.c

clean:
	rm -f $(BINS) *.o *~

# vim: set noexpandtab ts=8 sw=8 :
