/* defs.h -- this file is part of GccFindHit
*  Copyright (C) 1995 Daniel Verite -- daniel@brainstorm.eu.org
*  This program is distributed under the General GNU Public License version 2
*  See the file COPYING for information about the GPL
*/

#if BYTE_ORDER==LITTLE_ENDIAN
/* reverse for endianness */
#define GETLONG(x) ((((((x)&0xff)<<8) | (((x)&0xff00)>>8))<<16) | \
		   (((((x)&0xff000000)>>8)&0x00ff0000) | \
		   ((x)&0x00ff0000)<<8)>>16)
#define GETWORD(x) ((((x)&0xff)<<8) | ((((x)&0xff00)>>8)&0xff))
#else
#define GETLONG(x) (x)
#define GETWORD(x) (x)
#endif

#define	ZMAGIC	0x10b	/* demand-paged executable */
#define	N_SO	0x64
#define	N_SOL	0x84
#define	N_SLINE	0x44

/* Converts an SLINE value to an offset in the text section.
   This definition is OK for ld 1.8, currently used on the Amiga AFAIK,
   but you may change that for another linker */
#define OFFSET_N_SLINE(x) (x)

enum {
  HUNK_UNIT=0x3e7,
  HUNK_NAME,
  HUNK_CODE,
  HUNK_DATA,
  HUNK_BSS,
  HUNK_RELOC32,
  HUNK_RELOC16,
  HUNK_RELOC8,
  HUNK_EXT,
  HUNK_SYMBOL,
  HUNK_DEBUG,
  HUNK_END,
  HUNK_HEADER,
  HUNK_3F4, /* ? */
  HUNK_OVERLAY,
  HUNK_BREAK,
  HUNK_DRELOC32,
  HUNK_DRELOC16,
  HUNK_DRELOC8,
  HUNK_3FA, /* ? */
  HUNK_LIB,
 /* AmigaOS Manual 3rd ed. takes 0x3fc for both HUNK_INDEX and
    HUNK_RELOC32SHORT. I don't know if it's an error or something;
    anyway, there shouldn't be HUNK_INDEX in executable files, so
    let's take the other one */
  HUNK_RELOC32SHORT
};

/* amigaos hunk header structure */
struct Header {
  long	nb_hunks;
  long	first;
  long	last;
  long	sizes[1];
};

/* bsd header structure */
struct bsd_header{
  long	magic;
  long	symsz;
  long	strsz;
};

typedef struct {
  long strx;
  unsigned char	type;
  unsigned char other;
  unsigned short desc;
  long	value;
} BSD_SYM;

