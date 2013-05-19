/* BFD back-end for Amiga style m68k a.out binaries.
   Copyright (C) 1990, 1991, 1992, 1993, 1994 Free Software Foundation, Inc.
   Contributed by Stephan Thesing.

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#define TARGETNAME "a.out-amiga"
#define MACHTYPE_OK(m) ((m)==M_UNKNOWN || (m)==M_68010 || (m)==M_68020)
#define TARGET_IS_BIG_ENDIAN_P
#define TARGET_PAGE_SIZE 0x2000
#define N_HEADER_IN_TEXT(x) 0
#define N_SHARED_LIB(x) 0
#define TEXT_START_ADDR 0

/* Do not "beautify" the CONCAT* macro args.  Traditional C will not
   remove whitespace added here, and thus will fail to concatenate
   the tokens.  */
#define MY(OP) CONCAT2 (aout_amiga_,OP)

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "libaout.h"
#include "aout/aout64.h"

bfd_boolean
MY(final_link) PARAMS ((bfd *, struct bfd_link_info *));

bfd_boolean
amiga_final_link PARAMS ((bfd *, struct bfd_link_info *));
#define MY_bfd_final_link amiga_final_link

bfd_byte *
get_relocated_section_contents PARAMS ((bfd *, struct bfd_link_info *,
	struct bfd_link_order *, bfd_byte *, bfd_boolean, asymbol **));
#define MY_bfd_get_relocated_section_contents get_relocated_section_contents

static unsigned long MY(get_mach) PARAMS ((enum machine_type));
static bfd_boolean MY(write_object_contents) PARAMS ((bfd *));
static bfd_boolean MY(set_sizes) PARAMS ((bfd *));
static bfd_boolean MY(link_add_symbols) PARAMS ((bfd *, struct bfd_link_info *));
#define MY_bfd_link_add_symbols aout_amiga_link_add_symbols

static unsigned long
MY(get_mach) (machtype)
     enum machine_type machtype;
{
  unsigned long machine;
  switch (machtype)
    {
    default:
    case M_UNKNOWN:
      /* Some Sun3s make magic numbers without cpu types in them, so
	 we'll default to the 68000. */
      machine = bfd_mach_m68000;
      break;

    case M_68010:
      machine = bfd_mach_m68010;
      break;

    case M_68020:
      machine = bfd_mach_m68020;
      break;
    }
  return machine;
}
#define SET_ARCH_MACH(ABFD, EXEC) \
  bfd_set_arch_mach (ABFD, bfd_arch_m68k, MY(get_mach) (N_MACHTYPE (EXEC)))

static bfd_boolean
MY(write_object_contents) (abfd)
     bfd *abfd;
{
  struct external_exec exec_bytes;
  struct internal_exec *execp = exec_hdr (abfd);

  /* Magic number, maestro, please!  */
  switch (bfd_get_arch (abfd))
    {
    case bfd_arch_m68k:
      switch (bfd_get_mach (abfd))
	{
	case bfd_mach_m68000:
	  N_SET_MACHTYPE (*execp, M_UNKNOWN);
	  break;
	case bfd_mach_m68010:
	  N_SET_MACHTYPE (*execp, M_68010);
	  break;
	default:
	case bfd_mach_m68020:
	  N_SET_MACHTYPE (*execp, M_68020);
	  break;
	}
      break;
    default:
      N_SET_MACHTYPE (*execp, M_UNKNOWN);
    }

  WRITE_HEADERS (abfd, execp);

  return TRUE;
}
#define MY_write_object_contents MY(write_object_contents)

static bfd_boolean
MY(set_sizes) (abfd)
     bfd *abfd;
{
  adata (abfd).page_size = TARGET_PAGE_SIZE;
  adata (abfd).segment_size = TARGET_PAGE_SIZE;
  adata (abfd).exec_bytes_size = EXEC_BYTES_SIZE;
  return TRUE;
}
#define MY_set_sizes MY(set_sizes)

/* Include the usual a.out support.  */
#include "aout-target.h"

/* Add symbols from an object file to the global hash table.  */
static bfd_boolean
MY(link_add_symbols) (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  if (info->hash->creator->flavour == bfd_target_amiga_flavour)
    return _bfd_generic_link_add_symbols (abfd, info);
  return NAME(aout,link_add_symbols) (abfd, info);
}

/* Public final_link routine.  */
bfd_boolean
MY(final_link) (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  return NAME(aout,final_link) (abfd, info, MY_final_link_callback);
}
