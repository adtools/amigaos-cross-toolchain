/* GccFindHit V1.2.1, 07/12/96
*
*  The same than FindHit by Michael Sinz, but for GCC users
*  Usage: GccFindHit <executable file> <list of offsets in hexadecimal>
*  The file should have been linked with the '-g' flag passed to gcc
*  to turn on debugging information (the 'stabs')
*
*  GccFindHit outputs the line numbers matching with the offsets given by
*  Enforcer (or whatever). Currently, there is no need
*  to provide the hunk number because it should always be zero.
*
*  Copyright (C) 1995 Daniel Verite -- daniel@brainstorm.eu.org
*  This program is distributed under the General GNU Public License version 2
*  See the file COPYING for information about the GPL
*
*  v1.2.1, 07/12/96
*    David Zaroski, cz253@cleveland.Freenet.Edu:
*  o use BYTE_ORDER from system includes to get the host's endianness
*
*    Daniel Verite, daniel@brainstorm.eu.org:
*  o removed references to LITTLE_ENDIAN in Makefile.in
*
*  v1.2, 30/12/95
*
*    Daniel Verite, daniel@brainstorm.eu.org:
*  o added handling of HUNK_NAME
*  o fixed a small glitch in the strings seeks
*
*  v1.1, 28/09/95
*
*    Hans Verkuil, hans@wyst.hobby.nl:
*  o GccFindHit no longer reads the strings into memory, but seek()s them
*    instead, saving lots of memory.
*  o added version string.
*
*  v1.0, 22/05/95
*
*    First release
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <errno.h>

#include "defs.h"

typedef struct nlist BSD_SYM;

static int ExeFile,NbOffs;
static int32_t *SearchOffs;

const char version_id[] = "\000$VER: GccFindHit 1.2.1 (07.12.96)";

int Read4 (int32_t *buf)
{
  if (read (ExeFile,buf,4)==4) {
    *buf = GETLONG(*buf);
    return 1;
  }
  else
    return 0;
}

struct Header *ReadHeader ()
{
  int32_t nb,size;
  struct Header *h;
  int i;

  /* skip hunk names */
  while (Read4 (&size) && size)
    lseek (ExeFile, size<<2, SEEK_CUR);

  /* reads the number of hunks */
  if (!Read4(&nb))
    return NULL;
  h = (struct Header*)malloc(sizeof(struct Header)+(nb-1)*sizeof(int32_t));
  if (!h)
    return NULL;
  h->nb_hunks = nb;
  if (Read4 (&h->first) && Read4 (&h->last)) {
    for (i=0; i<nb; i++)
      if (!Read4 (&h->sizes[i])) {
	free (h);
	return NULL;
      }
  }
  return h;
} /* ReadHeader() */

int long_cmp (int32_t *e1,int32_t *e2)
{
  return (*e1)<(*e2);
}

void SkipRelocation ()
{
  int32_t no; /* number of offsets */
  int32_t h;  /* hunk number */
  while (Read4 (&no) && no && Read4 (&h))
    lseek (ExeFile, no<<2, SEEK_CUR);
}

/* this function hasn't been tested; AFAIK, ld won't output short relocs,
   so it's useless for now */
void SkipShortRel ()
{
  int32_t no;
  int16_t h;
  while (Read4 (&no) && no && read (ExeFile, &h, sizeof h))
    lseek (ExeFile, no<<1, SEEK_CUR);
}

/* can be slow if I/O buffering doesn't do some read-ahead */
void SkipSymbols ()
{
  int32_t nl; /* name length in long words */
  while (Read4 (&nl) && nl) {
    /* skips the name + the value */
    lseek (ExeFile, (nl+1)<<2, SEEK_CUR);
  }
}

/* skip hunks such as HUNK_NAME that have their size in the first long */
void SkipHunk ()
{
  int32_t size;
  if (Read4 (&size))
    lseek (ExeFile, size<<2, SEEK_CUR);
}

char *get_string(int32_t offset)
{
  static char buf[256];

  lseek(ExeFile, offset, SEEK_SET);
  read(ExeFile, buf, 255);
  buf[255] = 0;
  return buf;
}

void GetLines (int32_t symsz, BSD_SYM *syms, int32_t string_offset)
{
  int32_t nbsyms = symsz / sizeof(BSD_SYM);
  BSD_SYM *sym = syms;
  uint8_t prev_type;
  int32_t srcname = 0, prev_src = 0;
  uint16_t prev_line = 0;
  uint32_t offs , prev_offs = -1UL;
  int i;

  while (nbsyms--) {
    switch (sym->n_type) {
      case N_SO:
      case N_SOL:
        srcname = GETLONG (sym->n_un.n_strx);
        break;
      case N_SLINE:
        offs = OFFSET_N_SLINE (GETLONG (sym->n_value));
        for (i = 0; i < NbOffs; i++) {
          if (SearchOffs[i] >= prev_offs && SearchOffs[i] < offs) {
            printf ("%s: line %hd, offset 0x%x\n", get_string(prev_src +
                  string_offset), prev_line, prev_offs);
          }
        }
        prev_offs = offs;
        prev_line = GETWORD (sym->n_desc);
        prev_src = srcname;
        break;
    }
    prev_type = sym->n_type;
    sym++;
  }
  /* the last SLINE is a special case */
  for (i = 0; i < NbOffs; i++) {
    if (SearchOffs[i] == prev_offs) {
      printf ("%s: line %hd, offset 0x%x\n",
          get_string(prev_src + string_offset), prev_line,
          prev_offs);
    }
  }
}

void HunkDebug (void)
{
  int32_t hunksz, symsz, strsz;
  struct bsd_header hdr;
  int32_t pos, init_pos = lseek (ExeFile, 0, SEEK_CUR);
  char *syms;

  if (init_pos < 0)
    return;
  if (Read4(&hunksz) && read (ExeFile, &hdr, sizeof(hdr)) == sizeof(hdr)) {
    if (GETLONG(hdr.magic)==ZMAGIC) {
      /* seems to be gcc-compiled */
      strsz = GETLONG (hdr.strsz);
      symsz = GETLONG (hdr.symsz);
      if (strsz + symsz != 0) {
        syms = (char*)malloc (symsz);
        if (syms) {
          if (read (ExeFile, syms, symsz) == symsz) {
            pos = lseek(ExeFile, strsz, SEEK_CUR);
            if (pos > 0)
              GetLines (symsz, (BSD_SYM*)syms, pos - strsz);
          }
          free (syms);
        }
      }
    }
  }
  /* go to the end of the hunk whatever happened before */
  lseek (ExeFile, init_pos+((hunksz+1)<<2), SEEK_SET);
}

void DoHunks (struct Header *h)
{
  int32_t hnum,size,nsec=0;
  while (Read4 (&hnum)) {
    switch (hnum) {
    case HUNK_NAME:
      SkipHunk ();
      break;
    case HUNK_CODE:
    case HUNK_DATA:
      if (Read4 (&size)) {
	nsec++;
	lseek (ExeFile, (size&0x3fffffff)<<2, SEEK_CUR);
      }
      break;
    case HUNK_BSS:
      nsec++;
      Read4 (&size);
    case HUNK_END:
    case HUNK_BREAK:
      break;
    case HUNK_RELOC32:
    case HUNK_RELOC16:
    case HUNK_RELOC8:
    case HUNK_DREL32:
    case HUNK_DREL16:
    case HUNK_DREL8:
      SkipRelocation();
      break;
    case HUNK_RELOC32SHORT:
      SkipShortRel ();
      break;
    case HUNK_SYMBOL:
      SkipSymbols();
      break;
    case HUNK_DEBUG: /* here we are... */
      HunkDebug ();
      break;
    default:
      fprintf (stderr, "Unexpected hunk 0x%x\n", hnum);
      return;
    }
  }
} /* DoHunks() */

void Out(int code)
{
  if (ExeFile>0) close (ExeFile);
  if (SearchOffs) free (SearchOffs);
  exit (code);
}

int main(int argc,char **argv)
{
  int32_t HunkNum;
  struct Header *header=NULL;
  int i;

  if (argc<3) {
    fprintf (stderr,"Usage: %s <file> <hex offsets>\n",argv[0]);
    Out (1);
  }
  ExeFile = open (argv[1], O_RDONLY);
  if (ExeFile<0) {
    fprintf (stderr,"can't open %s:%s\n", argv[1], strerror (errno));
    Out (1);
  }
  NbOffs = argc-2;
  SearchOffs = (int32_t*)malloc (sizeof (int32_t)*NbOffs);
  if (!SearchOffs) {
    fprintf (stderr,"No memory\n");
    Out (1);
  }
  for (i=0; i<NbOffs; i++) {
    if (sscanf (argv[i+2],"%x",&SearchOffs[i])!=1) {
      fprintf (stderr, "Operand %s is not an hex offset\n", argv[i+2]);
      Out (1);
    }
  }
  if (!Read4(&HunkNum) || HunkNum!=HUNK_HEADER || !(header=ReadHeader())) {
    fprintf (stderr, "%s is not an amigaos executable\n", argv[1]);
    Out (1);
  }
  DoHunks (header);
  free (header);
  Out (0);
  return 0; /* another brick in the -Wall */
} /* main() */

