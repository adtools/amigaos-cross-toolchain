/* BFD back-end for Commodore-Amiga AmigaOS binaries.
   Copyright (C) 1990-1994 Free Software Foundation, Inc.
   Contributed by Leonard Norrgard.  Partially based on the bout
   and ieee BFD backends and Markus Wild's tool hunk2gcc.
   Revised and updated by Stephan Thesing 

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

/*
SECTION
	amiga back end

This section describes the overall structure of the Amiga BFD back end.
The linker stuff can be found in @xref{amigalink}.
@menu
@* implementation::
@* amigalink::
@end menu

INODE
implementation, amigalink, amiga, amiga


SECTION
	implementation


The need for a port of the bfd library for Amiga style object (hunk) files
arose by the desire to port the GNU debugger gdb to the Amiga.
Also, the linker ld should be updated to the current version (2.5.2).
@@*
This port bases on the work done by Leonard Norrgard, who started porting
gdb. Raphael Luebbert, who supports the ixemul.library, has also worked on
implementing the needed @code{ptrace()} system call and gas2.5.

@menu
@* not supported::
@* Does it work ?::
@* TODO::
@end menu

INODE
not supported, Does it work ?,implementation,implementation
SUBSECTION
	not supported

Currently, the implementation does not support Amiga link library files, like
e.g. amiga.lib. This may be added in a later version, if anyone starts work
on it, or I find some time for it.

The handling of the symbols in hunk files is a little bit broken:
	  o The symbols in a load file are totally ignored at the moment, so gdb and gprof
            do not work.
	  o The symbols of a object module (Hunk file, starting with HUNK_UNIT) are read in
            correctly, but HUNK_SYMBOL hunks are also ignored.

The reason for this is the following:
Amiga symbol hunks do not allow for much information. Only a name and a value are allowed.
On the other hand, a.out format carries along much more information (see, e.g. the
entry on set symbols in the ld manual). The old linker copied this information into
a HUNK_DEBUG hunk. Now there is the choice:
	o ignoring the debug hunk, read in only HUNK_SYMBOL definitions => extra info is lost.
	o read in the debug hunk and use the information therein => How can clashs between the
          information in the debug hunk and HUNK_SYMBOL or HUNK_EXT hunks be avoided ?
I haven't decided yet, what to do about this.


Although bfd allows to link together object modules of different flavours, 
producing a.out style executables does not work on Amiga :-)
It should, however, be possible to create a.out files with the -r option of ld
(incremental link).

INODE 
Does it work ?,TODO ,not supported , implementation
SUBSECTION
	Does it work ?

Currently, the following utilities work:
	o objdump
	o objcopy
	o strip
	o nm
	o ar
	o gas

	
INODE
TODO, , Does it work ?, implementation
SUBSECTION
	TODO

	o fix fixme:s

@*
BFD:
	o add flag to say if the format allows multiple sections with the
          same name.  Fix bfd_get_section_by_name() and bfd_make_section()
          accordingly.
       
	o dumpobj.c: the disassembler: use relocation record data to find symbolic
           names of addresses, when available.  Needs new routine where one can
          specify the source section of the symbol to be printed as well as some
          rewrite of the disassemble functions.
       
*/

#include "bfd.h"
#include "bfdlink.h"
#include "sysdep.h"
#include "genlink.h"
#include "libbfd.h"
#include "libamiga.h"

#ifndef alloca
extern PTR alloca ();
#endif

typedef struct aout_symbol {
  asymbol symbol;
  short desc;
  char other;
  unsigned char type;
} aout_symbol_type;

#include "aout/aout64.h"

typedef struct amiga_ardata_struct {
  /* generic stuff */
  struct artdata generic;
  /* amiga-specific stuff */
  unsigned long filesize;
  struct arch_syms *defsyms;
  unsigned long defsym_count;
} amiga_ardata_type;

#define amiga_ardata(abfd) ((abfd)->tdata.amiga_ardata)

#define GL(x) bfd_get_32 (abfd, (bfd_byte *) (x))
#define GW(x) bfd_get_16 (abfd, (bfd_byte *) (x))
#define LONGSIZE(l) (((l)+3) >> 2)

#define DEBUG_AMIGA 10000

static long determine_datadata_relocs PARAMS ((bfd *, asection *));
static boolean amiga_write_section_contents PARAMS ((bfd *, asection *,
						     asection *, long, long*));
static boolean amiga_write_symbols PARAMS ((bfd *, asection *));
static boolean amiga_digest_file ();
static boolean amiga_mkobject ();
static boolean amiga_mkarchive ();
static boolean amiga_read_unit PARAMS ((bfd *, unsigned long));
static boolean amiga_read_load PARAMS ((bfd *));
static boolean amiga_handle_cdb_hunk PARAMS ((bfd *, unsigned long,
					     unsigned long, int,
					     unsigned long));
static boolean amiga_handle_rest PARAMS ((bfd *,asection *,boolean));
static boolean write_name PARAMS ((bfd*, const char*, long));

/* AmigaOS doesn't like symbols names longer than 124 characters */
#define MAX_NAME_SIZE 124

extern int amiga_pOS_flg;

#if DEBUG_AMIGA
#include <stdarg.h>
static void
error_print(const char *fmt, ...)
{
  va_list args;

  va_start (args, fmt);
  vfprintf (stderr, fmt, args);
  va_end (args);
}

#define DPRINT(L,x) if (L>=DEBUG_AMIGA) error_print x
#else
#define DPRINT(L,x) 
#endif

reloc_howto_type howto_hunk_reloc8 =
{
  HUNK_RELOC8, /* type */
  0,           /* rightshift */
  0,           /* size */
  8,           /* bitsize */
  true,        /* pc_relative */
  0,           /* bitpos */
  complain_overflow_bitfield,  /* complain_on_overflow */
  0,           /* special_function */
  "reloc8",    /* textual name */
  false,       /* partial_inplace? */
  0x000000ff,  /* src_mask */
  0x000000ff,  /* dst_mask */
  true         /* pcrel_offset */
};

reloc_howto_type howto_hunk_reloc16 =
{HUNK_RELOC16,0,1,16,true,0,complain_overflow_bitfield,0,"reloc16",false,0x0000ffff,0x0000ffff,true};

reloc_howto_type howto_hunk_reloc32 =
{HUNK_RELOC32,0,2,32,true,0,complain_overflow_bitfield,0,"reloc32",false,0xffffffff,0xffffffff,true};

reloc_howto_type howto_hunk_drel8 =
{HUNK_DREL8,0,0,8,false,0,complain_overflow_bitfield,0,"drel8",false,0x000000ff,0x000000ff,false};

reloc_howto_type howto_hunk_drel16 =
{HUNK_DREL16,0,1,16,false,0,complain_overflow_bitfield,0,"drel16",false,0x0000ffff,0x0000ffff,false};

reloc_howto_type howto_hunk_drel32 =
{HUNK_DREL32,0,2,32,false,0,complain_overflow_bitfield,0,"drel32",false,0xffffffff,0xffffffff,false};

reloc_howto_type *amiga_howto_array[2][3] =
{
  { &howto_hunk_reloc8, &howto_hunk_reloc16, &howto_hunk_reloc32 },
  { &howto_hunk_drel8, &howto_hunk_drel16, &howto_hunk_drel32 }
};

/* The following are gross hacks that need to be fixed.  The problem is
   that the linker unconditionally references these values without
   going through any of bfd's standard interface.  Thus they need to
   be defined in a bfd module that is included in *all* configurations,
   and are currently in bfd.c, otherwise linking the linker will fail
   on non-Amiga target configurations. */
/* This one is used by the linker and tells us, if a debug hunk should be
   written out*/
extern int write_debug_hunk;
/* This is also used by the linker to set the attribute of sections */
extern int amiga_attribute;

static boolean
get_long (abfd, n)
    bfd *abfd;
    unsigned long *n;
{
  if (bfd_read ((PTR)n, 1, sizeof (*n), abfd) != sizeof (*n))
    return false;
  *n = GL (n);
  return true;
}

static const struct bfd_target *
amiga_object_p (abfd)
     bfd *abfd;
{
  char buf[8];
  unsigned int x;
  struct stat stat_buffer;

  /* An Amiga object file must be at least 8 bytes long.  */
  if (bfd_read ((PTR) buf, 1, 8, abfd) != 8)
    {
      bfd_set_error(bfd_error_wrong_format);
      return 0;
    }

  bfd_seek (abfd, 0, SEEK_SET);

  /* Does it look like an Amiga object file?  */
  x = GL(buf);
  if ((x != HUNK_UNIT) && (x != HUNK_HEADER) && (x != HUNK_HEADER_POS))
    {
      /* Not an Amiga file.  */
      bfd_set_error(bfd_error_wrong_format);
      return 0;
    }

  /* Can't fail and return (but must be declared boolean to suit
     other bfd requirements).  */
  (void) amiga_mkobject (abfd);

  AMIGA_DATA(abfd)->IsLoadFile = (x == HUNK_HEADER || x == HUNK_HEADER_POS);

  if (!amiga_digest_file (abfd))
    {
      /* Something went wrong.  */
      DPRINT (20,("bfd parser stopped at offset 0x%lx\n", bfd_tell (abfd)));
      return (const struct bfd_target *) 0;
    }

  /* Set default architecture to m68k:68000.  */
  /* So we can link on 68000 AMIGAs..... */
  abfd->arch_info = bfd_scan_arch ("m68k:68000");

  return (abfd->xvec);
}

/* Skip over the hunk length longword + the number of longwords given there.  */
#define next_hunk(abfd) \
  { AMIGA_DATA(abfd)->file_pointer += 1 + GL(AMIGA_DATA(abfd)->file_pointer); }

static asection *
amiga_get_section_by_hunk_number (abfd, hunk_number)
     bfd *abfd;
      long hunk_number;
{
  /* A cache, so we don't have to search the entire list every time.  */
  static asection *last_reference;
  static bfd *last_bfd;
  asection *p;

  switch(hunk_number)
    {
    case -1:
      return bfd_abs_section_ptr;
      break;
    case -2:
      return bfd_und_section_ptr;
      break;
    case -3: 
      return bfd_com_section_ptr;
      break;
    default:
      if (last_reference)
	if (last_bfd == abfd && last_reference->target_index == hunk_number)
	  return last_reference;
      for (p = abfd->sections; p != NULL; p = p->next)
	if (p->target_index == hunk_number)
	  {
	    last_reference = p;
	    last_bfd = abfd;
	    return p;
	  }
      BFD_FAIL();
      break;
    }
  return (asection *) 0;
}

static boolean
amiga_add_reloc (abfd, section, offset, symbol, howto, target_hunk)
     bfd *abfd;
     asection *section;
     bfd_size_type offset;
     amiga_symbol_type * symbol;
     reloc_howto_type *howto;
     long target_hunk;
{
  amiga_reloc_type *reloc;
  static int count;
  asection *target_sec;

  reloc = (amiga_reloc_type *) bfd_zalloc (abfd, sizeof (amiga_reloc_type));
  reloc->next = 0;

  if (!reloc)
    {
      bfd_set_error (bfd_error_no_memory);
      return(false);
    }

  abfd -> flags |= HAS_RELOC;
  section -> flags |= SEC_RELOC;

  if (amiga_per_section(section)->reloc_tail)
    amiga_per_section(section)->reloc_tail->next = reloc;
  else
    section->relocation = (struct reloc_cache_entry *) reloc;
  amiga_per_section(section)->reloc_tail = reloc;
  reloc->next = NULL;
  reloc->relent.address = offset;
  reloc->relent.addend = 0;
  reloc->relent.howto = howto;

  if (symbol==NULL) {		/* relative to section */
    target_sec = amiga_get_section_by_hunk_number (abfd, target_hunk);
     if (target_sec)
       reloc->symbol = (amiga_symbol_type *)target_sec->symbol;
     else
       return false;
  }
  else
    reloc->symbol = symbol;
  reloc->relent.sym_ptr_ptr=(asymbol **)(&(reloc->symbol));
  reloc->target_hunk = target_hunk;

  return true;
}

/* BFD doesn't currently allow multiple sections with the same
   name, so we try a little harder to get a unique name.  */
asection *
amiga_make_unique_section (abfd, name)
     bfd *abfd;
     CONST char *name;
{
  asection *section;

  bfd_set_error (bfd_error_no_error);
  section = bfd_make_section (abfd, name);
  if (!section && (bfd_get_error() == bfd_error_no_error))
    {
#if 0
      int i = 1;
      char *new_name;

      new_name = bfd_alloc (abfd, strlen(name) + 4);

      /* We try to come up with an original name (since BFD
	 currently requires all sections to have different names).  */
      while (!section && (i<=99))
	{
	  sprintf (new_name, "%s_%u", name, i++);
	  section = bfd_make_section (abfd, new_name);
	}
#else
      section = bfd_make_section_anyway (abfd, name);
#endif
    }
  return section;
}



#if DEBUG_AMIGA
#define DPRINTHUNK(x)   fprintf (stderr,"Processing %s hunk (0x%x)...",\
	  (x) == HUNK_UNIT ? "HUNK_UNIT" :\
	  (x) == HUNK_NAME ? "HUNK_NAME" :\
	  (x) == HUNK_DEBUG ? "HUNK_DEBUG" :\
	  (x) == HUNK_OVERLAY ? "HUNK_OVERLAY" :\
	  (x) == HUNK_BREAK ? "HUNK_BREAK" :\
	  (x) == HUNK_HEADER ? "HUNK_HEADER" :\
	  (x) == HUNK_HEADER_POS ? "HUNK_HEADER_POS" :\
	  (x) == HUNK_CODE ? "HUNK_CODE" :\
	  (x) == HUNK_DATA ? "HUNK_DATA" :\
	  (x) == HUNK_BSS ? "HUNK_BSS" :\
	  (x) == HUNK_RELOC8 ? "HUNK_RELOC8" :\
	  (x) == HUNK_RELOC16 ? "HUNK_RELOC16" :\
	  (x) == HUNK_RELOC32 ? "HUNK_RELOC32" :\
	  (x) == HUNK_DREL8 ? "HUNK_DREL8" :\
	  (x) == HUNK_DREL16 ? "HUNK_DREL16" :\
	  (x) == HUNK_DREL32 ? "HUNK_DREL32" :\
	  (x) == HUNK_SYMBOL ? "HUNK_SYMBOL" :\
	  (x) == HUNK_EXT ? "HUNK_EXT" :\
	  (x) == HUNK_END ? "HUNK_END" :\
	  (x) == HUNK_LIB ? "HUNK_LIB" :\
	  (x) == HUNK_INDEX ? "HUNK_INDEX" :\
	  "*unknown*",(x))
#define DPRINTHUNKEND fprintf(stderr,"...done\n")
#else
#define DPRINTHUNK(x) 
#define DPRINTHUNKEND 
#endif

#define STRSZ_BLOCK 4096
#define CARSYM_BLOCK 200

static boolean
parse_archive_units (abfd, n_units, filesize, one, syms, symcount)
  bfd *abfd;
  int *n_units;
  unsigned long filesize;
  boolean one;		       /* parse only the first unit ? */
  struct arch_syms **syms;
  symindex *symcount;
{
  unsigned long hunk_type,pos,no,hunk,len,n;
  long section_idx=-1;
  symindex defsymcount=0;
  unsigned long str_size=0, str_tot_size=0;
  char *strings = NULL;
  unsigned long unit_offset, defsym_pos=0;
  struct arch_syms *nsyms, *syms_tail=NULL;

  *n_units = 0;
  while (get_long (abfd, &hunk_type)) {
    switch (hunk_type) {
    case HUNK_UNIT:
      unit_offset = bfd_tell (abfd)-4;
      (*n_units)++;
      if (one && *n_units>1) {
	bfd_seek (abfd, -4, SEEK_CUR);
	return true;
      }
      if (get_long (abfd, &len) && !bfd_seek (abfd, len<<2, SEEK_CUR)) {
	section_idx = -1;
      }
      else
	return false;
      break;
    case HUNK_DEBUG:
    case HUNK_NAME:
      if (!(get_long (abfd, &len) && !bfd_seek (abfd, len<<2, SEEK_CUR)))
	return false;
      break;
    case HUNK_CODE:
    case HUNK_DATA:
      section_idx++;
      if (!(get_long (abfd, &len) && !bfd_seek (abfd, (len&0x3fffffff)<<2,
						SEEK_CUR)))
	return false;
      break;
    case HUNK_BSS:
      section_idx++;
      if (!get_long (abfd, &len))
	return false;
      break;
    case HUNK_RELOC8:
    case HUNK_RELOC16:
    case HUNK_RELOC32:
      if (!get_long (abfd, &no))
	return false;
      while (no) {
	/* destination hunk */
	if (!get_long (abfd, &hunk))
	  return false;
	/* skip the offsets */
	if (bfd_seek (abfd, no<<2, SEEK_CUR))
	  return false;
	/* read the number of offsets to come */
	if (!get_long (abfd, &no))
	  return false;
      }
      break;
    case HUNK_END:
      break;
    case HUNK_DREL8:
    case HUNK_DREL16:
    case HUNK_DREL32:
      fprintf (stderr, "hunk_drelx not supported yet\n");
      return false;
    case HUNK_SYMBOL:
      if (!get_long (abfd, &len))
	return false;
      while (len) {
	if (bfd_seek (abfd, (len+1)<<2, SEEK_CUR))
	  return false;
	if (!get_long (abfd, &len))
	  return false;
      }
      break;
    case HUNK_EXT:
      defsym_pos = 0;
      if (!get_long (abfd, &n))
	return false;
      while (n != 0) {
	unsigned long tmp, type;

	len = n & 0xffffff;
	type = (n>>24) & 0xff;
	switch (type) {
	case EXT_SYMB:
	case EXT_DEF:
	case EXT_ABS:
	  /* retain the positions of defined symbols for each object
             in the archive. They'll be used later to build a
             pseudo-armap, which _bfd_generic_link_add_archive_symbols
             needs */
	  if (defsym_pos==0)
	    defsym_pos = bfd_tell (abfd)-4;
	  /* skip name & value */
	  if (bfd_seek (abfd, (len+1)<<2, SEEK_CUR))
	    return false;
	  defsymcount++;
	  break;

	case EXT_REF8:
	case EXT_REF16:
	case EXT_REF32:
	case EXT_DEXT8:
	case EXT_DEXT16:
	case EXT_DEXT32:
	  /* skip name */
	  if (bfd_seek (abfd, len<<2, SEEK_CUR))
	    return false;

	  if (!get_long (abfd, &no))
	    return false;
	  if (no)
	    if (bfd_seek (abfd, no<<2, SEEK_CUR))
	      return false;
	  break;

	case EXT_COMMON:
	  /* skip name */
	  if (bfd_seek (abfd, len<<2, SEEK_CUR))
	    return false;
	  /* size of common block */
	  if (!get_long (abfd, &len))
	    return false;
	  /* number of references */
	  if (!get_long (abfd, &no))
	    return false;
	  /* skip references */
	  if (no)
	    if (bfd_seek (abfd, no<<2, SEEK_CUR))
	      return false;
	  break;

	default: /* error */
	  fprintf (stderr, "unexpected type in hunk_ext at offset 0x%lx\n",
		   bfd_tell (abfd));
	  return false;
	}

	if (!get_long (abfd, &n))
	  return false;
      }
      if (defsym_pos != 0 && syms) {
	/* there are some defined symbols, keep enough information on
           them to simulate an armap later on */
	nsyms = (struct arch_syms*) bfd_alloc (abfd, sizeof (struct arch_syms));
	nsyms->next = NULL;
	if (syms_tail)
	  syms_tail->next = nsyms;
	else
	  *syms = nsyms;
	syms_tail = nsyms;
	nsyms->offset = defsym_pos;
	nsyms->size = bfd_tell (abfd) - defsym_pos;
	nsyms->unit_offset = unit_offset;
      }
      break; /* of HUNK_EXT */

    default:
#if 0
      fprintf (stderr, "unexpected hunk 0x%lx at offset 0x%lx\n",
	       hunk_type, bfd_tell (abfd));
#endif
      return false;
    }
  }
  if (syms && symcount)
    *symcount = defsymcount;
  return (bfd_tell (abfd) == filesize);
}

static boolean
amiga_digest_file (abfd)
     bfd *abfd;
{
  amiga_data_type *amiga_data = AMIGA_DATA(abfd);
  unsigned long hunk_type,pos;
  struct stat stat_buffer;

  if (bfd_read ((PTR) &hunk_type, 1, 4, abfd) != 4)
    {
      bfd_set_error (bfd_error_wrong_format);
      return false;
    }

  if (bfd_stat (abfd, &stat_buffer) < 0)
    return false;

  hunk_type = HUNK_VALUE(GL(&hunk_type));

  switch (hunk_type)
    {
    case HUNK_UNIT:
      /* Read the unit(s) */
/*
      while ((pos=bfd_tell (abfd)) < stat_buffer.st_size)
	{
*/
      if (!amiga_read_unit (abfd, stat_buffer.st_size - abfd->origin))
	return false;
      if (abfd->arelt_data)
	arelt_size (abfd) = bfd_tell (abfd);
/*	}*/
      break;

    case HUNK_HEADER:
    case HUNK_HEADER_POS:
      /* This is a load file */
      if (!amiga_read_load (abfd))
	return(false);
      break;
    }

  return true;
}/* of amiga_digest_file */


/* Read in Unit file */
/* file pointer is located after the HUNK_UNIT LW */
static boolean
amiga_read_unit (abfd, size)
    bfd *abfd;
    unsigned long size;
{
  amiga_data_type *amiga_data = AMIGA_DATA(abfd);
  unsigned long hunk_type, hunk_number=0;
  unsigned long sz;
  unsigned long buf[2];

  /* read LW length of unit's name */
  if (bfd_read (buf, sizeof (buf[0]), 1, abfd) != sizeof (buf[0]))
    return false;

  /* and skip it (FIXME maybe) */
  if (bfd_seek (abfd, GL(buf)<<2, SEEK_CUR))
    return false;

  while (bfd_tell (abfd) < size)
    {
      if (!get_long (abfd, &hunk_type))
	return false;

      /* Now there may be CODE, DATA, BSS, SYMBOL, DEBUG, RELOC Hunks */
      hunk_type = HUNK_VALUE (hunk_type);

      switch(hunk_type)
	{
	case HUNK_UNIT:
	  /* next unit, seek back and return */
	  return (bfd_seek (abfd, -4, SEEK_CUR) == 0);

	case HUNK_DEBUG:
	  /* we don't parse hunk_debug at the moment */
	  if (!get_long (abfd, &sz) || bfd_seek (abfd, sz<<2, SEEK_CUR))
	    return false;
	  break;

	case HUNK_NAME:
	case HUNK_CODE:
	case HUNK_DATA:
	case HUNK_BSS:
	  /* Handle this hunk, including relocs, etc.
	     The finishing HUNK_END is consumed by the routine
	     */
	  if (!amiga_handle_cdb_hunk (abfd, hunk_type, hunk_number++,
				      0, -1))
	    return false;

	  break;

	default:
	  /* Something very nasty happened:
	     Illegal Hunk occured....
	     */
	  bfd_set_error (bfd_error_wrong_format);
	  return false;
	  break;
	}/* Of switch hunk_type */

      /* Next hunk */
    }
  return true;
}


/* Read a load file */
static boolean
amiga_read_load (abfd)
    bfd *abfd;
{
  amiga_data_type *amiga_data = AMIGA_DATA(abfd); 
  unsigned long *hunk_attributes, *hunk_sizes;
  int hunk_number=0;
  int hunk_type;
  unsigned long max_hunk_number;
  int i,n;
  unsigned long buf[4];

  /* Read hunk lengths (and memory attributes...) */
  /* Read in each hunk */

  if (bfd_read (buf, sizeof(*buf), 4, abfd) != 4 * sizeof (*buf))
    return false;

  /* If there are resident libs: abort (obsolete feature) */
  if (GL(buf) != 0)
    return false;

  max_hunk_number = GL(buf+1);

  /* Sanity */
  if (max_hunk_number<1)
    {
      bfd_set_error (bfd_error_wrong_format);
      return false;
    }

  amiga_data->nb_hunks = max_hunk_number;

  /* Num of root hunk must be 0 */
  if (GL(buf+2)!=0)
    {
      bfd_set_error (bfd_error_wrong_format);
      return false;
    }

  /* Num of last hunk must be mhn-1 */
  if (GL(buf+3) != max_hunk_number-1)
    {
      fprintf (stderr, "Overlay loadfiles are not supported\n");
      bfd_set_error (bfd_error_wrong_format);
      return false;
    }

  hunk_sizes = alloca (max_hunk_number * sizeof (unsigned long));
  hunk_attributes = alloca (max_hunk_number * sizeof (unsigned long));

  if (hunk_sizes == NULL || hunk_attributes == NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }

  if (bfd_read (hunk_sizes, sizeof (unsigned long), max_hunk_number, abfd) !=
      max_hunk_number * sizeof (unsigned long))
    return false;

  /* Now, read in sizes and memory attributes */
  for (i=0; i<max_hunk_number; i++)
    {
      /* convert to host format */
      hunk_sizes[i] = GL(hunk_sizes+i);
      switch (HUNK_ATTRIBUTE (hunk_sizes[i]))
	{
	case HUNK_ATTR_CHIP:
	  hunk_attributes[i] = MEMF_CHIP;
	  break;
	case HUNK_ATTR_FAST:
	  hunk_attributes[i] = MEMF_FAST;
	  break;
	case HUNK_ATTR_FOLLOWS:
	  if (!get_long (abfd, &hunk_attributes[i]))
	    return false;
	  break;
	default:
	  hunk_attributes[i] = 0;
	  break;
	}
      hunk_sizes[i] = (HUNK_VALUE (hunk_sizes[i]))<<2;
    }

  for (hunk_number=0; hunk_number < max_hunk_number; hunk_number++)
    {
      if (bfd_read (buf, sizeof(*buf), 1, abfd) != sizeof(*buf))
	return false;
      hunk_type = HUNK_VALUE (GL(buf));

      /* This may be HUNK_NAME, CODE, BSS, DEBUG, DATA */
      switch(hunk_type)
	{
	case HUNK_NAME:
	case HUNK_CODE:
	case HUNK_DATA:
	case HUNK_BSS:
	case HUNK_DEBUG:
	  if (!amiga_handle_cdb_hunk (abfd, hunk_type, hunk_number,
				      hunk_attributes[hunk_number],
				      hunk_sizes[hunk_number]))
	    {
	      bfd_set_error (bfd_error_wrong_format);
	      return false;
	    }
	  break;

	default:
	  /* illegal hunk */
	  bfd_set_error (bfd_error_wrong_format);
	  return false;
	  break;
	}/* Of switch */
    }

  return true;
}/* Of amiga_read_load */


/* Handle NAME, CODE, DATA, BSS, DEBUG Hunks */
static boolean 
amiga_handle_cdb_hunk (abfd, hunk_type, hunk_number, hunk_attribute,
		       hunk_size)
     bfd *abfd;
     unsigned long hunk_type;
     unsigned long hunk_number;
     int hunk_attribute;
     unsigned long hunk_size;
/* If hunk_size==-1, then we are digesting a HUNK_UNIT */
{
  amiga_data_type *amiga_data = AMIGA_DATA(abfd);
  char *current_name=NULL;
  unsigned long len;
  asection *current_section=NULL;
  int is_load = (hunk_size!=-1);
  unsigned long buf[6];
  int secflags;

  if (hunk_type==HUNK_NAME) /* get name */
    {
      if (!get_long (abfd, &len))
	return false;
      len = (HUNK_VALUE (len)) << 2;
      current_name = bfd_alloc (abfd, len+1);
      if (!current_name)
	return false;

      if (bfd_read (current_name, 1, len, abfd) != len)
	return false;

      current_name [len] = '\0';

      if (!get_long (abfd, &hunk_type))
	return false;

    }
  else /* Set curent name to something appropriate */
    current_name=(hunk_type==HUNK_CODE)?".text":
                 (hunk_type==HUNK_BSS)?".bss":".data";

  /* file_pointer is now after hunk_type */
  secflags = 0;
  switch (hunk_type)
    {
    case HUNK_CODE:
      secflags = SEC_ALLOC | SEC_LOAD | SEC_CODE | SEC_HAS_CONTENTS;
      goto do_section;

    case HUNK_DATA:
      secflags = SEC_ALLOC | SEC_LOAD | SEC_DATA | SEC_HAS_CONTENTS;
      goto do_section;

    case HUNK_BSS:
      secflags = SEC_ALLOC;

    do_section:
      if (!get_long (abfd, &len))
	return false;
      len = HUNK_VALUE (len)<<2; /* Length of section */
      if (!is_load)
	{
	  hunk_attribute = HUNK_ATTRIBUTE (len);
	  hunk_attribute=(hunk_attribute==HUNK_ATTR_CHIP)?MEMF_CHIP:
	             (hunk_attribute==HUNK_ATTR_FAST)?MEMF_FAST:0;
	}

      /* Make new section */
      current_section = amiga_make_unique_section (abfd, current_name);
      if (!current_section)
	return false;

      current_section->filepos = bfd_tell (abfd);
      /* For a loadfile, the section size in memory comes from the
         hunk header. The size on disk may be smaller. */
      current_section->_cooked_size = current_section->_raw_size =
	((hunk_size==-1) ? len : hunk_size);
      current_section->target_index = hunk_number;
      bfd_set_section_flags (abfd, current_section, secflags);

      amiga_per_section(current_section)->disk_size = len; /* size on disk */
      amiga_per_section(current_section)->attribute = hunk_attribute;

      /* skip the contents */
      if ((secflags & SEC_HAS_CONTENTS) && bfd_seek (abfd, len, SEEK_CUR))
	return false;

      if (!amiga_handle_rest (abfd, current_section, is_load))
	return false;
      break;

      /* Currently, there is one debug hunk per executable, instead of
         one per unit as it would with a "standard" amigaos
         implementation. So the debug hunk is at the same level as
         code/data/bss. This will change in the future */
    case HUNK_DEBUG:
      /* format of gnu debug hunk is:
          HUNK_DEBUG
              N
            0413.L    Magic number
	  symtabsize
	  strtabsize
	  symtabdata  [length=symtabsize]
	  strtabdata  [length=strtabsize]
          [pad bytes]
	  */

      /* read LW length */
      if (bfd_read (buf, sizeof(*buf), 1, abfd) != sizeof (*buf))
	return false;

      len = GL (buf) << 2;
      if (len > 5*sizeof(long))
	{
	  if (bfd_read (buf, sizeof(*buf), 6, abfd) != 6*sizeof(*buf))
	    return false;

	  if (GL(buf) == 0413) /* GNU DEBUG HUNK */
	    {
	      /*FIXME: we should add the symbols in the debug hunk to symtab... */
	      amiga_data->symtab_size = GL(buf+1);
	      amiga_data->stringtab_size = GL(buf+2);
	      adata(abfd).sym_filepos = bfd_tell (abfd);
	      adata(abfd).str_filepos = adata(abfd).sym_filepos +
		amiga_data->symtab_size;
	      len -= 5*sizeof(long);
	    }
	}
      if (bfd_seek (abfd, len, SEEK_CUR))
	return false;
      break;

    default:
      bfd_set_error (bfd_error_wrong_format);
      return false;
      break;
    } /* switch (hunk_type) */

  return true;

} /* Of amiga_handle_cdb_hunk */


/* Handle rest of a hunk 
   I.e.: Relocs, EXT, SYMBOLS... */
static boolean
amiga_handle_rest (abfd, current_section, isload)
     bfd *abfd;
     asection *current_section;
     boolean isload;
{
  amiga_data_type *amiga_data = AMIGA_DATA(abfd);
  unsigned long hunk_type, type, len, n;
  long tmp;
  unsigned long **p, no, *countptr;
  struct amiga_raw_symbol *sp=NULL;
  unsigned long buf[5];
  file_ptr filepos;
  amiga_per_section_type *asect = amiga_per_section (current_section);

  while (1)
    {
      if (!get_long (abfd, &hunk_type))
	return false;
      switch (hunk_type)
	{
	case HUNK_END:
	  return true;
	  break;

	case HUNK_RELOC8:
	  asect->raw_relocs8 = bfd_tell (abfd)-4;
	  countptr = &asect->num_raw_relocs8;
	  goto rel;
	case HUNK_RELOC16:
	  asect->raw_relocs16 = bfd_tell (abfd)-4;
	  countptr = &asect->num_raw_relocs16;
	  goto rel;
	case HUNK_RELOC32:
	  asect->raw_relocs32 = bfd_tell (abfd)-4;
	  countptr = &asect->num_raw_relocs32;
	  goto rel;
	case HUNK_DREL8:
	  fprintf (stderr, "hunk_drel8 not supported yet\n");
	  return false;
	case HUNK_DREL16:
	  fprintf (stderr, "hunk_drel16 not supported yet\n");
	  return false;
	case HUNK_DREL32:
	  fprintf (stderr, "hunk_drel32 not supported yet\n");
	  return false;
	rel:
	  current_section->flags |= SEC_RELOC;
	  abfd->flags |= HAS_RELOC;

	  /* read the first number of relocs */
	  if (!get_long (abfd, &no))
	    return false;

	  /* count and skip them */
	  while (no != 0)
	    {
	      current_section->reloc_count += no;
	      (*countptr) += no;
	      if (bfd_seek (abfd, (no+1)<<2, SEEK_CUR))
		return false;
	      if (!get_long (abfd, &no))
		return false;
	    }
	  break;

	case HUNK_SYMBOL:
	  /* In a unit, we ignore these, since all symbol information
	     comes with HUNK_EXT, in a load file, these are added */
	  if (!isload)
	    {
	      amiga_per_section(current_section)->hunk_symbol_pos =
		bfd_tell (abfd);
	      /* size of first symbol */
	      if (bfd_read (buf, sizeof(*buf), 1, abfd) != 1*sizeof (*buf))
		return false;
	      while (GL(buf) != 0) /* until size is 0 */
		{
		  /* skip the name */
		  if (bfd_seek (abfd, (GL(buf)+1)<<2, SEEK_CUR))
		    return false;
		  /* read the size */
		  if (bfd_read (buf, sizeof(*buf), 1, abfd) != 1*sizeof (*buf))
		    return false;
		}
	      break;
	    }
	  /* We add these, by falling through... */

	case HUNK_EXT:
	  /* We leave these alone, until they are requested by the user */
	  amiga_per_section(current_section)->hunk_ext_pos = bfd_tell (abfd);

	  if (!get_long (abfd, &n))
	    return false;
	  while (n != 0)
	    {
	      /* read the symbol type and length */
	      type = (n>>24) & 0xff;
	      len = n & 0xffffff;

	      /* skip the symbol name */
	      if (bfd_seek (abfd, len<<2, SEEK_CUR))
		return false;

	      switch(type)
		{
		case EXT_SYMB: /* Symbol hunks are relative to hunk start... */
		case EXT_DEF: /* def relative to hunk */
		case EXT_ABS: /* def absolute */
		  abfd->flags |= HAS_SYMS; /* We have symbols */
		  abfd->symcount++;
		  /* skip the value */
		  if (!get_long (abfd, &tmp))
		    return false;
		  break;

		case EXT_REF8: /* 8 bit ref */
		case EXT_REF16: /* 16 bit ref */
		case EXT_REF32: /* 32 bit ref */
		case EXT_DEXT8: /* 8 bit base relative ref */
		case EXT_DEXT16: /* 16 bit " "*/
		case EXT_DEXT32: /* 32 bit " " */
		  abfd->flags |= HAS_SYMS;
		  abfd->symcount++;
		  if (!get_long (abfd, &tmp))
		    return false;
		  if (tmp)
		    {
		      abfd->flags|=HAS_RELOC;
		      current_section->flags|=SEC_RELOC;
		      current_section->reloc_count += tmp;
		      /* skip references */
		      if (bfd_seek (abfd, tmp<<2, SEEK_CUR))
			return false;
		    }
		  break;

		case EXT_COMMON: /* Common ref/def */
		  abfd->flags |= HAS_SYMS;
		  abfd->symcount++;
		  /* skip the size of common block: FIXME */
		  if (!get_long (abfd, &tmp))
		    return false;
		  if (!get_long (abfd, &tmp))
		    return false;
		  if (tmp)
		    {
		      abfd->flags |= HAS_RELOC;
		      current_section->flags |= SEC_RELOC;
		      current_section->reloc_count += tmp;
		      if (bfd_seek (abfd, tmp<<2, SEEK_CUR))
			return false;
		    }
		  break;

		default: /* error */
		  bfd_set_error (bfd_error_wrong_format);
		  return false;
		  break;
		}/* of switch type */

	      if (!get_long (abfd, &n))
		return false;
	    }
	  break;

	  /* If a debug hunk is found at this position, the file has
             been generated by a third party tool and the debug info
             here are useless to us. Just skip the hunk, then. */
	case HUNK_DEBUG:
	  if (!get_long (abfd, &n) || bfd_seek (abfd, n<<2, SEEK_CUR))
	    return false;
	  break;
	default: /* error */
	  bfd_set_error (bfd_error_wrong_format);
	  return false;
	  break;
	}/* Of switch */
    }/* of while */
  return true;
}/* of amiga_handle_rest*/

static boolean
amiga_mkobject (abfd)
     bfd *abfd;
{
  struct amiga_data_struct *rawptr;

  rawptr = (struct amiga_data_struct *)
    bfd_zalloc (abfd, sizeof (struct amiga_data_struct));
  abfd->tdata.amiga_data = rawptr;
  return rawptr != NULL;
}

static boolean
amiga_mkarchive (abfd)
     bfd *abfd;
{
  amiga_ardata_type *ar;
  ar = (amiga_ardata_type*) bfd_zalloc (abfd, sizeof (amiga_ardata_type));
  amiga_ardata (abfd) = ar;
  return (ar!=NULL);
}

/* used with base relative linking */
extern int amiga_base_relative;

/* used with -resident linking */
extern int amiga_resident;

/* write nb long words (possibly swapped out) to the output file */
static boolean
write_longs (in, nb, abfd)
     unsigned long *in;
     long nb;
     bfd *abfd;
{
  unsigned long out [10];
  int i;
  
  while (nb > 0)
    {
      for (i=0; i<nb && i<10; i++)
	out[i] = GL (in++);
      if (bfd_write ((PTR)out, sizeof (long), i, abfd) != sizeof(long)*i)
	return false;
      nb -= 10;
    }
  return true;
}

static long
determine_datadata_relocs (abfd, section)
     bfd *abfd;
     asection *section;
{
  long relocs = 1, i;
  struct reloc_cache_entry *r;
  asection *insection;
  asymbol *sym_p;
  
  for (i=0;i<section->reloc_count;i++)
    {
      r=section->orelocation[i];
      if (r == NULL)
        continue;
      sym_p=*(r->sym_ptr_ptr); /* The symbol for this section */
      insection=sym_p->section;

      /* Is reloc relative to a special section ? */
      if ((insection==bfd_abs_section_ptr)||(insection==bfd_com_section_ptr)||
	  (insection==bfd_und_section_ptr)||(insection==bfd_ind_section_ptr))
	continue; /* Nothing to do, since this translates to HUNK_EXT */
      if (insection->output_section == section)
        relocs++;
    }
  return relocs;
}

/* Adjust the indices map when we decide not to output the section <sec> */
static void
remove_section_index (sec, index_map)
    asection *sec;
    long *index_map;
{
  int i = sec->index;
  sec = sec->next;
  index_map[i++] = -1;
  while (sec!=NULL) {
    (index_map[i++])--;
    sec=sec->next;
  }
}

/* Write out the contents of a bfd */
static boolean
amiga_write_object_contents (abfd)
     bfd *abfd;
{
  struct amiga_data_struct *amiga_data=AMIGA_DATA(abfd);
  sec_ptr p;
  unsigned long n[5];
  long i;
  static const char zero[3]={0,0,0};
  long datadata_relocs, bss_size = 0;
  long *index_map;
  asection *data_sec;

  /* Distinguish UNITS, LOAD Files
    Write out hunks+relocs+HUNK_EXT+HUNK_DEBUG (GNU format)*/
  DPRINT(5,("Entering write_object_conts\n"));

  abfd->output_has_begun=true; /* Output has begun */

  index_map = bfd_alloc (abfd, abfd->section_count * sizeof (long));
  if (!index_map)
    return false;

  for (i=0, p=abfd->sections; p!=NULL; p=p->next)
    index_map[i++] = p->index;

  /* Distinguish Load files and Unit files */
  if (amiga_data->IsLoadFile)
    {
      DPRINT(5,("Writing Load file\n"));

      /* Write out load file header */
      if (amiga_pOS_flg)
	n[0] = HUNK_HEADER_POS;
      else
	n[0] = HUNK_HEADER;
      n[1] = n[2] = 0;
      for (p=abfd->sections; p!=NULL; p=p->next) {
	/* For baserel linking, don't remove the empty sections, since
           they may get some contents later on */
	if ((amiga_base_relative || p->_raw_size!=0 || p->_cooked_size!=0) &&
	    !(amiga_base_relative && !strcmp (p->name, ".bss")))
	  n[2]++;
	else
	  remove_section_index (p, index_map);
      }

      if (amiga_base_relative)
	BFD_ASSERT(abfd->section_count==3);

      n[3]=0;
      n[4]=n[2]-1;
      if (!write_longs (n, 5, abfd))
	return false;

      /* Write out sizes and memory specifiers... */
      /* We have to traverse the section list again, bad but no other way... */
      if (amiga_base_relative) {
        for (p=abfd->sections; p!=NULL; p=p->next)
	  {
	    if (amiga_resident && strcmp(p->name,".data")==0)
	      {
	        datadata_relocs = determine_datadata_relocs (abfd, p);
	        data_sec = p;
	      }
	    else if (strcmp(p->name,".bss")==0)
	      {
	        /* Get size for header*/
	          bss_size = p->_raw_size;
	      }
	  }
      }

      for (p=abfd->sections; p!=NULL; p=p->next)
	{
          long extra = 0;

	  if (index_map[p->index] < 0)
	    continue;
	  if (amiga_resident && (strcmp(p->name,".text")==0))
	    extra = datadata_relocs*4;
	  else {
	    if (amiga_base_relative && !strcmp (p->name, ".data"))
	      extra = bss_size;
	  }

	  if (amiga_per_section(p)->disk_size == 0)
	    amiga_per_section(p)->disk_size = p->_raw_size;

	  /* convert to a size in long words */
	  n[0] = LONGSIZE (p->_raw_size+extra);

	  i=amiga_per_section(p)->attribute;
	  switch (i)
	    {
	    case MEMF_CHIP:
	      n[0]|=0x40000000;
	      i=1;
	      break;
	    case MEMF_FAST:
	      n[0]|=0x80000000;
	      i=1;
	      break;
	    case 0: /* nothing*/
	      i=1;
	      break;
	    default: /* Special one */
	      n[0]|=0xc0000000;
	      n[1]=i;
	      i=2;
	      break;
	    }/* Of switch */

	  if (!write_longs (n, i, abfd))
            return false;
	}/* Of for */
    }
  else
    {/* Unit , no base-relative linking here.... */
      int len = strlen (abfd->filename);
      /* Write out unit header */
      DPRINT(5,("Writing Unit\n"));

      n[0]=HUNK_UNIT;
      if (!write_longs (n, 1, abfd))
	return false;

      i = LONGSIZE (len);
      if (!write_name (abfd, abfd->filename, 0))
	return false;
    }

  /* Write out every section */
  for (p=abfd->sections; p!=NULL; p=p->next)
    {
      if (index_map[p->index] < 0)
	continue;
      if (amiga_per_section(p)->disk_size == 0)
	amiga_per_section(p)->disk_size = p->_raw_size;

      if (amiga_resident && (strcmp(p->name,".text")==0))
        {
          if (!amiga_write_section_contents (abfd,p,data_sec,datadata_relocs,
					    index_map))
	    return false;
        }
      else if (amiga_base_relative && (strcmp(p->name,".data")==0))
        {
          if (!amiga_write_section_contents (abfd,p,0,0,index_map))
	    return false;
        }
      else
        { 
          if (!amiga_write_section_contents (abfd,p,0,0,index_map))
	    return false;
        }

      if (!amiga_write_symbols(abfd,p)) /* Write out symbols, incl HUNK_END */
	return false;

    }/* of for sections */

  /* Write out debug hunk, if requested */
  if (amiga_data->IsLoadFile /*&& write_debug_hunk*/)
    {
      extern boolean
	translate_to_native_sym_flags (bfd*, asymbol*, struct external_nlist*);

      /* We have to convert all the symbols in abfd to a.out style.... */
      struct external_nlist data;
      int str_size, offset = 4;
      int symbols = 0;
      asymbol *sym;
      asection *s;

      if (abfd->symcount)
	{
	  /* Now, set the .text, .data and .bss fields in the tdata
	     struct (because translate_to_native_sym_flags needs
	     them... */
	  for (i=0,s=abfd->sections;s!=NULL;s=s->next)
	    if (strcmp(s->name,".text")==0)
	      {
		i|=1;
		adata(abfd).textsec=s;
	      }
	    else if (strcmp(s->name,".data")==0)
	      {
	        i|=2;
	        adata(abfd).datasec=s;
	      }
	    else if (strcmp(s->name,".bss")==0)
	      {
	        i|=4;
	        adata(abfd).bsssec=s;
	      }

	  if (i!=7) /* One section missing... */
	    {
	      fprintf(stderr,"Missing section, hunk not written\n");
	      return true;
	    }

	  str_size=4; /* the first 4 bytes will be replaced with the length */

#define CAN_WRITE_OUTSYM(sym) (sym!=NULL && sym->section && \
				((sym->section->owner && \
				 bfd_get_flavour (sym->section->owner) == \
				 bfd_target_aout_flavour) || \
				 bfd_asymbol_flavour(sym) == \
				 bfd_target_aout_flavour))

	  for (i = 0; i < abfd->symcount; i++) /* Translate every symbol */
	    {
	      sym = abfd->outsymbols[i];
	      /* NULL entries have been written already.... */
	      if (CAN_WRITE_OUTSYM (sym))
	        {
		  str_size += strlen(sym->name) + 1;
		  symbols++;
		}
	    }

	  /* Write out HUNK_DEBUG, size, 0413.... */
	  n[0] = HUNK_DEBUG;
	  n[1] = 3 + ((symbols * sizeof(struct internal_nlist) + str_size + 3) >> 2);
	  n[2] = 0413L; /* Magic number */
	  n[3] = symbols * sizeof(struct internal_nlist);
	  n[4] = str_size;
	  if (!write_longs ((PTR)(n), 5, abfd))
	    return false;

	  /* Write out symbols */
	  for (i = 0; i < abfd->symcount; i++) /* Translate every symbol */
	    {
	      sym = abfd->outsymbols[i];
	      /* NULL entries have been written already.... */
	      if (CAN_WRITE_OUTSYM (sym))
		{
	            {
                      aout_symbol_type *t = (aout_symbol_type *)
			&(sym)->the_bfd;

	              bfd_h_put_16(abfd, t->desc, data.e_desc);
	              bfd_h_put_8(abfd, t->other, data.e_other);
	              bfd_h_put_8(abfd, t->type, data.e_type);
		    }
		    if (!translate_to_native_sym_flags(abfd,sym,&data))
		      {
			fprintf (stderr, "Cannot translate flags for %s\n",
				 sym->name);
		      }
		  PUT_WORD(abfd, offset, &(data.e_strx[0])); /* Store index */
		  offset += strlen(sym->name) + 1;
	          if (bfd_write ((unsigned long *)&data,sizeof(long),3,abfd)
		      != sizeof(long)*3)
	            return false;
	      }
	    }

	  /* Write out strings */
	  if (!write_longs ((unsigned long *)&str_size, 1, abfd))
	    return false;
	  
	  for (i = 0; i < abfd->symcount; i++) /* Translate every symbol */
	    {
	      sym = abfd->outsymbols[i];
	      /* NULL entries have been written already.... */
	      if (CAN_WRITE_OUTSYM (sym))
		{
		  int len = strlen(sym->name) + 1;

	          /* Write string tab */
	          if (bfd_write((PTR)(sym->name),sizeof(char),len,abfd)!=len)
	            return false;
		}
	    }

          i = ((str_size + 3) & (~3)) - str_size;
          str_size = 0;
	  /* Write padding */
	  if (i && bfd_write((PTR)(&str_size),sizeof(char),i,abfd)!=i)
	    return false;

	  /* write a HUNK_END here to finish the loadfile, or amigaos
	     will refuse to load it */
	  n[0]=HUNK_END;
	  if (!write_longs (n, 1, abfd))
	    return false;
	}/* Of if abfd->symcount */
    }/* Of write out debug hunk */

  bfd_release (abfd, index_map);
  return true;
}

/* Write a string padded to 4 bytes and preceded by it's length in
   long words ORed with <value> */
static boolean
write_name (abfd, name, value)
     bfd *abfd;
     const char *name;
     long value;
{
  long i,j;
  struct name {
    long len;
    char buf[MAX_NAME_SIZE+3];
  } n;

  j = strlen (name);
  if (j > MAX_NAME_SIZE)
    j = MAX_NAME_SIZE;
  strncpy (n.buf, name, j);
  i = LONGSIZE (j) | value;
  n.len = GL (&i);
  if (j&3) {
    n.buf[j] = n.buf[j+1] = n.buf[j+2] = '\0';
    j += (4-(j&3))&3;
  }
  return (bfd_write ((PTR)&n, 1, sizeof(long)+j, abfd) == sizeof(long)+j);
}

static boolean
amiga_write_archive_contents (arch)
     bfd *arch;
{
  bfd *object;
  char buffer[DEFAULT_BUFFERSIZE];
  int i;
  unsigned long n[2];
  long size;
  struct stat status;

  for (object = arch->archive_head; object; object = object->next)
    {
      unsigned int remaining;

      if (bfd_write_p (object))
	{
	  bfd_set_error (bfd_error_invalid_operation);
	  return false;
	}

      if (stat (object->filename, &status) != 0)
	{
	  bfd_set_error (bfd_error_system_call);
	  return false;
	}

      if (bfd_seek (object, (file_ptr) 0, SEEK_SET) != 0)
	return false;

      remaining = status.st_size;

      while (remaining)
	{
	  unsigned int amt = DEFAULT_BUFFERSIZE;
	  if (amt > remaining)
	    amt = remaining;
	  errno = 0;
	  if (bfd_read (buffer, amt, 1, object) != amt)
	    {
	      if (bfd_get_error () != bfd_error_system_call)
		bfd_set_error (bfd_error_malformed_archive);
	      return false;
	    }
	  if (bfd_write (buffer, amt, 1, arch) != amt)
	    return false;
	  remaining -= amt;
	}
    }
    return true;
}

static boolean
amiga_write_armap (abfd)
     bfd *abfd;
{
  return true;
}

#define determine_size(type) (2 - ((type)>=3 ? (type)-3 : (type)))

static int
determine_type (r)
     struct reloc_cache_entry *r;
{
  switch (r->howto->type) /* FIXME: Is this sufficient to distinguish them ?*/
    {
      /* AMIGA specific */
      case HUNK_RELOC8:
      case HUNK_RELOC16:
      case HUNK_RELOC32:
      case HUNK_DREL8:
      case HUNK_DREL16:
      case HUNK_DREL32:
        if (r->howto->type >= HUNK_DREL32)
          return 3 + r->howto->type - HUNK_DREL32;
        return r->howto->type - HUNK_RELOC32;

      /* Now, these may occur, if a.out was used as input */
      case 0: /* 8 bit ref */
        return 2;

      case 1: /* 16 bit relative */
        return 1;

      case 2: /* 32 bit relative */
        return 0;

      case 9: /* 16 bit base rel */
        return 4;

      case 10: /* 32 bit baserel */
        return 3;

      /* FIXME: There are other (pc relative) displacements left */
      default: /* Error, can't represent this */
        bfd_set_error(bfd_error_nonrepresentable_section);
        return -1;
    }/* Of switch */
}

#define MAX_RELOC_OUT 3

static unsigned long reloc_types[]= {
  HUNK_RELOC32, HUNK_RELOC16, HUNK_RELOC8
/* these reloc types are not supported at the moment */
#if 0
  HUNK_DREL32,  HUNK_DREL16,  HUNK_DREL8
#endif
};

#define NB_RELOC_TYPES (sizeof(reloc_types) / sizeof(reloc_types[0]))

/* Write out section contents, including relocs */
static boolean
amiga_write_section_contents (abfd, section, data_sec, datadata_relocs,
			      index_map)
     bfd *abfd;
     asection *section;
     asection *data_sec;
     long datadata_relocs;
     long *index_map;
{
  static const char zero[3]={0,0,0};
  unsigned long n[2];
  int i, j, type, size;
  unsigned int k;
  struct reloc_cache_entry *r;
  asection *osection, *sec, *insection;
  asymbol *sym_p;
  int pad, reloc_count = 0;
  unsigned long disksize;
  int max_hunk = -1;
  char *c_p;
  unsigned char *values;
  long *reloc_counts;

  DPRINT(5, ("Entered Write-section-conts\n"));

  /* Compute the maximum hunk number of the ouput file */
  sec = abfd->sections;
  while (sec) {
    max_hunk++;
    sec = sec->next;
  }

  /* If we are base-relative linking and the section is .bss and abfd
     is a load file, then return */
  if (AMIGA_DATA(abfd)->IsLoadFile)
    {
      if (amiga_base_relative && (strcmp(section->name, ".bss") == 0))
	return true; /* Nothing to do */
    }
  else
    {
      /* WRITE out HUNK_NAME + section name */
      n[0] = HUNK_NAME;
      if (!write_longs (n, 1, abfd) || !write_name (abfd, section->name, 0))
	return false;
    }

  /* Depending on the type of the section, write out HUNK_{CODE|DATA|BSS} */
  if (section->flags & SEC_CODE) /* Code section */
    n[0] = HUNK_CODE;
  else if (section->flags & (SEC_DATA | SEC_LOAD)) /* data section */
    n[0] = HUNK_DATA;
  else if (section->flags & SEC_ALLOC) /* BSS */
    n[0] = HUNK_BSS;
  else if (section->flags & SEC_DEBUGGING) /* debug section */
    n[0] = HUNK_DEBUG;
  else /* Error */
    {
#if 0
      bfd_set_error(bfd_error_nonrepresentable_section);
      return(false);
#else
      /* FIXME: Just dump everything we don't currently recognize into
	 a DEBUG hunk. */
      n[0] = HUNK_DEBUG;
#endif
    }

  DPRINT(10,("Section type is %lx\n",n[0]));

  /* Get real size in n[1], this may be shorter than the size in the header */
  disksize = LONGSIZE (amiga_per_section(section)->disk_size) + datadata_relocs;
  pad = (4-(amiga_per_section(section)->disk_size & 3)) & 3;
  n[1] = disksize;

  /* in a load file, we put section attributes only in the header */
  if (!(AMIGA_DATA(abfd)->IsLoadFile))
    {
      /* Get attribute for section */
      switch (amiga_per_section(section)->attribute)
	{
	case MEMF_CHIP:
	  n[1] |= HUNKF_CHIP;
	  break;
	case MEMF_FAST:
	  n[1] |= HUNKF_FAST;
	  break;
	case 0:
	  break;
	default: /* error , can't represent this */
	  bfd_set_error(bfd_error_nonrepresentable_section);
	  return(false);
	  break;
	}
    }/* Of switch */

  if (!write_longs (n, 2, abfd))
      return false;

  DPRINT(5,("Wrote code and size=%lx\n",n[1]));

  /* If a BSS hunk, we're done, else write out section contents */
  if (HUNK_VALUE(n[0]) == HUNK_BSS)
    return true;

  DPRINT(5,("Non bss hunk...\n"));

  /* Traverse through the relocs, sample them in reloc_data, adjust section
     data to get 0 addend
     Then compactify reloc_data
     Set the entry in the section for the reloc to NULL */

  if (disksize != 0)
    BFD_ASSERT((section->flags & SEC_IN_MEMORY) != 0);

  reloc_counts  = (long*) bfd_alloc (abfd, NB_RELOC_TYPES * (max_hunk+1)
				     * sizeof (long));
  if (!reloc_counts)
    return false;
  bzero (reloc_counts, NB_RELOC_TYPES*(max_hunk+1)*sizeof (long));

  DPRINT(5,("Section has %d relocs\n", section->reloc_count));

  for (i = 0; i < section->reloc_count; i++)
    {
      r = section->orelocation[i];
      if (r == NULL)
        continue;
      sym_p = *(r->sym_ptr_ptr); /* The symbol for this section */
      insection = sym_p->section;
      DPRINT(5,("Sec for reloc is %lx(%s)\n",insection,insection->name));
      DPRINT(5,("Symbol for this reloc is %lx(%s)\n",sym_p, sym_p->name));

      /* Is reloc relative to a special section ? */
      if ((insection == bfd_abs_section_ptr) ||
	  (insection == bfd_com_section_ptr) ||
	  (insection == bfd_und_section_ptr) ||
	  (insection == bfd_ind_section_ptr))
	continue; /* Nothing to do, since this translates to HUNK_EXT */

      r->addend += sym_p->value; /* Add offset of symbol from section start */

      /* Address of reloc has been unchanged since original reloc, or has been
	 adjusted by get_relocated_section_contents. */
      /* For relocs, the vma of the target section is in the data, the
	 addend is -vma of that section =>No need to add vma*/
      /* Add in offset */
      r->addend += insection->output_offset;
      osection = insection->output_section; /* target section */

      /* Determine which hunk to write, and index of target */
      j = index_map[osection->index];

      if (j<0 || j>max_hunk) {
	fprintf (stderr, "erroneous relocation to hunk %d\n", j);
	BFD_FAIL ();
      }

      type = determine_type(r);
      if (type == -1)
	return false;
      size = determine_size(type);

      if (type < NB_RELOC_TYPES)
	reloc_counts[type+(j*NB_RELOC_TYPES)]++;
      else {
	bfd_set_error (bfd_error_nonrepresentable_section);
	return false;
      }

      c_p = ((char *)(section->contents)) + r->address;
      DPRINT(5,("reloc address=%lx,addend=%lx\n",r->address,r->addend));

      /* There is no error checking with these.. */
      values = (unsigned char *)c_p;
      switch (size)
	{
	case 0: /* adjust byte */
	  j = (int)(*c_p) + r->addend;
	  *c_p = (signed char)j;
	  break;
	case 1: /* Adjust word */
	  k = values[1] | (values[0] << 8);
	  j = (int)k + r->addend;
	  values[0] = (j & 0xff00) >> 8;
	  values[1] = j & 0xff;
	  break;
	case 2: /* adjust long */
	  k = values[3] | (values[2] << 8) | (values[1] << 16) |
	    (values[0] << 24);
	  j = (int)k + r->addend;
	  values[3] = j & 0xff;
	  values[2] = (j & 0xff00) >> 8;
	  values[1] = (j & 0xff0000) >> 16;
	  values[0] = ((unsigned int)j & 0xff000000) >> 24;
	  break;
	} /* of switch */

      r->addend = 0;
      DPRINT(5,("Did adjusting\n"));

      if (type < MAX_RELOC_OUT)
        reloc_count++;
      else
        section->orelocation[i] = NULL;
    } /* of for i */

  DPRINT(5,("Did all relocs\n"));

  /* We applied all the relocs, as far as possible to obtain 0 addend fields */
  /* Write the section contents */
  if (amiga_per_section(section)->disk_size != 0)
    {
      if (bfd_write((PTR)(section->contents), sizeof(char),
		    amiga_per_section(section)->disk_size, abfd) !=
	  amiga_per_section(section)->disk_size)
	return false;
    }

  /* pad the section on disk if necessary (to a long boundary) */
  if (pad!=0 && (bfd_write (zero, 1, pad, abfd) != pad))
    return false;

#if 0
  /* write bss data in the data hunk if needed */
  i = 0;
  while (bss_size--)
    if (!write_longs((PTR)&i, 1, abfd))
      return false;
#endif

  if (datadata_relocs)
    {
      datadata_relocs--;
      if (!write_longs (&datadata_relocs, 1, abfd))
        return false;
      for (i = 0; i < data_sec->reloc_count; i++)
        {
          r = data_sec->orelocation[i];
          if (r == NULL)
            continue;
          sym_p = *(r->sym_ptr_ptr); /* The symbol for this section */
          insection = sym_p->section;

          /* Is reloc relative to a special section ? */
          if ((insection == bfd_abs_section_ptr) ||
	      (insection == bfd_com_section_ptr) ||
              (insection == bfd_und_section_ptr) ||
	      (insection == bfd_ind_section_ptr))
            continue; /* Nothing to do, since this translates to HUNK_EXT */

	  if (insection->output_section == data_sec)
	    {
	      if (determine_type(r) == 0)
		if (!write_longs ((PTR)&r->address, 1, abfd))
		  return false;
	    }
	}
    }
  DPRINT(10,("Wrote contents, writing relocs now\n"));


  if (reloc_count) {
    while (reloc_count) {
      /* Sample every reloc type */
      for (i = 0; i < NB_RELOC_TYPES; i++) {
	int written = false;
	for (j = 0; j <= max_hunk; j++) {
	  long relocs;
	  while ((relocs = reloc_counts [i+(j*NB_RELOC_TYPES)]) > 0) {
	    if (!written)
	      if (!write_longs(&reloc_types[i], 1, abfd))
		return false;
	      else
		written = true;

	    if (relocs > 0xffff)
	      relocs = 0xffff;

	    reloc_counts [i+(j*NB_RELOC_TYPES)] -= relocs;
	    n[0] = relocs;
	    n[1] = j;
	    if (!write_longs(n, 2, abfd))
	      return false;
	    reloc_count -= relocs;

	    for (k = 0; k < section->reloc_count; k++) {
	      int jj;

	      r = section->orelocation[k];
	      if (r == NULL)	/* already written */
		continue;
	      sym_p = *(r->sym_ptr_ptr); /* The symbol for this section */
	      insection = sym_p->section;
	      /* Is reloc relative to a special section ? */
	      if ((insection == bfd_abs_section_ptr) ||
		  (insection == bfd_com_section_ptr) ||
		  (insection == bfd_und_section_ptr) ||
		  (insection == bfd_ind_section_ptr))
		/* Nothing to do, since this translates to HUNK_EXT */
		continue;

	      osection = insection->output_section; /* target section */

#if 0
	      /* Determine which hunk to write, and index of target */
	      for (jj = 0, sec = abfd->sections; sec != NULL;
		   sec = sec->next, jj++) {
		if (sec == osection)
		  break;
	      }

	      BFD_ASSERT (jj==index_map[insection->output_section->index]);
#else
	      jj=index_map[insection->output_section->index];
#endif
	      if (jj == j && i == determine_type(r)) {
		section->orelocation[k] = NULL;
		if (!write_longs((PTR)&r->address, 1, abfd))
		  return false;
		if (--relocs == 0)
		  break;
	      }
	    }
	  }
	}
      }
    }
    /* write a zero to finish the relocs */
    if (!write_longs((PTR)&reloc_count, 1, abfd))
      return false;
  }
  bfd_release (abfd, reloc_counts);
  DPRINT(5,("Leaving write_section...\n"));
  return true;
}


/* Write out symbol information, including HUNK_EXT, DEFS, ABS. 
   In the case, we were linking base relative, the symbols of the .bss
   hunk have been converted already to belong to the .data hunk */

static boolean
amiga_write_symbols (abfd, section)
     bfd *abfd;
     asection *section;
{

  int i,j;
  struct reloc_cache_entry *r;
  asection *osection;
  asymbol *sym_p;
  char b[3]="\0\0\0";
  unsigned long n[3];
  int symbol_count;
  unsigned long symbol_header;
  unsigned long type, tmp;
  int len;

  /* If base rel linking and section is .bss ==> exit */
  if (amiga_base_relative && (strcmp(section->name,".bss")==0))
    return true;

  if (section->reloc_count==0 && abfd->symcount==0)
    {/* Write HUNK_END */
    alldone:
      DPRINT(5,("Leaving write_symbols\n"));
      n[0]=HUNK_END;
      return write_longs ((PTR)n, 1, abfd);
    }

  symbol_count=0;
  symbol_header=HUNK_EXT;

  /* If this is Loadfile, then do not write HUNK_EXT, but rather HUNK_SYMB*/

 /* Write out all the symbol definitions, then HUNK_END 

     Now, first traverse the relocs, all entries that are non NULL
     have to be taken into account */
  /* Determine the type of HUNK_EXT to issue and build a single
     HUNK_EXT subtype */


  /*FIXME: We write out many HUNK_EXT's entries for references to the
    same symbol.. */
  for (i=0;i<section->reloc_count;i++)
    {
      r=section->orelocation[i];

      if (r==NULL) /* Empty entry */
	continue;

      sym_p=*(r->sym_ptr_ptr); /* The symbol for this section */
      osection=sym_p->section; /* The section the symbol belongs to */
      /* this section MUST be a special section */

      DPRINT(5,("Symbol is %s, section is %lx(%s)\n",sym_p->name,osection,osection->name));

      if (osection!=bfd_com_section_ptr) /* Not common symbol */
	{
	  DPRINT(5,("Non common ref\n"));
	  /* Add a reference to this HUNK */
	  if ((symbol_count++)==0) /* First write out the HUNK_EXT */
	    {
	      tmp=HUNK_EXT;
	      if (!write_longs(&tmp, 1, abfd))
		return false;
	    }

	  /* Determine type of ref */
	  switch(r->howto->type)
	    {
	      /* AMIGA specific */
	    case 0:
	    case HUNK_RELOC8:
	      type=EXT_REF8;
	      break;

	    case 1:
	    case HUNK_RELOC16:
	      type=EXT_REF16;
	      break;

	    case 2:
	    case HUNK_RELOC32:
	      type=EXT_REF32;
	      break;
	    case HUNK_DREL8:
	      type=EXT_DEXT8;
	      break;

	    case 9:
	    case HUNK_DREL16:
	      type=EXT_DEXT16;
	      break;

	    case 10:
	    case HUNK_DREL32:
	        type=EXT_DEXT32;
	      break;
	      
	      /* FIXME: There are other (pc relative) displacements left */
	    default: /* Error, can't represent this */
	      bfd_set_error(bfd_error_nonrepresentable_section);
	      return false;
	      break;
	    }/* Of switch */
	  DPRINT(5,("Type is %x\n",type));

	  if (!write_name (abfd, sym_p->name, type << 24))
	    return false;
	  n[0]=1; /* 1 ref at address... */
	  n[1]=r->address;
	  if (!write_longs (n, 2, abfd))
	    return false;

	  continue; /* Next relocation */
	}/* Of is ref to undefined or abs symbol */

      else /* ref to common symbol */
	{
	  DPRINT(5,("Common ref\n"));

	  /* If the reference is NOT 32 bit wide absolute , then issue warning */
	  if ((r->howto->type!=2)&&(r->howto->type!=HUNK_RELOC32))
	    fprintf(stderr,"Warning: Non 32 bit wide reference to common symbol %s\n",
		    sym_p->name);

	  if ((symbol_count++)==0) /* First write out the HUNK_EXT */
	    {
	      tmp=HUNK_EXT;
	      if (!write_longs (&tmp, 1, abfd))
		return false;
	    }

	  if (!write_name (abfd, sym_p->name, EXT_COMMON<<24))
	    return false;
	  n[0]=sym_p->value; /* Size of common block */
	  n[1]=1;
	  n[2]=r->address;
	  if (!write_longs (n, 3, abfd))
	    return false;

	  continue;
	}/* Of is common section */

      DPRINT(10,("Failing...\n"));
      BFD_FAIL();
    }/* Of traverse relocs */
      
	  
  /* Now traverse the symbol table and write out all definitions, that are relative
     to this hunk */
  /* Absolute defs are always only written out with the first hunk */
  /* Don't write out local symbols
                     undefined symbols
		     indirect symbols
		     warning symbols
		     debugging symbols
		     warning symbols
		     constructor symbols, since they are unrepresentable in HUNK format..*/

  DPRINT(10,("Traversing symbol table\n"));
  symbol_header=(AMIGA_DATA(abfd)->IsLoadFile)?HUNK_SYMBOL:HUNK_EXT;
  for (i=0;i<abfd->symcount;i++)
    {
      sym_p=abfd->outsymbols[i];
      osection=sym_p->section;

      DPRINT(5,("%d. symbol(%s), osec=%x(%s)\n",i,sym_p->name,osection,osection->name));

      if ((osection==bfd_und_section_ptr)/*||(osection==bfd_com_section_ptr)*/||
	  (osection==bfd_ind_section_ptr))
	continue; /* Don't write these */

      /* Only write abs defs, if not writing A Loadfile */
      if ((osection==bfd_abs_section_ptr)&&(section->index==0)&&
	  !AMIGA_DATA(abfd)->IsLoadFile) /* Write out abs defs */
	{
	  DPRINT(5,("Abs symbol\n"));
	  /* don't write debug symbols, they will be written in a
             HUNK_DEBUG later on */
	  if (sym_p->flags & BSF_DEBUGGING)
	    continue;

	  if ((symbol_count++)==0) /* First write out the HUNK_EXT */
	    {
	      if (!write_longs (&symbol_header, 1, abfd))
		return false;
	    }

	  if (!write_name (abfd, sym_p->name, EXT_ABS << 24))
	    return false;
	  n[0]=sym_p->value;
	  if (!write_longs (n, 1,abfd))
	    return false;
	  continue;
	}/* Of abs def */
      if (osection == NULL) /* Happens with constructor functions. FIXME */
        continue;
      if (osection==bfd_abs_section_ptr) /* Not first hunk. Already written */
	continue;

      /* If it is a warning symbol, or a constructor symbol or a
	 debugging or a local symbol, don't write it */
      if (sym_p->flags & (BSF_WARNING|BSF_CONSTRUCTOR|BSF_DEBUGGING|BSF_LOCAL))
	continue;

      if (!(sym_p->flags & BSF_GLOBAL))
	continue;

      /* Now, if osection==section, write it out */
      if (osection->output_section==section)
	{
	  DPRINT(5,("Writing it out\n"));

	  if ((symbol_count++)==0) /* First write out the header */
	    {
	      if (!write_longs (&symbol_header, 1, abfd))
		return false;
	    }
	  type=((symbol_header==HUNK_EXT?EXT_DEF:0)<<24)&0xff000000;

	  if (!write_name (abfd, sym_p->name, type))
	    return false;
	  n[0] = sym_p->value + sym_p->section->output_offset;
	  if (!write_longs (n, 1, abfd))
	    return false;
	}
      else
	{
	  /* write common definitions as bss common references */
	  if (osection->output_section == bfd_com_section_ptr &&
	      section->index == 2)
	    {
	      if ((symbol_count++)==0) /* First write out the header */
		{
		  if (!write_longs (&symbol_header, 1, abfd))
		    return false;
		}
	      if (!write_name (abfd, sym_p->name, EXT_COMMON<<24))
		return false;

	      n[0]=sym_p->value;
	      n[1]=0;
	      if (!write_longs (n, 2, abfd))
		return false;
	    }
	}
    }/* Of for */

  DPRINT(10,("Did traversing\n"));
  if (symbol_count) /* terminate HUNK_EXT, HUNK_SYMBOL */
    {
      n[0]=0;
      if (!write_longs (n, 1, abfd))
	return false;
    }
  DPRINT(5,("Leaving\n"));
  goto alldone; /* Write HUNK_END, return */
}

static boolean
amiga_get_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  long disk_size = amiga_per_section (section)->disk_size;

  if (bfd_seek (abfd, section->filepos + offset, SEEK_SET))
    return false;

  if (offset+count > disk_size) {
    /* the section's size on disk may be smaller than in memory
       in this case, pad the contents */
    if (bfd_read (location, 1, disk_size-offset, abfd) != disk_size-offset)
      return false;
    memset ((char *) location + disk_size - offset, 0, count-(disk_size-offset));
  }
  else {
    if (bfd_read (location, 1, count, abfd) != disk_size-offset)
      return false;
  }
  return true;
}


boolean
amiga_new_section_hook (abfd, newsect)
     bfd *abfd;
     asection *newsect;
{
  newsect->used_by_bfd = (PTR) bfd_zalloc (abfd,
					   sizeof (amiga_per_section_type));
  newsect->alignment_power = 2;
  amiga_per_section(newsect)->reloc_tail = NULL;
  if (!strcmp (newsect->name, ".data_chip") || !strcmp (newsect->name,
						       ".bss_chip"))
      amiga_per_section(newsect)->attribute |= MEMF_CHIP;
  return true;
}

static boolean
amiga_slurp_symbol_table (abfd)
     bfd *abfd;
{
  amiga_data_type *amiga_data=AMIGA_DATA(abfd);
  asection *section;
  struct amiga_raw_symbol *sp;
  amiga_symbol_type *asp=NULL;
  unsigned long *lp, l, buf[4];
  unsigned long len, type;

  if (amiga_data->symbols)
    return true; /* already read */

  if (abfd->symcount)
    asp = (amiga_symbol_type*) bfd_alloc (abfd, sizeof (amiga_symbol_type) *
					     abfd->symcount);
  else
    return true;

  if (abfd->symcount!=0 && !asp) {
    bfd_set_error (bfd_error_no_memory);
    return false;
  }

  amiga_data->symbols = asp;

  /* Symbols are associated with every section */
  for (section=abfd->sections; section!=NULL; section=section->next)
    {
      if (amiga_per_section(section)->hunk_ext_pos == 0)
	continue;

      if (bfd_seek (abfd, amiga_per_section(section)->hunk_ext_pos, SEEK_SET))
	return false;

      amiga_per_section(section)->amiga_symbols = asp;

      while (get_long (abfd, &l) && (l!=0))
	{
	  type = l>>24;	/* type of entry */
	  len = (l & 0xffffff) << 2; /* namelength */

	  /* read the name */
	  if ((asp->symbol.name = bfd_alloc (abfd ,len+1))==NULL)
	    {
	      bfd_set_error (bfd_error_no_memory);
	      return false;
	    }
	  if (bfd_read ((PTR)asp->symbol.name, 1, len, abfd) != len)
	    return false;
	  ((char*)asp->symbol.name)[len] = '\0';

	  asp->symbol.the_bfd = abfd;
	  asp->type = type;
	  asp->symbol.flags = BSF_GLOBAL;
	  asp->index = asp - amiga_data->symbols;

	  switch(type) {
	  case EXT_COMMON: /* Common reference/definition*/
	    asp->symbol.section = bfd_com_section_ptr;
	    asp->hunk_number = -3;
	    /* size of common block -> symbol's value */
	    if (!get_long (abfd, &l))
	      return false;
	    asp->symbol.value = l;
	    /* skip refs */
	    if (!(get_long (abfd, &l) && bfd_seek (abfd, l<<2, SEEK_CUR)==0))
	      return false;
	    break;
	  case EXT_ABS: /* Absolute */
	    asp->symbol.section = bfd_abs_section_ptr;
	    asp->hunk_number = -1;
	    goto rval;
	    break;
	  case EXT_DEF: /* Relative Definition */
	  case EXT_SYMB: /* Same as EXT_DEF for load files */
	    asp->symbol.section = section;
	    asp->hunk_number = section->target_index;
	  rval:
	    /* read the value */
	    if (get_long (abfd, &l))
	      asp->symbol.value = l;
	    else
	      return false;
	    break;

	  default: /* References to an undefined symbol */
	    asp->symbol.section = bfd_und_section_ptr;
	    asp->hunk_number = -2; /* undefined */
	    asp->symbol.flags = 0;
	    /* skip refs */
	    if (!(get_long (abfd, &l) && bfd_seek (abfd, l<<2, SEEK_CUR)==0))
	      return false;
	    break;
	  }
	  asp++;
	}
    }
  return true;
}


/* Get size of symtab */
long
amiga_get_symtab_upper_bound (abfd)
     bfd *abfd;
{
  if (!amiga_slurp_symbol_table (abfd))
    return -1;
   return (abfd->symcount+1) * (sizeof (amiga_symbol_type *));
}

long
amiga_get_symtab (abfd, location)
     bfd *abfd;
     asymbol **location;
{
  amiga_symbol_type *symp;
  int i=0;

  if(!amiga_slurp_symbol_table(abfd))
    return -1;

  if (abfd->symcount)
    {
      for (symp = AMIGA_DATA(abfd)->symbols; i < bfd_get_symcount (abfd);
	   i++, symp++)
	*location++ = &symp->symbol;
    }
  return abfd->symcount;
}

asymbol *
amiga_make_empty_symbol (abfd)
     bfd *abfd;
{
  amiga_symbol_type *new =
    (amiga_symbol_type *) bfd_zalloc (abfd, sizeof (amiga_symbol_type));
  new->symbol.the_bfd = abfd;
  return &new->symbol;
}



void
amiga_get_symbol_info (ignore_abfd, symbol, ret)
      bfd *ignore_abfd;
      asymbol *symbol;
      symbol_info *ret;
{
  bfd_symbol_info (symbol, ret);
  if (symbol->name[0] == ' ')
    ret->name = "* empty table entry ";
  if (symbol->section==bfd_abs_section_ptr)
    ret->type = (symbol->flags & BSF_LOCAL) ? 'a' : 'A';
}



void
amiga_print_symbol (ignore_abfd, afile,  symbol, how)
      bfd *ignore_abfd;
      PTR afile;
      asymbol *symbol;
      bfd_print_symbol_type how;
{
  FILE *file = (FILE *)afile;
  
  switch (how) {
  case bfd_print_symbol_name:
    fprintf(file, "%s", symbol->name);
    break;
  case bfd_print_symbol_more:
    fprintf(stderr,"%4x %2x %2x",
	    (unsigned int)((amiga_symbol(symbol)->hunk_number)&0xffff),0,
	    amiga_symbol(symbol)->type);
    break;
  case bfd_print_symbol_all:
      {
	CONST char *section_name = symbol->section->name;
	if (symbol->name[0] == ' ')
	  {
	    fprintf(file, "* empty table entry ");
	  }
	else
	  {
	    bfd_print_symbol_vandf ((PTR)file, symbol);

	    fprintf(file," %-5s %04x %02x %s",
		    section_name,
		    (unsigned int)((amiga_symbol(symbol)->hunk_number)&0xffff),
		    (unsigned) 0,                       /* ->other */
		    symbol->name);                      /* ->name */
	}
      }
    break;
  }
}

 long
amiga_get_reloc_upper_bound (abfd, asect)
     bfd *abfd;
     sec_ptr asect;
{
  if (bfd_get_format (abfd) != bfd_object)
    {
      bfd_set_error(bfd_error_invalid_operation);
      return 0;
    }
  return sizeof (arelent *) * (asect->reloc_count + 1);
}


static boolean
read_raw_relocs (abfd, section, d_offset, count)
    bfd *abfd;
    sec_ptr section;
    unsigned long d_offset;	/* offset in the bfd */
    unsigned long count;	/* number of relocs */
{
  unsigned long type, no ,offset, hunk_number, j;
  int index,br;

  if (bfd_seek (abfd, d_offset, SEEK_SET))
    return false;
  while (count > 0) 
    {
      /* first determine type of reloc */
      if (!get_long (abfd, &type))
	return false;

      switch (type)
	{
	case HUNK_RELOC32: /* 32 bit ref */
	case HUNK_RELOC16: /* 16 bit ref */
	case HUNK_RELOC8: /* 8 bit ref */
	case HUNK_DREL32: /* 32 bit ref baserel */
	case HUNK_DREL16: /* 16 bit baserel */
	case HUNK_DREL8: /* 8 bit baserel */
	  if (type < HUNK_DREL32)
	    { /*0:8bit, 1: 16bit, 2:32bit */
	      index=2-(type-HUNK_RELOC32);
	      br=0; /* not base relative */
	    }
	  else
	    {
	      index=2-(type-HUNK_DREL32);
	      br=1; /* base relative */
	    }

	  if (!get_long (abfd, &no))
	    return false;
	  while (no) /* read offsets and hunk number */
	    {
	      if (!get_long (abfd, &hunk_number))
		return false;
	      for (j=0; j<no; j++)
		{ /* add relocs */
		  if (!get_long (abfd, &offset) ||
		      !amiga_add_reloc (abfd, section, offset, NULL,
				       amiga_howto_array[br][index],hunk_number))
		    return false;
		}
	      count -= no;
	      if (!get_long (abfd, &no))
		return false;
	    }
	  break;

	default: /* error */
	  bfd_set_error (bfd_error_wrong_format);
	  return false;
	  break;
	}
    }

}


/* slurp in relocs , amiga_digest_file left various pointers for us*/
static boolean 
amiga_slurp_relocs (abfd, section, symbols)
     bfd *abfd;
     sec_ptr section;
     asymbol **symbols;
{
  amiga_data_type *amiga_data=AMIGA_DATA(abfd);
  struct amiga_raw_symbol *sp;
  amiga_symbol_type *asp;
  long *lp;
  unsigned long type, offset, hunk_number, no;
  int i,n,br,j;
  int index;
  long count;
  amiga_per_section_type *asect = amiga_per_section(section);

  if (section->relocation)
    return true;

  if (asect->raw_relocs8)
    if (!read_raw_relocs (abfd, section, asect->raw_relocs8,
			  asect->num_raw_relocs8))
      return false;

  if (asect->raw_relocs16)
    if (!read_raw_relocs (abfd, section, asect->raw_relocs16,
			  asect->num_raw_relocs16))
      return false;

  if (asect->raw_relocs32)
    if (!read_raw_relocs (abfd, section, asect->raw_relocs32,
			  asect->num_raw_relocs32))
      return false;

  /* Now step through the raw_symbols and add all relocs in them */
  if (!amiga_data->symbols && !amiga_slurp_symbol_table (abfd))
    return false;

  if (amiga_per_section(section)->hunk_ext_pos == 0)
    return true;

  if (bfd_seek (abfd, amiga_per_section(section)->hunk_ext_pos, SEEK_SET))
    return false;

  asp = amiga_per_section(section)->amiga_symbols;
  while (get_long (abfd, &n) && n!=0)
    {
      type = (n>>24) & 0xff;
      n &= 0xffffff;

      /* skip the name */
      if (bfd_seek (abfd, n<<2, SEEK_CUR))
	return false;

      switch (type)
	{
	case EXT_SYMB:
	case EXT_DEF:
	case EXT_ABS: /* no relocs here */
	  if (bfd_seek (abfd, sizeof (long), SEEK_CUR))
	    return false;
	  break;
	  /* same as below, but advance lp by one to skip common size */
	case EXT_COMMON:
	  if (bfd_seek (abfd, sizeof (long), SEEK_CUR))
	    return false;
	  /* Fall through */
	default: /* reference to something */
	  /* points to num of refs to hunk */
	  if (!get_long (abfd, &n))
	    return false;

	  /* Add relocs to this section, relative to asp */
	  /* determine howto first */
	  if (type==EXT_COMMON) /* 32 bit ref */
	    {
	      index=2;
	      br=0;
	    }
	  else
	    {
	      if (type>EXT_REF32)
		type--; /* skip EXT_COMMON gap */
		     
	      type-=EXT_REF32;
	      br=0;

	      if (type>2) /* base relative */
		{
		  type-=3;
		  br=1;
		}
	      index=2-type;
	    }/* of else */

	  for (i=0;i<n;i++) /* refs follow */
	    {
	      if (!get_long (abfd, &offset))
		return false;
	      if (!amiga_add_reloc (abfd, section, offset, abfd->outsymbols ?
				    (amiga_symbol_type*)abfd->outsymbols[asp->index]: asp,
				    amiga_howto_array[br][index],-4))
		return false;
	    }
	      
	  break;
	}/* of switch */
      asp++;
    }

  return true;

}/* Of slurp_relocs */


long
amiga_canonicalize_reloc (abfd, section, relptr, symbols)
     bfd *abfd;
     sec_ptr section;
     arelent **relptr;
     asymbol **symbols;
{
  amiga_reloc_type *src;
  int i=0;

  if (!section->relocation && !amiga_slurp_relocs (abfd, section, symbols))
    return -1;

  src = (amiga_reloc_type *)section->relocation;
  while (src != (amiga_reloc_type *) 0)
    {
      *relptr++ = &src->relent;
      src = src->next;
    }
  *relptr = (arelent *) 0;

  return section->reloc_count;
}


/* Set section contents */
/* We do it the following way: 
   if this is a bss section ==> error
   otherwise, we try to allocate space for this section,
   if  this has not already been done
   Then we set the memory area to the contents */
static boolean
amiga_set_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     unsigned char *location;
     file_ptr offset;
     int count;
{
  unsigned char *contents;

  if ((section->flags&SEC_HAS_CONTENTS)==0) /* BSS */
    {
      bfd_set_error(bfd_error_invalid_operation);
      return false;
    }
  
  if ((section->flags&SEC_IN_MEMORY)==0) /* Not in memory, so alloc space */
    {
      contents=bfd_zalloc(abfd,section->_raw_size);
      if (!contents)
	{
	  bfd_set_error(bfd_error_no_memory);
	  return false;
	}
      
      DPRINT(5,("Allocated %lx bytes at %lx\n",section->_raw_size,contents));

      section->contents=contents;
      section->flags|=SEC_IN_MEMORY;
    }
  else /* In memory */
    contents=section->contents;

  /* Copy mem */
  memmove(contents+offset,location,count);

  return(true);

}/* Of section_set_contents */


/* FIXME: Is this everything ? */
static boolean
amiga_set_arch_mach (abfd, arch, machine)
     bfd *abfd;
     enum bfd_architecture arch;
     unsigned long machine;
{
  bfd_default_set_arch_mach(abfd, arch, machine);

  if (arch == bfd_arch_m68k)
    {
      switch (machine)
	{
	case 68000:
	case 68008:
	case 68010:
	case 68020:
	case 68030:
	case 68040:
	case 68070:
	case 0:
	  return true;
	default:
	  return false;
	}
    }
  else if (arch == bfd_arch_powerpc)
    {
      return true;
    }
  return false;
}

static int
DEFUN(amiga_sizeof_headers,(ignore_abfd, ignore),
      bfd *ignore_abfd AND
      boolean ignore)
{
  /* The amiga hunk format doesn't have headers.*/
  return 0;
}

/* Provided a BFD, a section and an offset into the section, calculate
   and return the name of the source file and the line nearest to the
   wanted location.  */
boolean
amiga_find_nearest_line(abfd, section, symbols, offset, filename_ptr,
			functionname_ptr, line_ptr)
     bfd *abfd;
     asection *section;
     asymbol **symbols;
     bfd_vma offset;
     char **filename_ptr;
     char **functionname_ptr;
     int *line_ptr;
{
  /* FIXME (see aoutx.h, for example) */
  return false;
}

static const struct reloc_howto_struct *
amiga_bfd_reloc_type_lookup (abfd, code)
       bfd *abfd;
       bfd_reloc_code_real_type code;
{
  switch (code)
    {
    case BFD_RELOC_8_PCREL:  return &howto_hunk_reloc8;
    case BFD_RELOC_16_PCREL: return &howto_hunk_reloc16;
    case BFD_RELOC_CTOR:
    case BFD_RELOC_32_PCREL: return &howto_hunk_reloc32;
    case BFD_RELOC_8:        return &howto_hunk_reloc8;
    case BFD_RELOC_16:       return &howto_hunk_reloc16;
    case BFD_RELOC_32:       return &howto_hunk_reloc32;
      /* FIXME: Add more cases here for base relative relocs*/
    default:                 return 0;
    }
}

static boolean
amiga_bfd_copy_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  AMIGA_DATA(obfd)->IsLoadFile = AMIGA_DATA(ibfd)->IsLoadFile;
  return true;
}

static boolean
amiga_bfd_copy_private_section_data (ibfd, isec, obfd, osec)
     bfd *ibfd;
     asection *isec;
     bfd *obfd;
     asection *osec;
{
  if (bfd_get_flavour (osec->owner) == bfd_target_amiga_flavour) {
    amiga_per_section (osec)->disk_size = amiga_per_section (isec)->disk_size;
    amiga_per_section (osec)->attribute = amiga_per_section (isec)->attribute;
  }
  return true;
}

/* There is no armap in the amiga libraries, so we fill carsym entries
   one by one after having parsed the whole archive. */
static boolean
amiga_slurp_armap (abfd)
     bfd *abfd;
{
  unsigned long type, n, slen;
  struct arch_syms *syms;
  unsigned long symcount=0;
  carsym *defsyms, *csym;
  unsigned long *symblock,len;

  /* allocate the carsyms */
  syms = amiga_ardata(abfd)->defsyms;
  symcount = amiga_ardata(abfd)->defsym_count;

  defsyms = (carsym*) bfd_alloc (abfd, sizeof (carsym) * symcount);
  if (!defsyms)
    return false;

  bfd_ardata(abfd)->symdefs = defsyms;
  bfd_ardata(abfd)->symdef_count = symcount;

  csym = defsyms;
  while (syms) {
    if (bfd_seek (abfd, syms->offset, SEEK_SET))
      return false;
    symblock = (unsigned long*) bfd_alloc (abfd, syms->size);
    if (!symblock)
      return false;
    if (bfd_read (symblock, 1, syms->size, abfd) != syms->size)
      return false;
    while ((n=GL(symblock)) != 0)
      {
	symblock++;
	len = n & 0xffffff;
	type = (n>>24) & 0xff;
	switch (type) {
	case EXT_SYMB:
	case EXT_DEF:
	case EXT_ABS:
	  slen = len<<2;
	  csym->name = (char*)symblock;
	  if (*((char*)symblock+slen-1) != '\0')
	    *((char*)symblock+slen) = '\0';
	  csym->file_offset = syms->unit_offset;
	  csym++;
	  symblock += len+1;	/* name+value */
	  break;
	case EXT_REF8:
	case EXT_REF16:
	case EXT_REF32:
	case EXT_DEXT8:
	case EXT_DEXT16:
	case EXT_DEXT32:
	  symblock += len;
	  symblock += 1+GL(symblock);
	  break;
	case EXT_COMMON:
	  symblock += len+1;
	  symblock += 1+GL(symblock);
	  break;
	default: /* error */
	  fprintf (stderr, "unexpected type in hunk_ext\n");
	  return false;
	}
      }
    syms = syms->next;
  }
  bfd_has_map (abfd) = true;
  return true;
}

static void amiga_truncate_arname ()
{
}

static const struct bfd_target *
amiga_archive_p (abfd)
     bfd *abfd;
{
  long header;
  struct stat stat_buffer;
  int units;
  symindex symcount = 0;
  struct arch_syms *symbols = NULL;

  bfd_set_error (bfd_error_wrong_format);

  if (bfd_stat (abfd, &stat_buffer) < 0)
    return false;

  if (stat_buffer.st_size != 0)
    {
      /* scan the units */
      if (!parse_archive_units (abfd, &units, stat_buffer.st_size, false,
				&symbols, &symcount))
	return NULL;

      /* if there is only one unit, we consider it's an object, not an
	 archive. Obviously it's not always true but taking objects
	 for archives makes ld fail, so we don't have much of a choice */
      if (units == 1)
	return NULL;

    }

  if (abfd->arelt_data)
    arelt_size (abfd) = bfd_tell (abfd);

  bfd_seek (abfd, 0, SEEK_SET);
  abfd->arch_info = bfd_scan_arch ("m68k:68000");

  if (amiga_mkarchive (abfd))
    {
      amiga_ardata(abfd)->filesize = stat_buffer.st_size;
      bfd_ardata(abfd)->first_file_filepos = 0;
      amiga_ardata(abfd)->defsym_count = symcount;
      amiga_ardata(abfd)->defsyms = symbols;
      if (amiga_slurp_armap (abfd))
	return abfd->xvec;
      else
	return NULL;
    }
    else
      return NULL;
}

static bfd *
amiga_openr_next_archived_file (archive, last_file)
     bfd *archive;
     bfd *last_file;
{
  file_ptr filestart;

  if (!last_file)
    filestart = bfd_ardata(archive)->first_file_filepos;
  else
    {
      unsigned int size = arelt_size (last_file);
      /* Pad to an even boundary... */
      filestart = last_file->origin + size;
      filestart += filestart % 2;
    }

  return _bfd_get_elt_at_filepos (archive, filestart);
}

static PTR
amiga_read_ar_hdr (abfd)
   bfd *abfd;
{
  struct areltdata *ared;
  char *filename = NULL;
  unsigned long header, read, len, start_pos;

  start_pos = bfd_tell (abfd);

  if (start_pos >= amiga_ardata(abfd)->filesize) {
    bfd_set_error (bfd_error_no_more_archived_files);
    return NULL;
  }

  if (bfd_read ((PTR) &header, 1, sizeof (header), abfd) != sizeof (header))
      return NULL;

  if (GL(&header) != HUNK_UNIT)
    {
      bfd_set_error (bfd_error_malformed_archive);
      return NULL;
    }

  /* get the unit name length in long words */
  if (!get_long (abfd, &len))
    return NULL;
  len = len << 2;

  ared = bfd_zalloc (abfd, sizeof (struct areltdata));
  if (ared == NULL) {
    bfd_set_error (bfd_error_no_memory);
    return NULL;
  }

  if (len)
    {
      if (!(ared->filename =  bfd_alloc (abfd, len+1)))
	{
	  bfd_set_error (bfd_error_no_memory);
	  return NULL;
	}
      else
	{
	  if (bfd_read (ared->filename, 1, len, abfd) != len)
	    return NULL;
	  ared->filename[len] = '\0';
	}
    }
  else
    ared->filename = "(no name)";

  if (bfd_seek (abfd, start_pos+4, SEEK_SET))
    return false;

  if (!amiga_read_unit (abfd, amiga_ardata(abfd)->filesize))
    return NULL;

  ared->parsed_size = bfd_tell(abfd)-start_pos;
  if (bfd_seek (abfd, start_pos, SEEK_SET))
    return false;
  return (PTR) ared;
}

int
amiga_generic_stat_arch_elt (abfd, buf)
   bfd *abfd;
   struct stat *buf;
{
  if (abfd->arelt_data == NULL)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return -1;
    }

  /* No header in amiga archives. Let's set reasonable default values */
  buf->st_mtime = 0;
  buf->st_uid = 0;
  buf->st_gid = 0;
  buf->st_mode = 0666;

  buf->st_size = ((struct areltdata*)(abfd->arelt_data))->parsed_size;

  return 0;
}

/* We don't have core files.  */
#define	amiga_core_file_failing_command _bfd_dummy_core_file_failing_command
#define	amiga_core_file_failing_signal _bfd_dummy_core_file_failing_signal
#define	amiga_core_file_matches_executable_p _bfd_dummy_core_file_matches_executable_p

/* Entry points through BFD_JUMP_TABLE_ARCHIVE */
/*#define	amiga_slurp_armap		bfd_slurp_amiga_armap*/
#define	amiga_slurp_extended_name_table	_bfd_slurp_extended_name_table
#define amiga_construct_extended_name_table _bfd_archive_bsd_construct_extended_name_table
/*#define	amiga_truncate_arname		bfd_gnu_truncate_arname*/
/*#define	amiga_write_armap		amiga_write_armap*/
/*#define	amiga_read_ar_hdr		_bfd_generic_read_ar_hdr*/
/*#define	amiga_openr_next_archived_file	bfd_generic_openr_next_archived_file*/

#define amiga_get_elt_at_index		_bfd_generic_get_elt_at_index
/* #define amiga_generic_stat_arch_elt	bfd_generic_stat_arch_elt */
#define amiga_update_armap_timestamp	_bfd_archive_bsd_update_armap_timestamp

/* Entry points through BFD_JUMP_TABLE_SYMBOLS */
#undef amiga_get_symtab_upper_bound	/* defined above */
#undef amiga_get_symtab			/* defined above */
#undef amiga_make_empty_symbol		/* defined above */
#undef amiga_print_symbol		/* defined above */
#undef amiga_get_symbol_info		/* defined above */
#define amiga_bfd_is_local_label	bfd_generic_is_local_label
#define amiga_get_lineno		(struct lineno_cache_entry *(*)())bfd_nullvoidptr
#undef amiga_find_nearest_line		/* defined above */
#define amiga_bfd_make_debug_symbol	(asymbol * (*)(bfd *, void *, unsigned long)) bfd_nullvoidptr
#define amiga_read_minisymbols		_bfd_generic_read_minisymbols
#define amiga_minisymbol_to_symbol	_bfd_generic_minisymbol_to_symbol

#define amiga_bfd_debug_info_start		bfd_void
#define amiga_bfd_debug_info_end		bfd_void
#define amiga_bfd_debug_info_accumulate	(PROTO(void,(*),(bfd*, struct sec *))) bfd_void
#define amiga_bfd_is_local_label_name bfd_generic_is_local_label_name

/* NOTE: We use a special get_relocated_section_contents both in amiga AND in a.out files.
   In addition, we use an own final_link routine, which is nearly identical to _bfd_generic_final_link */
extern bfd_byte *get_relocated_section_contents(bfd*, struct bfd_link_info *,
						struct bfd_link_order *, bfd_byte *,
						boolean, asymbol **);
#define amiga_bfd_get_relocated_section_contents get_relocated_section_contents
#define amiga_bfd_relax_section                   bfd_generic_relax_section

#define amiga_bfd_link_hash_table_create _bfd_generic_link_hash_table_create
#define amiga_bfd_link_add_symbols _bfd_generic_link_add_symbols
extern boolean amiga_final_link(bfd *, struct bfd_link_info *);
#define amiga_bfd_final_link amiga_final_link

/* Entry points through BFD_JUMP_TABLE_GENERIC */
#define amiga_close_and_cleanup         _bfd_generic_close_and_cleanup
#define amiga_bfd_free_cached_info	_bfd_generic_bfd_free_cached_info
/* amiga_new_section_hook defined above */
/* amiga_get_section_hook defined above */
#define amiga_get_section_contents_in_window _bfd_generic_get_section_contents_in_window

/* Entry points through BFD_JUMP_TABLE_COPY */
#define amiga_bfd_merge_private_bfd_data _bfd_generic_bfd_merge_private_bfd_data
/*#define amiga_bfd_copy_private_section_data _bfd_generic_bfd_copy_private_section_data*/
#define amiga_bfd_copy_private_symbol_data _bfd_generic_bfd_copy_private_symbol_data
#define amiga_bfd_set_private_flags _bfd_generic_bfd_set_private_flags
#define amiga_bfd_print_private_flags _bfd_generic_bfd_print_private_flags
#define amiga_bfd_print_private_bfd_data _bfd_generic_bfd_print_private_bfd_data

#define amiga_bfd_link_split_section  _bfd_generic_link_split_section

#if defined (amiga)
/* So that the JUMP_TABLE() macro below can work.  */
#undef amiga
#endif

const bfd_target amiga_vec =
{
  "amiga",		/* name */
  bfd_target_amiga_flavour,
  true,			/* data byte order is big */
  true,			/* header byte order is big */
  HAS_RELOC | EXEC_P | HAS_LINENO | HAS_DEBUG | HAS_SYMS | HAS_LOCALS | WP_TEXT, /* object flags */
 /* section flags */
  SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_CODE | SEC_DATA,
  '_',				/* symbol leading char */
  ' ',				/* ar_pad_char */
  15,				/* ar_max_namelen */	/* (15 for UNIX compatibility) */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64, bfd_getb32, bfd_getb_signed_32,
  bfd_putb32, bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64, bfd_getb32, bfd_getb_signed_32,
  bfd_putb32, bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* hdrs */
  {
    /* bfd_check_format */
    _bfd_dummy_target,
    amiga_object_p,
    amiga_archive_p,
    _bfd_dummy_target
  },
  {
    /* bfd_set_format */
    bfd_false,
    amiga_mkobject,
    amiga_mkarchive,
    bfd_false
  },
  {
    /* bfd_write_contents */
    bfd_false,
    amiga_write_object_contents,
    amiga_write_archive_contents,
    bfd_false
  },
  BFD_JUMP_TABLE_GENERIC (amiga),
  BFD_JUMP_TABLE_COPY (amiga),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (amiga),
  BFD_JUMP_TABLE_SYMBOLS (amiga),
  BFD_JUMP_TABLE_RELOCS (amiga),
  BFD_JUMP_TABLE_WRITE (amiga),
  BFD_JUMP_TABLE_LINK (amiga),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),
  (PTR) 0
#if 0
/* fixme: no longer in use?  */
  /* How applications can find out about amiga relocation types (see
     documentation on reloc types).  */
  amiga_reloc_type_lookup
#endif
};
