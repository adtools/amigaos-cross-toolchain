CC = gcc
CFLAGS = -O2 -Wall -I../target/include

BINS = GccFindHit hunk2aout

all: $(BINS)

GccFindHit.o: GccFindHit.c defs.h a.out.h
hunk2aout.o: hunk2aout.c a.out.h

clean:
	rm -f $(BINS) *.o *~

# vim: set noexpandtab ts=8 sw=8 :
