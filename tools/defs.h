/* defs.h -- this file is part of GccFindHit
*  Copyright (C) 1995 Daniel Verite -- daniel@brainstorm.eu.org
*  This program is distributed under the General GNU Public License version 2
*  See the file COPYING for information about the GPL
*/

#ifndef _DEFS_H_
#define _DEFS_H_

#include <endian.h>
#include <dos/doshunks.h>

#include "a.out.h"

#define GETWORD(x) be16toh(x)
#define GETLONG(x) be32toh(x)
#define PUTWORD(x) htobe16(x)
#define PUTLONG(x) htobe32(x)

/* Converts an SLINE value to an offset in the text section.
   This definition is OK for ld 1.8, currently used on the Amiga AFAIK,
   but you may change that for another linker */
#define OFFSET_N_SLINE(x) (x)

/* amigaos hunk header structure */
struct Header {
  int32_t	nb_hunks;
  int32_t	first;
  int32_t	last;
  int32_t	sizes[1];
};

/* bsd header structure */
struct bsd_header{
  int32_t	magic;
  int32_t	symsz;
  int32_t	strsz;
};

#endif
