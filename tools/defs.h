/* defs.h -- this file is part of GccFindHit
*  Copyright (C) 1995 Daniel Verite -- daniel@brainstorm.eu.org
*  This program is distributed under the General GNU Public License version 2
*  See the file COPYING for information about the GPL
*/

#ifndef _DEFS_H_
#define _DEFS_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <dos/doshunks.h>

#include "a.out.h"

#define GETWORD(x) ntohs(x)
#define GETLONG(x) ntohl(x)
#define PUTWORD(x) htons(x)
#define PUTLONG(x) htonl(x)

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
