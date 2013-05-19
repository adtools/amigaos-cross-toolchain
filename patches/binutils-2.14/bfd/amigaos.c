/* BFD back-end for Commodore-Amiga AmigaOS binaries.
   Copyright (C) 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998
   Free Software Foundation, Inc.
   Contributed by Leonard Norrgard.  Partially based on the bout
   and ieee BFD backends and Markus Wild's tool hunk2gcc.
   Revised and updated by Stephan Thesing.

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
@* Does it work?::
@* TODO::
@end menu

INODE
not supported, Does it work?, implementation, implementation

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
	  information in the debug hunk and HUNK_SYMBOL or HUNK_EXT hunks be avoided?
I haven't decided yet, what to do about this.

Although bfd allows to link together object modules of different flavours,
producing a.out style executables does not work on Amiga :-)
It should, however, be possible to create a.out files with the -r option of ld
(incremental link).

INODE
Does it work?, TODO, not supported, implementation

SUBSECTION
	Does it work?

Currently, the following utilities work:
	o objdump
	o objcopy
	o strip
	o nm
	o ar
	o gas

INODE
TODO, , Does it work?, implementation

SUBSECTION
	TODO

	o fix FIXME:s

@*
BFD:
	o add flag to say if the format allows multiple sections with the
	  same name. Fix bfd_get_section_by_name() and bfd_make_section()
	  accordingly.

	o dumpobj.c: the disassembler: use relocation record data to find symbolic
	  names of addresses, when available.  Needs new routine where one can
	  specify the source section of the symbol to be printed as well as some
	  rewrite of the disassemble functions.
*/

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "libamiga.h"

#define BYTES_IN_WORD 4
#include "aout/aout64.h" /* struct external_nlist */

#ifndef alloca
extern PTR alloca PARAMS ((size_t));
#endif

#define bfd_is_bfd_section(sec) \
  (bfd_is_abs_section(sec)||bfd_is_com_section(sec)||bfd_is_und_section(sec)||bfd_is_ind_section(sec))

struct arch_syms {
  unsigned long offset;		/* disk offset in the archive */
  unsigned long size;		/* size of the block of symbols */
  unsigned long unit_offset;	/* start of unit on disk */
  struct arch_syms *next;	/* linked list */
};

typedef struct amiga_ardata_struct {
  /* generic stuff */
  struct artdata generic;
  /* amiga-specific stuff */
  unsigned long filesize;
  struct arch_syms *defsyms;
  unsigned long defsym_count;
  unsigned long outnum;
} amiga_ardata_type;

#define amiga_ardata(bfd) (*(amiga_ardata_type **)(void *)&(bfd)->tdata.aout_ar_data)

#define bfd_msg (*_bfd_error_handler)

#define GL(x) bfd_get_32 (abfd, (bfd_byte *) (x))
#define GW(x) bfd_get_16 (abfd, (bfd_byte *) (x))
#define LONGSIZE(l) (((l)+3) >> 2)

/* AmigaOS doesn't like HUNK_SYMBOL with symbol names longer than 124 characters */
#define MAX_NAME_SIZE 124

static bfd_boolean get_long PARAMS ((bfd *, unsigned long *));
static const struct bfd_target *amiga_object_p PARAMS ((bfd *));
static sec_ptr amiga_get_section_by_hunk_number PARAMS ((bfd *, long));
static bfd_boolean amiga_add_reloc PARAMS ((bfd *, sec_ptr, bfd_size_type,
	amiga_symbol_type *, reloc_howto_type *, long));
static sec_ptr amiga_make_unique_section PARAMS ((bfd *, const char *));
static bfd_boolean parse_archive_units PARAMS ((bfd *, int *, unsigned long,
 	bfd_boolean, struct arch_syms **, symindex *));
static bfd_boolean amiga_digest_file PARAMS ((bfd *));
static bfd_boolean amiga_read_unit PARAMS ((bfd *, unsigned long));
static bfd_boolean amiga_read_load PARAMS ((bfd *));
static bfd_boolean amiga_handle_cdb_hunk PARAMS ((bfd *, unsigned long,
	unsigned long, unsigned long, unsigned long));
static bfd_boolean amiga_handle_rest PARAMS ((bfd *, sec_ptr, bfd_boolean));
static bfd_boolean amiga_mkobject PARAMS ((bfd *));
static bfd_boolean amiga_mkarchive PARAMS ((bfd *));
static bfd_boolean write_longs PARAMS ((const unsigned long *, unsigned long,
	bfd *));
static long determine_datadata_relocs PARAMS ((bfd *, sec_ptr));
static void remove_section_index PARAMS ((sec_ptr, int *));
static bfd_boolean amiga_write_object_contents PARAMS ((bfd *));
static bfd_boolean write_name PARAMS ((bfd *, const char *, unsigned long));
static bfd_boolean amiga_write_archive_contents PARAMS ((bfd *));
static bfd_boolean amiga_write_armap PARAMS ((bfd *, unsigned int,
	struct orl *, unsigned int, int));
static int determine_type PARAMS ((arelent *));
static bfd_boolean amiga_write_section_contents PARAMS ((bfd *, sec_ptr,
	sec_ptr, unsigned long, int *, int));
static bfd_boolean amiga_write_symbols PARAMS ((bfd *, sec_ptr));
static bfd_boolean amiga_get_section_contents PARAMS ((bfd *, sec_ptr, PTR,
	file_ptr, bfd_size_type));
static bfd_boolean amiga_new_section_hook PARAMS ((bfd *, sec_ptr));
static bfd_boolean amiga_slurp_symbol_table PARAMS ((bfd *));
static long amiga_get_symtab_upper_bound PARAMS ((bfd *));
static long amiga_get_symtab PARAMS ((bfd *, asymbol **));
static asymbol *amiga_make_empty_symbol PARAMS ((bfd *));
static void amiga_get_symbol_info PARAMS ((bfd *, asymbol *, symbol_info *));
static void amiga_print_symbol PARAMS ((bfd *, PTR,   asymbol *,
	bfd_print_symbol_type));
static long amiga_get_reloc_upper_bound PARAMS ((bfd *, sec_ptr));
static bfd_boolean read_raw_relocs PARAMS ((bfd *, sec_ptr, unsigned long,
	unsigned long));
static bfd_boolean amiga_slurp_relocs PARAMS ((bfd *, sec_ptr, asymbol **));
static long amiga_canonicalize_reloc PARAMS ((bfd *, sec_ptr, arelent **,
	asymbol **));
static bfd_boolean amiga_set_section_contents PARAMS ((bfd *, sec_ptr, PTR,
	file_ptr, bfd_size_type));
static bfd_boolean amiga_set_arch_mach PARAMS ((bfd *, enum bfd_architecture,
	unsigned long));
static int amiga_sizeof_headers PARAMS ((bfd *, bfd_boolean));
static bfd_boolean amiga_find_nearest_line PARAMS ((bfd *, sec_ptr,
	asymbol **, bfd_vma, const char **, const char **, unsigned int *));
static reloc_howto_type *amiga_bfd_reloc_type_lookup PARAMS ((bfd *,
	bfd_reloc_code_real_type));
static bfd_boolean amiga_bfd_copy_private_bfd_data PARAMS ((bfd *, bfd *));
static bfd_boolean amiga_bfd_copy_private_section_data PARAMS ((bfd *,
	sec_ptr, bfd *, sec_ptr));
static bfd_boolean amiga_slurp_armap PARAMS ((bfd *));
static void amiga_truncate_arname PARAMS ((bfd *, const char *, char *));
static const struct bfd_target *amiga_archive_p PARAMS ((bfd *));
static bfd *amiga_openr_next_archived_file PARAMS ((bfd *, bfd *));
static PTR amiga_read_ar_hdr PARAMS ((bfd *));
static int amiga_generic_stat_arch_elt PARAMS ((bfd *, struct stat *));

/*#define DEBUG_AMIGA 1*/
#if DEBUG_AMIGA
#include <stdarg.h>
static void
error_print (const char *fmt, ...)
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

enum {R_ABS32=0,R_PC16,R_PC8,R_SD32,R_SD16,R_SD8,R_ABS32SHORT,R_PC26,R_PC32,R__MAX};
static reloc_howto_type howto_table[R__MAX] =
{
  {H_ABS32,   /* type */
  0,          /* rightshift */
  2,          /* size */
  32,         /* bitsize */
  FALSE,      /* pc_relative */
  0,          /* bitpos */
  complain_overflow_bitfield,/* complain_on_overflow */
  0,          /* special_function */
  "RELOC32",  /* textual name */
  FALSE,      /* partial_inplace */
  0xffffffff, /* src_mask */
  0xffffffff, /* dst_mask */
  FALSE},     /* pcrel_offset */
  {H_PC16,       0, 1, 16, TRUE,  0, complain_overflow_signed,   0, "RELRELOC16",   FALSE, 0x0000ffff, 0x0000ffff, TRUE},
  {H_PC8,        0, 0,  8, TRUE,  0, complain_overflow_signed,   0, "RELRELOC8",    FALSE, 0x000000ff, 0x000000ff, TRUE},
  {H_SD32,       0, 2, 32, FALSE, 0, complain_overflow_bitfield, 0, "DREL32",       FALSE, 0xffffffff, 0xffffffff, FALSE},
  {H_SD16,       0, 1, 16, FALSE, 0, complain_overflow_bitfield, 0, "DREL16",       FALSE, 0x0000ffff, 0x0000ffff, FALSE},
  {H_SD8,        0, 0,  8, FALSE, 0, complain_overflow_bitfield, 0, "DREL8",        FALSE, 0x000000ff, 0x000000ff, FALSE},
  {H_ABS32SHORT, 0, 1, 16, FALSE, 0, complain_overflow_bitfield, 0, "RELOC32SHORT", FALSE, 0x0000ffff, 0x0000ffff, FALSE},
  {H_PC26,       0, 2, 26, TRUE,  0, complain_overflow_signed,   0, "RELRELOC26",   FALSE, 0x03fffffc, 0x03fffffc, TRUE},
  {H_PC32,       0, 2, 32, TRUE,  0, complain_overflow_signed,   0, "RELRELOC32",   FALSE, 0xffffffff, 0xffffffff, TRUE}
};

/* The following are gross hacks that need to be fixed.  The problem is
   that the linker unconditionally references these values without
   going through any of bfd's standard interface.  Thus they need to
   be defined in a bfd module that is included in *all* configurations,
   and are currently in bfd.c, otherwise linking the linker will fail
   on non-Amiga target configurations. */

/* This one is used by the linker and tells us, if a debug hunk should
   be written out. */
extern int write_debug_hunk;

/* This is also used by the linker to set the attribute of sections. */
extern int amiga_attribute;

/* used with base-relative linking */
extern int amiga_base_relative;

/* used with -resident linking */
extern int amiga_resident;

static bfd_boolean
get_long (abfd, n)
     bfd *abfd;
     unsigned long *n;
{
  if (bfd_bread ((PTR)n, 4, abfd) != 4)
    return FALSE;
  *n = GL (n);
  return TRUE;
}

static const struct bfd_target *
amiga_object_p (abfd)
     bfd *abfd;
{
  unsigned long x;
  char buf[8];

  /* An Amiga object file must be at least 8 bytes long.  */
  if (bfd_bread (buf, sizeof(buf), abfd) != sizeof(buf))
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  bfd_seek (abfd, 0, SEEK_SET);

  /* Does it look like an Amiga object file?  */
  x = GL (&buf[0]);
  if ((x != HUNK_UNIT) && (x != HUNK_HEADER))
    {
      /* Not an Amiga file.  */
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  /* Can't fail and return (but must be declared bfd_boolean to suit
     other bfd requirements).  */
  (void) amiga_mkobject (abfd);

  AMIGA_DATA(abfd)->IsLoadFile = (x == HUNK_HEADER);

  if (!amiga_digest_file (abfd))
    {
      /* Something went wrong.  */
      DPRINT(20,("bfd parser stopped at offset 0x%lx\n",bfd_tell(abfd)));
      return NULL;
    }

  /* Set default architecture to m68k:68000.  */
  /* So we can link on 68000 AMIGAs... */
  abfd->arch_info = bfd_scan_arch ("m68k:68000");

  return abfd->xvec;
}

static sec_ptr
amiga_get_section_by_hunk_number (abfd, hunk_number)
     bfd *abfd;
     long hunk_number;
{
  /* A cache, so we don't have to search the entire list every time.  */
  static sec_ptr last_reference;
  static bfd *last_bfd;
  sec_ptr p;

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
  BFD_FAIL ();
  return NULL;
}

static bfd_boolean
amiga_add_reloc (abfd, section, offset, symbol, howto, target_hunk)
     bfd *abfd;
     sec_ptr section;
     bfd_size_type offset;
     amiga_symbol_type *symbol;
     reloc_howto_type *howto;
     long target_hunk;
{
  amiga_reloc_type *reloc;
  sec_ptr target_sec;

  reloc = (amiga_reloc_type *) bfd_alloc (abfd, sizeof (amiga_reloc_type));
  if (reloc == NULL)
    return FALSE;

  abfd->flags |= HAS_RELOC;
  section->flags |= SEC_RELOC;

  if (amiga_per_section(section)->reloc_tail)
    amiga_per_section(section)->reloc_tail->next = reloc;
  else
    section->relocation = &reloc->relent;
  amiga_per_section(section)->reloc_tail = reloc;

  reloc->relent.sym_ptr_ptr = &reloc->symbol;
  reloc->relent.address = offset;
  reloc->relent.addend = 0;
  reloc->relent.howto = howto;

  reloc->next = NULL;
  if (symbol==NULL) {		/* relative to section */
    target_sec = amiga_get_section_by_hunk_number (abfd, target_hunk);
    if (target_sec)
      reloc->symbol = target_sec->symbol;
    else
      return FALSE;
  }
  else
    reloc->symbol = &symbol->symbol;

  return TRUE;
}

/* BFD doesn't currently allow multiple sections with the same
   name, so we try a little harder to get a unique name.  */
static sec_ptr
amiga_make_unique_section (abfd, name)
     bfd *abfd;
     const char *name;
{
  sec_ptr section;

  bfd_set_error (bfd_error_no_error);
  section = bfd_make_section (abfd, name);
  if ((section == NULL) && (bfd_get_error() == bfd_error_no_error))
    {
#if 0
      char *new_name = bfd_alloc (abfd, strlen(name) + 4);
      int i = 1;

      /* We try to come up with an original name (since BFD currently
	 requires all sections to have different names).  */
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
#define DPRINTHUNK(x) fprintf(stderr,"Processing %s hunk (0x%x)...",\
	(x) == HUNK_UNIT ? "HUNK_UNIT" :\
	(x) == HUNK_NAME ? "HUNK_NAME" :\
	(x) == HUNK_CODE ? "HUNK_CODE" :\
	(x) == HUNK_DATA ? "HUNK_DATA" :\
	(x) == HUNK_BSS ? "HUNK_BSS" :\
	(x) == HUNK_ABSRELOC32 ? "HUNK_RELOC32" :\
	(x) == HUNK_RELRELOC16 ? "HUNK_RELRELOC16" :\
	(x) == HUNK_RELRELOC8 ? "HUNK_RELRELOC8" :\
	(x) == HUNK_EXT ? "HUNK_EXT" :\
	(x) == HUNK_SYMBOL ? "HUNK_SYMBOL" :\
	(x) == HUNK_DEBUG ? "HUNK_DEBUG" :\
	(x) == HUNK_END ? "HUNK_END" :\
	(x) == HUNK_HEADER ? "HUNK_HEADER" :\
	(x) == HUNK_OVERLAY ? "HUNK_OVERLAY" :\
	(x) == HUNK_BREAK ? "HUNK_BREAK" :\
	(x) == HUNK_DREL32 ? "HUNK_DREL32" :\
	(x) == HUNK_DREL16 ? "HUNK_DREL16" :\
	(x) == HUNK_DREL8 ? "HUNK_DREL8" :\
	(x) == HUNK_LIB ? "HUNK_LIB" :\
	(x) == HUNK_INDEX ? "HUNK_INDEX" :\
	(x) == HUNK_RELOC32SHORT ? "HUNK_RELOC32SHORT" :\
	(x) == HUNK_RELRELOC32 ? "HUNK_RELRELOC32" :\
	(x) == HUNK_PPC_CODE ? "HUNK_PPC_CODE" :\
	(x) == HUNK_RELRELOC26 ? "HUNK_RELRELOC26" :\
	"*unknown*",(x))
#define DPRINTHUNKEND fprintf(stderr,"done\n")
#else
#define DPRINTHUNK(x)
#define DPRINTHUNKEND
#endif

static bfd_boolean
parse_archive_units (abfd, n_units, filesize, one, syms, symcount)
     bfd *abfd;
     int *n_units;
     unsigned long filesize;
     bfd_boolean one;			/* parse only the first unit? */
     struct arch_syms **syms;
     symindex *symcount;
{
  struct arch_syms *nsyms,*syms_tail=NULL;
  unsigned long unit_offset,defsym_pos=0;
  unsigned long hunk_type,type,len,no,n;
  symindex defsymcount=0;

  *n_units = 0;
  while (get_long (abfd, &hunk_type)) {
    switch (hunk_type) {
    case HUNK_END:
      break;
    case HUNK_UNIT:
      unit_offset = bfd_tell (abfd) - 4;
      (*n_units)++;
      if (one && *n_units>1) {
	bfd_seek (abfd, -4, SEEK_CUR);
	return TRUE;
      }
      /* Fall through */
    case HUNK_NAME:
    case HUNK_CODE:
    case HUNK_DATA:
    case HUNK_DEBUG:
    case HUNK_PPC_CODE:
      if (!get_long (abfd, &len)
	  || bfd_seek (abfd, HUNK_VALUE (len) << 2, SEEK_CUR))
	return FALSE;
      break;
    case HUNK_BSS:
      if (!get_long (abfd, &len))
	return FALSE;
      break;
    case HUNK_ABSRELOC32:
    case HUNK_RELRELOC16:
    case HUNK_RELRELOC8:
    case HUNK_SYMBOL:
    case HUNK_DREL32:
    case HUNK_DREL16:
    case HUNK_DREL8:
      for (;;) {
	/* read offsets count */
	if (!get_long (abfd, &no))
	  return FALSE;
	if (!no)
	  break;
	/* skip hunk+offsets */
	if (bfd_seek (abfd, (no+1)<<2, SEEK_CUR))
	  return FALSE;
      }
      break;
    case HUNK_EXT:
      defsym_pos = 0;
      if (!get_long (abfd, &n))
	return FALSE;
      while (n) {
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
	    defsym_pos = bfd_tell (abfd) - 4;
	  /* skip name & value */
	  if (bfd_seek (abfd, (len+1)<<2, SEEK_CUR))
	    return FALSE;
	  defsymcount++;
	  break;

	case EXT_ABSREF32:
	case EXT_RELREF16:
	case EXT_RELREF8:
	case EXT_DEXT32:
	case EXT_DEXT16:
	case EXT_DEXT8:
	case EXT_RELREF32:
	case EXT_RELREF26:
	  /* skip name */
	  if (bfd_seek (abfd, len<<2, SEEK_CUR))
	    return FALSE;
	  /* skip references */
	  if (!get_long (abfd, &no))
	    return FALSE;
	  if (no && bfd_seek (abfd, no<<2, SEEK_CUR))
	    return FALSE;
	  break;

	case EXT_ABSCOMMON:
	case EXT_DEXT32COMMON:
	case EXT_DEXT16COMMON:
	case EXT_DEXT8COMMON:
	  /* skip name & value */
	  if (bfd_seek (abfd, (len+1)<<2, SEEK_CUR))
	    return FALSE;
	  /* skip references */
	  if (!get_long (abfd, &no))
	    return FALSE;
	  if (no && bfd_seek (abfd, no<<2, SEEK_CUR))
	    return FALSE;
	  break;

	default: /* error */
	  bfd_msg ("unexpected type %ld(0x%lx) in hunk_ext1 at offset 0x%lx",
		   type, type, bfd_tell (abfd));
	  return FALSE;
	}

	if (!get_long (abfd, &n))
	  return FALSE;
      }
      if (defsym_pos != 0 && syms) {
	/* there are some defined symbols, keep enough information on
	   them to simulate an armap later on */
	nsyms = (struct arch_syms *) bfd_alloc (abfd, sizeof (struct arch_syms));
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
      bfd_msg ("unexpected hunk 0x%lx at offset 0x%lx",
	       hunk_type, bfd_tell (abfd));
#endif
      return FALSE;
    }
  }
  if (syms && symcount)
    *symcount = defsymcount;
  return (bfd_tell (abfd) == filesize);
}

static bfd_boolean
amiga_digest_file (abfd)
     bfd *abfd;
{
  struct stat stat_buffer;
  unsigned long tmp;

  if (!get_long (abfd, &tmp))
    {
      bfd_set_error (bfd_error_wrong_format);
      return FALSE;
    }

  switch (HUNK_VALUE (tmp))
    {
    case HUNK_UNIT:
      /* Read the unit(s) */
      if (bfd_stat (abfd, &stat_buffer) < 0)
	return FALSE;
/*
      while ((pos=bfd_tell (abfd)) < stat_buffer.st_size)
	{*/
      if (!amiga_read_unit (abfd, stat_buffer.st_size - abfd->origin))
	return FALSE;
      if (abfd->arelt_data)
	arelt_size (abfd) = bfd_tell (abfd);
/*	}*/
      break;

    case HUNK_HEADER:
      /* This is a load file */
      if (!amiga_read_load (abfd))
	return FALSE;
      break;
    }

  return TRUE;
}/* of amiga_digest_file */


/* Read in Unit file */
/* file pointer is located after the HUNK_UNIT LW */
static bfd_boolean
amiga_read_unit (abfd, size)
     bfd *abfd;
     unsigned long size;
{
  unsigned long hunk_number=0,hunk_type,tmp;

  /* read LW length of unit's name */
  if (!get_long (abfd, &tmp))
    return FALSE;

  /* and skip it (FIXME maybe) */
  if (bfd_seek (abfd, tmp<<2, SEEK_CUR))
    return FALSE;

  while (bfd_tell (abfd) < size)
    {
      if (!get_long (abfd, &tmp))
	return FALSE;

      /* Now there may be CODE, DATA, BSS, SYMBOL, DEBUG, RELOC Hunks */
      hunk_type = HUNK_VALUE (tmp);
      switch (hunk_type)
	{
	case HUNK_UNIT:
	  /* next unit, seek back and return */
	  return (bfd_seek (abfd, -4, SEEK_CUR) == 0);

	case HUNK_DEBUG:
	  /* we don't parse hunk_debug at the moment */
	  if (!get_long (abfd, &tmp) || bfd_seek (abfd, tmp<<2, SEEK_CUR))
	    return FALSE;
	  break;

	case HUNK_NAME:
	case HUNK_CODE:
	case HUNK_DATA:
	case HUNK_BSS:
	case HUNK_PPC_CODE:
	  /* Handle this hunk, including relocs, etc.
	     The finishing HUNK_END is consumed by the routine */
	  if (!amiga_handle_cdb_hunk (abfd, hunk_type, hunk_number++, 0, -1))
	    return FALSE;
	  break;

	default:
	  /* Something very nasty happened: invalid hunk occured... */
	  bfd_set_error (bfd_error_wrong_format);
	  return FALSE;
	  break;
	}/* Of switch hunk_type */

      /* Next hunk */
    }
  return TRUE;
}


/* Read a load file */
static bfd_boolean
amiga_read_load (abfd)
     bfd *abfd;
{
  unsigned long max_hunk_number,hunk_type,tmp,i;
  unsigned long *hunk_attributes,*hunk_sizes;
  char buf[16];

  /* Read hunk lengths (and memory attributes...) */
  /* Read in each hunk */

  if (bfd_bread (buf, sizeof(buf), abfd) != sizeof(buf))
    return FALSE;

  /* If there are resident libs: abort (obsolete feature) */
  if (GL (&buf[0]) != 0)
    return FALSE;

  max_hunk_number = GL (&buf[4]);

  /* Sanity */
  if (max_hunk_number<1)
    {
      bfd_set_error (bfd_error_wrong_format);
      return FALSE;
    }

  AMIGA_DATA(abfd)->nb_hunks = max_hunk_number;

  /* Num of root hunk must be 0 */
  if (GL (&buf[8]) != 0)
    {
      bfd_set_error (bfd_error_wrong_format);
      return FALSE;
    }

  /* Num of last hunk must be mhn-1 */
  if (GL (&buf[12]) != max_hunk_number-1)
    {
      bfd_msg ("Overlay loadfiles are not supported");
      bfd_set_error (bfd_error_wrong_format);
      return FALSE;
    }

  hunk_sizes = alloca (max_hunk_number * sizeof (unsigned long));
  hunk_attributes = alloca (max_hunk_number * sizeof (unsigned long));
  if (hunk_sizes == NULL || hunk_attributes == NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return FALSE;
    }

  /* Now, read in sizes and memory attributes */
  for (i=0; i<max_hunk_number; i++)
    {
      if (!get_long (abfd, &hunk_sizes[i]))
	return FALSE;
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
	    return FALSE;
	  break;
	default:
	  hunk_attributes[i] = 0;
	  break;
	}
      hunk_sizes[i] = HUNK_VALUE (hunk_sizes[i]) << 2;
    }

  for (i=0; i<max_hunk_number; i++)
    {
      if (!get_long (abfd, &tmp))
	return FALSE;

      /* This may be HUNK_NAME, CODE, DATA, BSS, DEBUG */
      hunk_type = HUNK_VALUE (tmp);
      switch (hunk_type)
	{
	case HUNK_NAME:
	case HUNK_CODE:
	case HUNK_DATA:
	case HUNK_BSS:
	case HUNK_PPC_CODE:
	  if (!amiga_handle_cdb_hunk (abfd, hunk_type, i,
				      hunk_attributes[i], hunk_sizes[i]))
	    {
	      bfd_set_error (bfd_error_wrong_format);
	      return FALSE;
	    }
	  break;

	case HUNK_DEBUG:
	  if (--i,!amiga_handle_cdb_hunk (abfd, hunk_type, -1, 0, 0))
	    {
	      bfd_set_error (bfd_error_wrong_format);
	      return FALSE;
	    }
	  break;

	default:
	  /* invalid hunk */
	  bfd_set_error (bfd_error_wrong_format);
	  return FALSE;
	  break;
	}/* Of switch */
    }

  return TRUE;
}/* Of amiga_read_load */


/* Handle NAME, CODE, DATA, BSS, DEBUG Hunks */
static bfd_boolean
amiga_handle_cdb_hunk (abfd, hunk_type, hunk_number, hunk_attribute,
		       hunk_size)
     bfd *abfd;
     unsigned long hunk_type;
     unsigned long hunk_number;
     unsigned long hunk_attribute;
     unsigned long hunk_size;
/* If hunk_size==-1, then we are digesting a HUNK_UNIT */
{
  sec_ptr current_section;
  char *sec_name,*current_name=NULL;
  unsigned long len,tmp;
  int secflags,is_load=(hunk_size!=(unsigned long)-1);

  if (hunk_type==HUNK_NAME) /* get name */
    {
      if (!get_long (abfd, &tmp))
	return FALSE;

      len = HUNK_VALUE (tmp) << 2;
      if (len != 0)
	{
	  current_name = bfd_alloc (abfd, len+1);
	  if (!current_name)
	    return FALSE;

	  if (bfd_bread (current_name, len, abfd) != len)
	    return FALSE;

	  current_name[len] = '\0';
	  if (current_name[0] == '\0')
	    {
	       bfd_release (abfd, current_name);
	       current_name = NULL;
	    }
	}

      if (!get_long (abfd, &hunk_type))
	return FALSE;
    }

  /* file_pointer is now after hunk_type */
  secflags = 0;
  switch (hunk_type)
    {
    case HUNK_CODE:
    case HUNK_PPC_CODE:
      secflags = SEC_ALLOC | SEC_LOAD | SEC_CODE | SEC_HAS_CONTENTS;
      sec_name = ".text";
      goto do_section;

    case HUNK_DATA:
      secflags = SEC_ALLOC | SEC_LOAD | SEC_DATA | SEC_HAS_CONTENTS;
      sec_name = ".data";
      goto do_section;

    case HUNK_BSS:
      secflags = SEC_ALLOC;
      sec_name = ".bss";

    do_section:
      if (!current_name)
	current_name = sec_name;
      if (!get_long (abfd, &tmp))
	return FALSE;
      len = HUNK_VALUE (tmp) << 2; /* Length of section */
      if (!is_load)
	{
	  hunk_attribute=HUNK_ATTRIBUTE (len);
	  hunk_attribute=(hunk_attribute==HUNK_ATTR_CHIP)?MEMF_CHIP:
			 (hunk_attribute==HUNK_ATTR_FAST)?MEMF_FAST:0;
	}

      /* Make new section */
      current_section = amiga_make_unique_section (abfd, current_name);
      if (!current_section)
	return FALSE;

      current_section->filepos = bfd_tell (abfd);
      /* For a loadfile, the section size in memory comes from the
	 hunk header. The size on disk may be smaller. */
      current_section->_cooked_size = current_section->_raw_size =
	((hunk_size==(unsigned long)-1) ? len : hunk_size);
      current_section->target_index = hunk_number;
      bfd_set_section_flags (abfd, current_section, secflags);

      amiga_per_section(current_section)->disk_size = len; /* size on disk */
      amiga_per_section(current_section)->attribute = hunk_attribute;

      /* skip the contents */
      if ((secflags & SEC_HAS_CONTENTS) && bfd_seek (abfd, len, SEEK_CUR))
	return FALSE;

      if (!amiga_handle_rest (abfd, current_section, is_load))
	return FALSE;
      break;

      /* Currently, there is one debug hunk per executable, instead of one
	 per unit as it would with a "standard" AmigaOS implementation. So
	 the debug hunk is at the same level as code/data/bss.
	 This will change in the future */
    case HUNK_DEBUG:
      /* format of gnu debug hunk is:
	  HUNK_DEBUG
	      N
	    ZMAGIC
	  symtabsize
	  strtabsize
	  symtabdata  [length=symtabsize]
	  strtabdata  [length=strtabsize]
	  [pad bytes]
	  */

      /* read LW length */
      if (!get_long (abfd, &tmp))
	return FALSE;
      len = tmp << 2;
      if (len > 12)
	{
	  char buf[12];
	  if (bfd_bread (buf, sizeof(buf), abfd) != sizeof(buf))
	    return FALSE;
	  if (GL (&buf[0]) == ZMAGIC) /* GNU DEBUG HUNK */
	    {
	      amiga_data_type *amiga_data=AMIGA_DATA(abfd);
	      /* FIXME: we should add the symbols in the debug hunk to symtab... */
	      amiga_data->symtab_size = GL (&buf[4]);
	      amiga_data->stringtab_size = GL (&buf[8]);
	      adata(abfd).sym_filepos = bfd_tell (abfd);
	      adata(abfd).str_filepos = adata(abfd).sym_filepos +
		amiga_data->symtab_size;
	    }
	  len -= sizeof(buf);
	}
      if (bfd_seek (abfd, len, SEEK_CUR))
	return FALSE;
      break;

    default:
      bfd_set_error (bfd_error_wrong_format);
      return FALSE;
      break;
    }/* switch (hunk_type) */

  return TRUE;
}/* Of amiga_handle_cdb_hunk */


/* Handle rest of a hunk
   I.e.: Relocs, EXT, SYMBOLS... */
static bfd_boolean
amiga_handle_rest (abfd, current_section, isload)
     bfd *abfd;
     sec_ptr current_section;
     bfd_boolean isload;
{
  amiga_per_section_type *asect=amiga_per_section(current_section);
  unsigned long hunk_type,relno,type,len,no;
  raw_reloc_type *relp;

  for (relno=0;;)
    {
      if (!get_long (abfd, &hunk_type))
	return FALSE;
      switch (hunk_type)
	{
	case HUNK_END:
	  if (relno)
	    {
	      abfd->flags |= HAS_RELOC;
	      current_section->flags |= SEC_RELOC;
	      current_section->reloc_count = relno;
	    }
	  return TRUE;
	  break;

	case HUNK_DREL32:
	  if (isload)
	    hunk_type = HUNK_RELOC32SHORT;
	case HUNK_ABSRELOC32:
	case HUNK_RELRELOC16:
	case HUNK_RELRELOC8:
	case HUNK_DREL16:
	case HUNK_DREL8:
	case HUNK_RELOC32SHORT:
	  /* count and skip relocs */
	  relp = (raw_reloc_type *) bfd_alloc (abfd, sizeof (*relp));
	  relp->next = asect->relocs;
	  asect->relocs = relp;
	  relp->pos = bfd_tell (abfd) - 4;
	  relp->num = 0;
	  if (hunk_type != HUNK_RELOC32SHORT) {
	    for (;;) {
	      if (!get_long (abfd, &no))
		return FALSE;
	      if (!no)
		break;
	      relp->num += no;
	      if (bfd_seek (abfd, (no+1)<<2, SEEK_CUR))
		return FALSE;
	    }
	  }
	  else {
	    for (;;) {
	      char buf[2];
	      if (bfd_bread (buf, 2, abfd) != 2)
		return FALSE;
	      if (no=GW(buf),!no)
		break;
	      relp->num += no;
	      if (bfd_seek (abfd, (no+1)<<1, SEEK_CUR))
		return FALSE;
	    }
	    if ((bfd_tell (abfd) & 2) && bfd_seek (abfd, 2, SEEK_CUR))
	      return FALSE;
	  }
	  relno += relp->num;
	  break;

	case HUNK_SYMBOL:
	  /* In a unit, we ignore these, since all symbol information
	     comes with HUNK_EXT, in a load file, these are added */
	  if (!isload)
	    {
	      asect->hunk_symbol_pos = bfd_tell (abfd);
	      for (;;) {
		/* size of symbol */
		if (!get_long (abfd, &no))
		  return FALSE;
		if (!no)
		  break;
		/* skip the name */
		if (bfd_seek (abfd, (no+1)<<2, SEEK_CUR))
		  return FALSE;
	      }
	      break;
	    }
	  /* We add these, by falling through... */

	case HUNK_EXT:
	  /* We leave these alone, until they are requested by the user */
	  asect->hunk_ext_pos = bfd_tell (abfd);
	  for (;;)
	    {
	      if (!get_long (abfd, &no))
	        return FALSE;
	      if (!no)
	        break;

	      /* symbol type and length */
	      type = (no>>24) & 0xff;
	      len = no & 0xffffff;

	      /* skip symbol name */
	      if (bfd_seek (abfd, len<<2, SEEK_CUR))
		return FALSE;

	      /* We have symbols */
	      abfd->flags |= HAS_SYMS;
	      abfd->symcount++;

	      switch (type)
		{
		case EXT_SYMB: /* Symbol hunks are relative to hunk start... */
		case EXT_DEF: /* def relative to hunk */
		case EXT_ABS: /* def absolute */
		  /* skip the value */
		  if (!get_long (abfd, &no))
		    return FALSE;
		  break;

		case EXT_ABSCOMMON: /* Common ref/def */
		case EXT_DEXT32COMMON:
		case EXT_DEXT16COMMON:
		case EXT_DEXT8COMMON:
		  /* FIXME: skip the size of common block */
		  if (!get_long (abfd, &no))
		    return FALSE;

		  /* Fall through */

		case EXT_ABSREF32: /* 32 bit ref */
		case EXT_RELREF16: /* 16 bit ref */
		case EXT_RELREF8: /* 8 bit ref */
		case EXT_DEXT32: /* 32 bit baserel */
		case EXT_DEXT16: /* 16 bit baserel */
		case EXT_DEXT8: /* 8 bit baserel */
		case EXT_RELREF32:
		case EXT_RELREF26:
		  if (!get_long (abfd, &no))
		    return FALSE;
		  if (no)
		    {
		      relno += no;
		      /* skip references */
		      if (bfd_seek (abfd, no<<2, SEEK_CUR))
			return FALSE;
		    }
		  break;

		default: /* error */
		  bfd_msg ("unexpected type %ld(0x%lx) in hunk_ext2 at offset 0x%lx",
			   type, type, bfd_tell (abfd));
		  bfd_set_error (bfd_error_wrong_format);
		  return FALSE;
		  break;
		}/* of switch type */
	    }
	  break;

	case HUNK_DEBUG:
	  /* If a debug hunk is found at this position, the file has
	     been generated by a third party tool and the debug info
	     here are useless to us. Just skip the hunk, then. */
	  if (!get_long (abfd, &no) || bfd_seek (abfd, no<<2, SEEK_CUR))
	    return FALSE;
	  break;

	default: /* error */
	  bfd_seek (abfd, -4, SEEK_CUR);
	  bfd_msg ("missing HUNK_END: unexpected hunktype %ld(0x%lx) at offset 0x%lx",
		   hunk_type, hunk_type, bfd_tell (abfd));
	  hunk_type = HUNK_VALUE(hunk_type);
	  if (hunk_type == HUNK_CODE || hunk_type == HUNK_DATA || hunk_type == HUNK_BSS)
	    return TRUE;
	  bfd_set_error (bfd_error_wrong_format);
	  return FALSE;
	  break;
	}/* Of switch */
    }/* Of for */
  return TRUE;
}/* of amiga_handle_rest */

static bfd_boolean
amiga_mkobject (abfd)
     bfd *abfd;
{
  amiga_data_type *rawptr;
  rawptr = (amiga_data_type *) bfd_zalloc (abfd, sizeof (*rawptr));
  abfd->tdata.amiga_data = rawptr;
  return (rawptr!=NULL);
}

static bfd_boolean
amiga_mkarchive (abfd)
     bfd *abfd;
{
  amiga_ardata_type *ar;
  ar = (amiga_ardata_type *) bfd_zalloc (abfd, sizeof (*ar));
  amiga_ardata (abfd) = ar;
  return (ar!=NULL);
}

/* write nb long words (possibly swapped out) to the output file */
static bfd_boolean
write_longs (in, nb, abfd)
     const unsigned long *in;
     unsigned long nb;
     bfd *abfd;
{
  unsigned char out[10*4];
  unsigned long i;

  while (nb)
    {
      for (i=0; i<nb && i<10; in++,i++)
        bfd_putb32 (in[0], &out[i*4]);
      if (bfd_bwrite ((PTR)out, 4*i, abfd) != 4*i)
	return FALSE;
      nb -= i;
    }
  return TRUE;
}

static long
determine_datadata_relocs (abfd, section)
     bfd *abfd ATTRIBUTE_UNUSED;
     sec_ptr section;
{
  sec_ptr insection;
  asymbol *sym_p;
  unsigned int i;
  long relocs=1;

  for (i=0;i<section->reloc_count;i++)
    {
      arelent *r=section->orelocation[i];
      if (r == NULL)
	continue;
      sym_p=*(r->sym_ptr_ptr); /* The symbol for this relocation */
      insection=sym_p->section;

      /* Is reloc relative to a special section? */
      if (bfd_is_bfd_section(insection))
	continue; /* Nothing to do, since this translates to HUNK_EXT */
      if (insection->output_section == section)
	relocs++;
    }
  return relocs;
}

/* Adjust the indices map when we decide not to output the section <sec> */
static void
remove_section_index (sec, index_map)
     sec_ptr sec;
     int *index_map;
{
  int i=sec->index;
  for (sec=sec->next,index_map[i++]=-1; sec; sec=sec->next)
    (index_map[i++])--;
}

/* Write out the contents of a bfd */
static bfd_boolean
amiga_write_object_contents (abfd)
     bfd *abfd;
{
  long datadata_relocs=0,bss_size=0,idx;
  int *index_map,max_hunk=-1;
  sec_ptr data_sec,p;
  unsigned long i,n[5];

  /* Distinguish UNITS, LOAD Files
    Write out hunks+relocs+HUNK_EXT+HUNK_DEBUG (GNU format) */
  DPRINT(5,("Entering write_object_conts\n"));

  abfd->output_has_begun=TRUE; /* Output has begun */

  index_map = bfd_alloc (abfd, abfd->section_count * sizeof (int));
  if (!index_map)
    return FALSE;

  for (idx=0, p=abfd->sections; p!=NULL; p=p->next)
    index_map[idx++] = p->index;

  /* Distinguish Load files and Unit files */
  if (AMIGA_DATA(abfd)->IsLoadFile)
    {
      DPRINT(5,("Writing load file\n"));

      if (amiga_base_relative)
	BFD_ASSERT (abfd->section_count==3);

      /* Write out load file header */
      n[0] = HUNK_HEADER;
      n[1] = n[2] = 0;
      for (p=abfd->sections; p!=NULL; p=p->next) {
	/* For baserel linking, don't remove empty sections, since they
	   may get some contents later on */
	if ((amiga_base_relative || p->_raw_size!=0 || p->_cooked_size!=0) &&
	    !(amiga_base_relative && !strcmp (p->name, ".bss")))
	  n[2]++;
	else
	  remove_section_index (p, index_map);
      }
      n[3] = 0;
      n[4] = n[2]-1;
      if (!write_longs (n, 5, abfd))
	return FALSE;

      /* Write out sizes and memory specifiers... */
      /* We have to traverse the section list again, bad but no other way... */
      if (amiga_base_relative) {
	for (p=abfd->sections; p!=NULL; p=p->next)
	  {
	    if (amiga_resident && !strcmp(p->name,".data"))
	      {
		datadata_relocs = determine_datadata_relocs (abfd, p);
		data_sec = p;
	      }
	    else if (!strcmp(p->name,".bss"))
	      {
		/* Get size for header */
		bss_size = p->_raw_size;
	      }
	  }
      }

      for (p=abfd->sections; p!=NULL; p=p->next)
	{
	  long extra = 0, i;

	  if (index_map[p->index] < 0)
	    continue;

	  if (datadata_relocs && !strcmp(p->name,".text"))
	    extra = datadata_relocs * 4;
	  else if (bss_size && !strcmp (p->name, ".data"))
	    extra = bss_size;
	  /* convert to a size in long words */
	  n[0] = LONGSIZE (p->_raw_size + extra);

	  i = amiga_per_section(p)->attribute;
	  switch (i)
	    {
	    case MEMF_CHIP:
	      n[0]|=HUNKF_CHIP;
	      i=1;
	      break;
	    case MEMF_FAST:
	      n[0]|=HUNKF_FAST;
	      i=1;
	      break;
	    case 0: /* nothing */
	      i=1;
	      break;
	    default: /* special one */
	      n[0]|=0xc0000000;
	      n[1]=i;
	      i=2;
	      break;
	    }/* Of switch */

	  if (!write_longs (n, i, abfd))
	    return FALSE;
	}/* Of for */
    }
  else
    { /* Unit, no base-relative linking here.. */
      DPRINT(5,("Writing unit\n"));

      /* Write out unit header */
      n[0]=HUNK_UNIT;
      if (!write_longs (n, 1, abfd) || !write_name (abfd, abfd->filename, 0))
	return FALSE;

      for (i=0;i<bfd_get_symcount (abfd);i++) {
	asymbol *sym_p=abfd->outsymbols[i];
	sec_ptr osection=sym_p->section;
	if (!osection || !bfd_is_com_section(osection->output_section))
	  continue;
	for (p=abfd->sections; p!=NULL; p=p->next) {
	  if (!strcmp(p->name, ".bss")) {
	    if (!p->_raw_size && !p->_cooked_size)
	      p->_cooked_size = sym_p->value;
	    break;
	  }
	}
	break;
      }

      for (p=abfd->sections; p!=NULL; p=p->next) {
	if (p->_raw_size==0 && p->_cooked_size==0)
	  remove_section_index (p, index_map);
      }
    }

  /* Compute the maximum hunk number of the ouput file */
  for (p=abfd->sections; p!=NULL; p=p->next)
    max_hunk++;

  /* Write out every section */
  for (p=abfd->sections; p!=NULL; p=p->next)
    {
      if (index_map[p->index] < 0)
	continue;

#define ddrels (datadata_relocs&&!strcmp(p->name,".text")?datadata_relocs:0)
      if (!amiga_write_section_contents (abfd,p,data_sec,ddrels,index_map,
					 max_hunk))
	return FALSE;

      if (!amiga_write_symbols (abfd,p)) /* Write out symbols + HUNK_END */
	return FALSE;
    }/* of for sections */

  /* Write out debug hunk, if requested */
  if (AMIGA_DATA(abfd)->IsLoadFile && write_debug_hunk)
    {
      extern bfd_boolean
	translate_to_native_sym_flags (bfd*, asymbol*, struct external_nlist*);

      unsigned int offset = 4, symbols = 0, i;
      unsigned long str_size = 4; /* the first 4 bytes will be replaced with the length */
      asymbol *sym;
      sec_ptr s;

      /* We have to convert all the symbols in abfd to a.out style... */
      if (bfd_get_symcount (abfd))
	{
#define CAN_WRITE_OUTSYM(sym) (sym!=NULL && sym->section && \
				((sym->section->owner && \
				 bfd_get_flavour (sym->section->owner) == \
				 bfd_target_aout_flavour) || \
				 bfd_asymbol_flavour (sym) == \
				 bfd_target_aout_flavour))

	  for (i = 0; i < bfd_get_symcount (abfd); i++)
	    {
	      sym = abfd->outsymbols[i];
	      /* NULL entries have been written already... */
	      if (CAN_WRITE_OUTSYM (sym))
	        {
		  str_size += strlen(sym->name) + 1;
		  symbols++;
		}
	    }

	  if (!symbols)
	    return TRUE;

	  /* Now, set the .text, .data and .bss fields in the tdata struct
	     because translate_to_native_sym_flags needs them... */
	  for (i=0,s=abfd->sections;s!=NULL;s=s->next)
	    if (!strcmp(s->name,".text"))
	      {
		i|=1;
		adata(abfd).textsec=s;
	      }
	    else if (!strcmp(s->name,".data"))
	      {
	        i|=2;
	        adata(abfd).datasec=s;
	      }
	    else if (!strcmp(s->name,".bss"))
	      {
	        i|=4;
	        adata(abfd).bsssec=s;
	      }

	  if (i!=7) /* section(s) missing... */
	    {
	      bfd_msg ("Missing section, debughunk not written");
	      return TRUE;
	    }

	  /* Write out HUNK_DEBUG, size, ZMAGIC, ... */
	  n[0] = HUNK_DEBUG;
	  n[1] = 3 + ((symbols * sizeof(struct internal_nlist) + str_size + 3) >> 2);
	  n[2] = ZMAGIC; /* Magic number */
	  n[3] = symbols * sizeof(struct internal_nlist);
	  n[4] = str_size;
	  if (!write_longs (n, 5, abfd))
	    return FALSE;

	  /* Write out symbols */
	  for (i = 0; i < bfd_get_symcount (abfd); i++) /* Translate every symbol */
	    {
	      sym = abfd->outsymbols[i];
	      if (CAN_WRITE_OUTSYM (sym))
		{
		  amiga_symbol_type *t = (amiga_symbol_type *) sym;
		  struct external_nlist data;

		  bfd_h_put_16(abfd, t->desc, data.e_desc);
		  bfd_h_put_8(abfd, t->other, data.e_other);
		  bfd_h_put_8(abfd, t->type, data.e_type);
		  if (!translate_to_native_sym_flags(abfd,sym,&data))
		    {
		      bfd_msg ("Cannot translate flags for %s", sym->name);
		    }
		  bfd_h_put_32(abfd, offset, &data.e_strx[0]); /* Store index */
		  offset += strlen(sym->name) + 1;
		  if (bfd_bwrite ((PTR)&data, sizeof(data), abfd)
		      != sizeof(data))
		    return FALSE;
		}
	    }

	  /* Write out strings */
	  if (!write_longs (&str_size, 1, abfd))
	    return FALSE;

	  for (i = 0; i < bfd_get_symcount (abfd); i++)
	    {
	      sym = abfd->outsymbols[i];
	      if (CAN_WRITE_OUTSYM (sym))
		{
		  size_t len = strlen(sym->name) + 1;

	          /* Write string tab */
	          if (bfd_bwrite (sym->name, len, abfd) != len)
	            return FALSE;
		}
	    }

	  /* Write padding */
	  n[0] = 0;
	  i = (4 - (str_size & 3)) & 3;
	  if (i && bfd_bwrite ((PTR)n, i, abfd) != i)
	    return FALSE;

	  /* write a HUNK_END here to finish the loadfile, or AmigaOS
	     will refuse to load it */
	  n[0] = HUNK_END;
	  if (!write_longs (n, 1, abfd))
	    return FALSE;
	}/* Of if bfd_get_symcount (abfd) */
    }/* Of write out debug hunk */

  bfd_release (abfd, index_map);
  return TRUE;
}

/* Write a string padded to 4 bytes and preceded by it's length in
   long words ORed with <value> */
static bfd_boolean
write_name (abfd, name, value)
     bfd *abfd;
     const char *name;
     unsigned long value;
{
  unsigned long n[1];
  size_t l;

  l = strlen (name);
  if (AMIGA_DATA(abfd)->IsLoadFile && l > MAX_NAME_SIZE)
    l = MAX_NAME_SIZE;
  n[0] = (LONGSIZE (l) | value);
  if (!write_longs (n, 1, abfd))
    return FALSE;
  if (bfd_bwrite (name, l, abfd) != l)
    return FALSE;
  n[0] = 0;
  l = (4 - (l & 3)) & 3;
  return (l && bfd_bwrite ((PTR)n, l, abfd) != l ? FALSE : TRUE);
}

static bfd_boolean
amiga_write_archive_contents (arch)
     bfd *arch;
{
  struct stat status;
  bfd *object;

  for (object = arch->archive_head; object; object = object->next)
    {
      unsigned long remaining;

      if (bfd_write_p (object))
	{
	  bfd_set_error (bfd_error_invalid_operation);
	  return FALSE;
	}

      if (object->arelt_data != NULL)
	{
	  remaining = arelt_size (object);
	}
      else
	{
	  if (stat (object->filename, &status) != 0)
	    {
	      bfd_set_error (bfd_error_system_call);
	      return FALSE;
	    }
	  remaining = status.st_size;
	}

      if (bfd_seek (object, 0, SEEK_SET))
	return FALSE;

      while (remaining)
	{
	  char buf[DEFAULT_BUFFERSIZE];
	  unsigned long amt = sizeof(buf);
	  if (amt > remaining)
	    amt = remaining;
	  errno = 0;
	  if (bfd_bread (buf, amt, object) != amt)
	    {
	      if (bfd_get_error () != bfd_error_system_call)
		bfd_set_error (bfd_error_malformed_archive);
	      return FALSE;
	    }
	  if (bfd_bwrite (buf, amt, arch) != amt)
	    return FALSE;
	  remaining -= amt;
	}
    }
  return TRUE;
}

static bfd_boolean
amiga_write_armap (arch, elength, map, orl_count, stridx)
     bfd *arch ATTRIBUTE_UNUSED;
     unsigned int elength ATTRIBUTE_UNUSED;
     struct orl *map ATTRIBUTE_UNUSED;
     unsigned int orl_count ATTRIBUTE_UNUSED;
     int stridx ATTRIBUTE_UNUSED;
{
  return TRUE;
}

static int
determine_type (r)
     arelent *r;
{
  switch (r->howto->type)
    {
      case H_ABS8: /* 8 bit absolute */
      case H_PC8:  /* 8 bit pcrel */
	return 2;

      case H_ABS16: /* 16 bit absolute */
      case H_PC16:  /* 16 bit pcrel */
	return 1;

      case H_ABS32: /* 32 bit absolute */
    /*case H_PC32:*//* 32 bit pcrel */
	return 0;

      case H_SD8: /* 8 bit base rel */
	return 5;

      case H_SD16: /* 16 bit base rel */
	return 4;

      case H_SD32: /* 32 bit baserel */
	return 3;

      default: /* Error, can't represent this */
	bfd_set_error (bfd_error_nonrepresentable_section);
	return -1;
    }/* Of switch */
}

#define NB_RELOC_TYPES 6
static const unsigned long reloc_types[NB_RELOC_TYPES] = {
  HUNK_ABSRELOC32, HUNK_RELRELOC16, HUNK_RELRELOC8,
  HUNK_DREL32,     HUNK_DREL16,     HUNK_DREL8
};

/* Write out section contents, including relocs */
static bfd_boolean
amiga_write_section_contents (abfd, section, data_sec, datadata_relocs,
			      index_map, max_hunk)
     bfd *abfd;
     sec_ptr section;
     sec_ptr data_sec;
     unsigned long datadata_relocs;
     int *index_map;
     int max_hunk;
{
  sec_ptr insection;
  asymbol *sym_p;
  arelent *r;
  unsigned long zero=0,disksize,pad,n[2],k,l,s;
  long *reloc_counts,reloc_count=0;
  unsigned char *values;
  int i,j,x,type;

  DPRINT(5,("Entering write_section_contents\n"));

  /* If we are base-relative linking and the section is .bss and abfd
     is a load file, then return */
  if (AMIGA_DATA(abfd)->IsLoadFile)
    {
      if (amiga_base_relative && !strcmp(section->name, ".bss"))
	return TRUE; /* Nothing to do */
    }
  else
    {
      /* WRITE out HUNK_NAME + section name */
      n[0] = HUNK_NAME;
      if (!write_longs (n, 1, abfd) || !write_name (abfd, section->name, 0))
	return FALSE;
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
      bfd_set_error (bfd_error_nonrepresentable_section);
      return FALSE;
#else
      /* FIXME: Just dump everything we don't currently recognize into
	 a DEBUG hunk. */
      n[0] = HUNK_DEBUG;
#endif
    }

  DPRINT(10,("Section type is %lx\n",n[0]));

  /* Get real size in n[1], this may be shorter than the size in the header */
  if (amiga_per_section(section)->disk_size == 0)
    amiga_per_section(section)->disk_size = section->_raw_size;
  disksize = LONGSIZE (amiga_per_section(section)->disk_size) + datadata_relocs;
  n[1] = disksize;

  /* in a load file, we put section attributes only in the header */
  if (!AMIGA_DATA(abfd)->IsLoadFile)
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
	default: /* error, can't represent this */
	  bfd_set_error (bfd_error_nonrepresentable_section);
	  return FALSE;
	  break;
	}
    }/* Of switch */

  if (!write_longs (n, 2, abfd))
      return FALSE;

  DPRINT(5,("Wrote code and size=%lx\n",n[1]));

  /* If a BSS hunk, we're done, else write out section contents */
  if (HUNK_VALUE (n[0]) == HUNK_BSS)
    return TRUE;

  DPRINT(5,("Non bss hunk...\n"));

  /* Traverse through the relocs, sample them in reloc_data, adjust section
     data to get 0 addend
     Then compactify reloc_data
     Set the entry in the section for the reloc to NULL */

  if (disksize != 0)
    BFD_ASSERT ((section->flags & SEC_IN_MEMORY) != 0);

  reloc_counts = (long *) bfd_zalloc (abfd, NB_RELOC_TYPES * (max_hunk+1)
				      * sizeof (long));
  if (!reloc_counts)
    return FALSE;

  DPRINT(5,("Section has %d relocs\n",section->reloc_count));

  for (l = 0; l < section->reloc_count; l++)
    {
      r = section->orelocation[l];
      if (r == NULL)
	continue;

      sym_p = *(r->sym_ptr_ptr); /* The symbol for this relocation */
      insection = sym_p->section;
      DPRINT(5,("Sec for reloc is %lx(%s)\n",insection,insection->name));
      DPRINT(5,("Symbol for this reloc is %lx(%s)\n",sym_p,sym_p->name));
      /* Is reloc relative to a special section? */
      if (bfd_is_bfd_section(insection))
	continue; /* Nothing to do, since this translates to HUNK_EXT */

      r->addend += sym_p->value; /* Add offset of symbol from section start */

      /* Address of reloc has been unchanged since original reloc, or has
	 been adjusted by get_relocated_section_contents. */
      /* For relocs, the vma of the target section is in the data, the
	 addend is -vma of that section =>No need to add vma */
      /* Add in offset */
      r->addend += insection->output_offset;

      /* Determine which hunk to write, and index of target */
      x = index_map[insection->output_section->index];
      if (x<0 || x>max_hunk) {
	bfd_msg ("erroneous relocation to hunk %d/%s from %s",
		 x, insection->output_section->name, insection->name);
	bfd_set_error (bfd_error_nonrepresentable_section);
	return FALSE;
      }

      type = determine_type(r);
      if (type == -1)
	return FALSE;
      if (type >= NB_RELOC_TYPES) {
	bfd_set_error (bfd_error_nonrepresentable_section);
	return FALSE;
      }
      reloc_counts[type+(x*NB_RELOC_TYPES)]++;
      reloc_count++;

      /* There is no error checking with these... */
      DPRINT(5,("reloc address=%lx,addend=%lx\n",r->address,r->addend));
      values = &section->contents[r->address];

      switch (type)
	{
	case 2: case 5: /* adjust byte */
	  x = ((char *)values)[0] + r->addend;
	  values[0] = x & 0xff;
	  break;
	case 1: case 4: /* adjust word */
	  k = values[1] | (values[0] << 8);
	  x = (int)k + r->addend;
	  values[0] = (x & 0xff00) >> 8;
	  values[1] = x & 0xff;
	  break;
	case 0: case 3: /* adjust long */
	  k = values[3] | (values[2] << 8) | (values[1] << 16) |
	    (values[0] << 24);
	  x = (int)k + r->addend;
	  values[3] = x & 0xff;
	  values[2] = (x & 0xff00) >> 8;
	  values[1] = (x & 0xff0000) >> 16;
	  values[0] = ((unsigned int)x & 0xff000000) >> 24;
	  break;
	}/* of switch */

      r->addend = 0;
      DPRINT(5,("Did adjusting\n"));
    }/* of for l */

  DPRINT(5,("Did all relocs\n"));

  /* We applied all the relocs, as far as possible to obtain 0 addend fields */
  /* Write the section contents */
  if (amiga_per_section(section)->disk_size != 0)
    {
      if (bfd_bwrite ((PTR)section->contents,
		      amiga_per_section(section)->disk_size, abfd) !=
	  amiga_per_section(section)->disk_size)
	return FALSE;

      /* pad the section on disk if necessary (to a long boundary) */
      pad = (4 - (amiga_per_section(section)->disk_size & 3)) & 3;
      if (pad && (bfd_bwrite ((PTR)&zero, pad, abfd) != pad))
	return FALSE;
    }

#if 0
  /* write bss data in the data hunk if needed */
  for (; bss_size--;)
    if (!write_longs (&zero, 1, abfd))
      return FALSE;
#endif

  if (datadata_relocs)
    {
      datadata_relocs--;
      if (!write_longs (&datadata_relocs, 1, abfd))
	return FALSE;
      for (s = 0; s < data_sec->reloc_count; s++)
	{
	  r = data_sec->orelocation[s];
	  if (r == NULL)
	    continue;

	  sym_p = *(r->sym_ptr_ptr); /* The symbol for this relocation */
	  insection = sym_p->section;
	  /* Is reloc relative to a special section? */
	  if (bfd_is_bfd_section(insection))
	    continue; /* Nothing to do, since this translates to HUNK_EXT */

	  if (insection->output_section == data_sec)
	    {
	      if (determine_type(r) == 0)
		if (!write_longs (&r->address, 1, abfd))
		  return FALSE;
	    }
	}
    }

  DPRINT(10,("Wrote contents, writing relocs now\n"));

  if (reloc_count > 0) {
    /* Sample every reloc type */
    for (i = 0; i < NB_RELOC_TYPES; i++) {
      int written = FALSE;
      for (j = 0; j <= max_hunk; j++) {
	long relocs;
	while ((relocs = reloc_counts[i+(j*NB_RELOC_TYPES)]) > 0) {

	  if (!written) {
	    if (!write_longs (&reloc_types[i], 1, abfd))
	      return FALSE;
	    written = TRUE;
	  }

	  if (relocs > 0xffff)
	    relocs = 0xffff;

	  n[0] = relocs;
	  n[1] = j;
	  if (!write_longs (n, 2, abfd))
	    return FALSE;

	  reloc_counts[i+(j*NB_RELOC_TYPES)] -= relocs;
	  reloc_count -= relocs;

	  for (k = 0; k < section->reloc_count; k++) {
	    int jj;

	    r = section->orelocation[k];
	    if (r == NULL) /* already written */
	      continue;

	    sym_p = *(r->sym_ptr_ptr); /* The symbol for this relocation */
	    insection = sym_p->section;
	    /* Is reloc relative to a special section? */
	    if (bfd_is_bfd_section(insection))
	      continue; /* Nothing to do, since this translates to HUNK_EXT */
#if 0
	    /* Determine which hunk to write, and index of target */
	    for (jj = 0, sec = abfd->sections; sec; sec = sec->next, jj++) {
	      if (sec == insection->output_section)
		break;
	    }
	    BFD_ASSERT (jj==index_map[insection->output_section->index]);
#else
	    jj=index_map[insection->output_section->index];
#endif
	    if (jj == j && i == determine_type(r)) {
	      section->orelocation[k] = NULL;
	      if (!write_longs (&r->address, 1, abfd))
		return FALSE;
	      if (--relocs == 0)
		break;
	    }
	  }
	}
      }
      /* write a zero to finish the relocs */
      if (written && !write_longs (&zero, 1, abfd))
	return FALSE;
    }
  }

  bfd_release (abfd, reloc_counts);
  DPRINT(5,("Leaving write_section...\n"));
  if (reloc_count > 0) {
    bfd_set_error (bfd_error_nonrepresentable_section);
    return FALSE;
  }
  return TRUE;
}


/* Write out symbol information, including HUNK_EXT, DEFS, ABS.
   In the case, we were linking base relative, the symbols of the .bss
   hunk have been converted already to belong to the .data hunk */

static bfd_boolean
amiga_write_symbols (abfd, section)
     bfd *abfd;
     sec_ptr section;
{
  sec_ptr osection;
  asymbol *sym_p;
  arelent *r;
  unsigned long n[3],symbol_header,type;
  unsigned int i,j,idx,ncnt,symbol_count;

  /* If base rel linking and section is .bss ==> exit */
  if (amiga_base_relative && !strcmp(section->name,".bss"))
    return TRUE;

  if (section->reloc_count==0 && bfd_get_symcount (abfd)==0)
    {/* Write HUNK_END */
    alldone:
      DPRINT(5,("Leaving write_symbols\n"));
      n[0]=HUNK_END;
      return write_longs (n, 1, abfd);
    }

  /* If this is Loadfile, then do not write HUNK_EXT, but rather HUNK_SYMBOL */
  symbol_header = AMIGA_DATA(abfd)->IsLoadFile ? HUNK_SYMBOL : HUNK_EXT;

  /* Write out all the symbol definitions, then HUNK_END

     Now, first traverse the relocs, all entries that are non NULL
     have to be taken into account */
  symbol_count = 0;

  DPRINT(10,("Traversing relocation table\n"));
  for (i=0;i<section->reloc_count;i++)
    {
      r=section->orelocation[i];
      if (r==NULL)
	continue;

      sym_p=*(r->sym_ptr_ptr); /* The symbol for this relocation */
      osection=sym_p->section; /* The section the symbol belongs to */
      /* this section MUST be a special section */

      DPRINT(5,("Symbol is %s, section is %lx(%s)\n",sym_p->name,osection,osection->name));

      /* group together relocations referring to the same symbol and howto */
      for(idx=i,j=i+1;j<section->reloc_count;j++)
	{
	  arelent *rj=section->orelocation[j];
	  if (rj==NULL || sym_p!=*(rj->sym_ptr_ptr) || r->howto!=rj->howto)
	    continue; /* no match */
	  if (++i == j)
	    continue; /* adjacent */
	  section->orelocation[j] = section->orelocation[i];
	  section->orelocation[i] = rj;
	}

      if ((symbol_count++)==0) /* First write out the HUNK_EXT */
	{
	  if (!write_longs (&symbol_header, 1, abfd))
	    return FALSE;
	}

      if (!bfd_is_com_section(osection)) /* Not common symbol */
	{
	  DPRINT(5,("Non common ref\n"));
	  /* Determine type of ref */
	  switch (r->howto->type)
	    {
	    case H_ABS8:
	    case H_PC8:
	      type=EXT_RELREF8;
	      break;

	    case H_ABS16:
	    case H_PC16:
	      type=EXT_RELREF16;
	      break;

	    case H_ABS32:
	      type=EXT_ABSREF32;
	      break;

	    case H_PC32:
	      type=EXT_RELREF32;
	      break;

	    case H_SD8:
	      type=EXT_DEXT8;
	      break;

	    case H_SD16:
	      type=EXT_DEXT16;
	      break;

	    case H_SD32:
	      type=EXT_DEXT32;
	      break;

	    case H_PC26:
	      type=EXT_RELREF26;
	      break;

	    default: /* Error, can't represent this */
	      bfd_msg ("unexpected reloc %d(%s) at offset 0x%lx",
		       r->howto->type, r->howto->name, bfd_tell (abfd));
	      bfd_set_error (bfd_error_nonrepresentable_section);
	      return FALSE;
	      break;
	    }/* Of switch */
	  ncnt=0;
	}/* Of is ref to undefined or abs symbol */
      else /* ref to common symbol */
	{
	  DPRINT(5,("Common ref\n"));
	  switch (r->howto->type)
	    {
	    default:
	      bfd_msg ("Warning: bad reloc %s for common symbol %s",
		       r->howto->name, sym_p->name);
	    case H_ABS32:
	      type=EXT_ABSCOMMON;
	      break;

	    case H_SD8:
	      type=EXT_DEXT8COMMON;
	      break;

	    case H_SD16:
	      type=EXT_DEXT16COMMON;
	      break;

	    case H_SD32:
	      type=EXT_DEXT32COMMON;
	      break;
	    }/* Of switch */
	  n[0]=sym_p->value; /* Size of common block */
	  ncnt=1;
	}/* Of is common section */

	DPRINT(5,("Type is %lx\n",type));
	if (!write_name (abfd, sym_p->name, type << 24))
	  return FALSE;
	n[ncnt]=i-idx+1; /* refs for symbol... */
	if (!write_longs (n, ncnt+1, abfd))
	  return FALSE;
	for(;idx<=i;++idx)
	  {
	    n[0]=section->orelocation[idx]->address;
	    if (!write_longs (n, 1, abfd))
	      return FALSE;
	  }
    }/* Of traverse relocs */

  /* Now traverse the symbol table and write out all definitions, that are relative
     to this hunk.
     Absolute defs are always only written out with the first hunk.
     Don't write out:
	local symbols
	undefined symbols
	indirect symbols
	warning symbols
	debugging symbols
	warning symbols
	constructor symbols
     since they are unrepresentable in HUNK format.. */

  DPRINT(10,("Traversing symbol table\n"));
  for (i=0;i<bfd_get_symcount (abfd);i++)
    {
      sym_p=abfd->outsymbols[i];
      osection=sym_p->section;

      DPRINT(5,("%d: symbol(%s), osec=%lx(%s)\n",
	i,sym_p->name,osection,osection?osection->name:"null"));

      if (osection==NULL) /* FIXME: Happens with constructor functions. */
	continue;

      if (bfd_is_und_section(osection)
	/*||bfd_is_com_section(osection)*/
	  ||bfd_is_ind_section(osection))
	continue; /* Don't write these */

      /* Only write abs defs, if not writing a Loadfile */
      if (bfd_is_abs_section(osection)&&(section->index==0)&&
	  !AMIGA_DATA(abfd)->IsLoadFile)
	{
	  DPRINT(5,("Abs symbol\n"));
	  /* don't write debug symbols, they will be written in a
	     HUNK_DEBUG later on */
	  if (sym_p->flags & BSF_DEBUGGING)
	    continue;

	  if ((symbol_count++)==0) /* First write out the HUNK_EXT */
	    {
	      if (!write_longs (&symbol_header, 1, abfd))
		return FALSE;
	    }

	  if (!write_name (abfd, sym_p->name, EXT_ABS << 24))
	    return FALSE;
	  n[0]=sym_p->value;
	  if (!write_longs (n, 1, abfd))
	    return FALSE;
	  continue;
	}/* Of abs def */
      if (bfd_is_abs_section(osection))
	continue; /* Not first hunk, already written */

      /* If it is a warning symbol, or a constructor symbol or a
	 debugging or a local symbol, don't write it */
      if (sym_p->flags & (BSF_WARNING|BSF_CONSTRUCTOR|BSF_DEBUGGING|BSF_LOCAL))
	continue;
      if ((sym_p->flags & BSF_GLOBAL) == 0)
	continue;

      /* Now, if osection==section, write it out */
      if (osection->output_section==section)
	{
	  DPRINT(5,("Writing it out\n"));

	  if ((symbol_count++)==0) /* First write out the header */
	    {
	      if (!write_longs (&symbol_header, 1, abfd))
		return FALSE;
	    }

	  type = symbol_header == HUNK_EXT ? EXT_DEF << 24 : 0;
	  if (!write_name (abfd, sym_p->name, type))
	    return FALSE;
	  n[0] = sym_p->value + sym_p->section->output_offset;
	  if (!write_longs (n, 1, abfd))
	    return FALSE;
	}
      else
	{
	  /* write common definitions as bss common references */
	  if (bfd_is_com_section(osection->output_section) &&
	      section->index == 2)
	    {
	      if ((symbol_count++)==0) /* First write out the header */
		{
		  if (!write_longs (&symbol_header, 1, abfd))
		    return FALSE;
		}

	      if (!write_name (abfd, sym_p->name, EXT_ABSCOMMON << 24))
		return FALSE;
	      n[0]=sym_p->value;
	      n[1]=0;
	      if (!write_longs (n, 2, abfd))
		return FALSE;
	    }
	}
    }/* Of for */

  DPRINT(10,("Did traversing\n"));
  if (symbol_count) /* terminate HUNK_EXT, HUNK_SYMBOL */
    {
      n[0]=0;
      if (!write_longs (n, 1, abfd))
	return FALSE;
    }
  DPRINT(5,("Leaving\n"));
  goto alldone; /* Write HUNK_END, return */
}

static bfd_boolean
amiga_get_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  unsigned long disk_size=amiga_per_section(section)->disk_size;

  if (bfd_seek (abfd, section->filepos + offset, SEEK_SET))
    return FALSE;

  if (offset+count > disk_size) {
    /* the section's size on disk may be smaller than in memory
       in this case, pad the contents */
    if (bfd_bread (location, disk_size-offset, abfd) != disk_size-offset)
      return FALSE;
    memset ((char *) location + disk_size - offset, 0, count-(disk_size-offset));
  }
  else {
    if (bfd_bread (location, count, abfd) != count)
      return FALSE;
  }
  return TRUE;
}

static bfd_boolean
amiga_new_section_hook (abfd, newsect)
     bfd *abfd;
     sec_ptr newsect;
{
  newsect->used_by_bfd = (PTR) bfd_zalloc (abfd,
					   sizeof (amiga_per_section_type));
  newsect->alignment_power = 2;
  if (!strcmp (newsect->name, ".data_chip")
      || !strcmp (newsect->name, ".bss_chip"))
    amiga_per_section(newsect)->attribute |= MEMF_CHIP;
  return TRUE;
}

static bfd_boolean
amiga_slurp_symbol_table (abfd)
     bfd *abfd;
{
  amiga_data_type *amiga_data=AMIGA_DATA(abfd);
  amiga_symbol_type *asp;
  unsigned long l,len,type;
  sec_ptr section;

  if (amiga_data->symbols)
    return TRUE; /* already read */

  if (!bfd_get_symcount (abfd))
    return TRUE;

  asp = (amiga_symbol_type *) bfd_zalloc (abfd, sizeof (amiga_symbol_type) *
					  bfd_get_symcount (abfd));
  if ((amiga_data->symbols = asp) == NULL)
    return FALSE;

  /* Symbols are associated with every section */
  for (section=abfd->sections; section!=NULL; section=section->next)
    {
      amiga_per_section_type *asect=amiga_per_section(section);

      if (asect->hunk_ext_pos == 0)
	continue;

      if (bfd_seek (abfd, asect->hunk_ext_pos, SEEK_SET))
	return FALSE;

      for (asect->amiga_symbols=asp; get_long (abfd, &l) && l; asp++)
	{
	  type = l>>24;	/* type of entry */
	  len = (l & 0xffffff) << 2; /* namelength */

	  /* read the name */
	  if ((asp->symbol.name = bfd_alloc (abfd, len+1))==NULL)
	    return FALSE;
	  if (bfd_bread ((PTR)asp->symbol.name, len, abfd) != len)
	    return FALSE;
	  ((char *)asp->symbol.name)[len] = '\0';

	  asp->symbol.the_bfd = abfd;
	  asp->symbol.flags = BSF_GLOBAL;
	  /*asp->desc = 0;
	  asp->other = 0;*/
	  asp->type = type;
	  asp->index = asp - amiga_data->symbols;

	  switch (type) {
	  case EXT_ABSCOMMON: /* Common reference/definition */
	  case EXT_DEXT32COMMON:
	  case EXT_DEXT16COMMON:
	  case EXT_DEXT8COMMON:
	    asp->symbol.section = bfd_com_section_ptr;
	    /* size of common block -> symbol's value */
	    if (!get_long (abfd, &l))
	      return FALSE;
	    asp->symbol.value = l;
	    /* skip refs */
	    if (!get_long (abfd, &l) || bfd_seek (abfd, l<<2, SEEK_CUR))
	      return FALSE;
	    asp->refnum = l;
	    break;
	  case EXT_ABS: /* Absolute */
	    asp->symbol.section = bfd_abs_section_ptr;
	    goto rval;
	    break;
	  case EXT_DEF: /* Relative Definition */
	  case EXT_SYMB: /* Same as EXT_DEF for load files */
	    asp->symbol.section = section;
	  rval:
	    /* read the value */
	    if (!get_long (abfd, &l))
	      return FALSE;
	    asp->symbol.value = l;
	    break;
	  default: /* References to an undefined symbol */
	    asp->symbol.section = bfd_und_section_ptr;
	    asp->symbol.flags = 0;
	    /* skip refs */
	    if (!get_long (abfd, &l) || bfd_seek (abfd, l<<2, SEEK_CUR))
	      return FALSE;
	    asp->refnum = l;
	    break;
	  }
	}
    }
  return TRUE;
}


/* Get size of symtab */
static long
amiga_get_symtab_upper_bound (abfd)
     bfd *abfd;
{
  if (!amiga_slurp_symbol_table (abfd))
    return -1;
  return (bfd_get_symcount (abfd)+1) * (sizeof (amiga_symbol_type *));
}


static long
amiga_get_symtab (abfd, location)
     bfd *abfd;
     asymbol **location;
{
  if(!amiga_slurp_symbol_table (abfd))
    return -1;
  if (bfd_get_symcount (abfd))
    {
      amiga_symbol_type *symp=AMIGA_DATA(abfd)->symbols;
      unsigned int i;
      for (i = 0; i < bfd_get_symcount (abfd); i++, symp++)
	*location++ = &symp->symbol;
      *location = 0;
    }
  return bfd_get_symcount (abfd);
}


static asymbol *
amiga_make_empty_symbol (abfd)
     bfd *abfd;
{
  amiga_symbol_type *new =
    (amiga_symbol_type *) bfd_zalloc (abfd, sizeof (amiga_symbol_type));
  new->symbol.the_bfd = abfd;
  return &new->symbol;
}


static void
amiga_get_symbol_info (ignore_abfd, symbol, ret)
     bfd *ignore_abfd ATTRIBUTE_UNUSED;
     asymbol *symbol;
     symbol_info *ret;
{
  bfd_symbol_info (symbol, ret);
  if (symbol->name[0] == ' ')
    ret->name = "* empty table entry ";
  if (bfd_is_abs_section(symbol->section))
    ret->type = (symbol->flags & BSF_LOCAL) ? 'a' : 'A';
}


static void
amiga_print_symbol (abfd, afile,  symbol, how)
     bfd *abfd;
     PTR afile;
     asymbol *symbol;
     bfd_print_symbol_type how;
{
  FILE *file = (FILE *)afile;

  switch (how) {
  case bfd_print_symbol_name:
    fprintf (file, "%s", symbol->name);
    break;
  case bfd_print_symbol_more:
    fprintf (file, "%4lx %2x",
	     amiga_symbol(symbol)->refnum,
	     (unsigned int)amiga_symbol(symbol)->type);
    break;
  case bfd_print_symbol_all:
    if (symbol->name[0] == ' ')
      {
	fprintf (file, "* empty table entry ");
      }
    else
      {
	bfd_print_symbol_vandf (abfd, (PTR)file, symbol);
	fprintf (file, " %-10s %04lx %02x %s",
		 symbol->section->name,
		 amiga_symbol(symbol)->refnum,
		 (unsigned int)amiga_symbol(symbol)->type,
		 symbol->name);
      }
    break;
  }
}


static long
amiga_get_reloc_upper_bound (abfd, asect)
     bfd *abfd ATTRIBUTE_UNUSED;
     sec_ptr asect;
{
  return (asect->reloc_count + 1) * sizeof (arelent *);
}


static bfd_boolean
read_raw_relocs (abfd, section, d_offset, count)
     bfd *abfd;
     sec_ptr section;
     unsigned long d_offset;	/* offset in the bfd */
     unsigned long count;	/* number of relocs */
{
  unsigned long hunk_number,offset,type,no,j;
  reloc_howto_type *howto;

  if (bfd_seek (abfd, d_offset, SEEK_SET))
    return FALSE;
  while ((long)count > 0)
    {
      /* first determine type of reloc */
      if (!get_long (abfd, &type))
	return FALSE;

      if (type==HUNK_DREL32 && AMIGA_DATA(abfd)->IsLoadFile)
	type = HUNK_RELOC32SHORT;

      switch (type)
	{
	case HUNK_RELOC32SHORT:
	  /* read reloc count, hunk number and offsets */
	  for (howto=&howto_table[R_ABS32SHORT];;) {
	    char buf[2];
	    if (bfd_bread (buf, 2, abfd) != 2)
	      return FALSE;
	    if (no=GW(buf),!no)
	      break;
	    count -= no;
	    if (bfd_bread (buf, 2, abfd) != 2)
	      return FALSE;
	    hunk_number = GW (buf);
	    /* add relocs */
	    for (j=0; j<no; j++) {
	      if (bfd_bread (buf, 2, abfd) != 2)
		return FALSE;
	      offset = GW (buf);
	      if (!amiga_add_reloc (abfd, section, offset, NULL, howto,
				    hunk_number))
	        return FALSE;
	    }
	  }
	  break;

	case HUNK_DREL32: /* 32 bit baserel */
	case HUNK_DREL16: /* 16 bit baserel */
	case HUNK_DREL8: /* 8 bit baserel */
	  type -= 8;
	case HUNK_ABSRELOC32: /* 32 bit ref */
	case HUNK_RELRELOC16: /* 16 bit ref */
	case HUNK_RELRELOC8: /* 8 bit ref */
	  for (howto=&howto_table[R_ABS32+type-HUNK_ABSRELOC32];;) {
	    /* read offsets and hunk number */
	    if (!get_long (abfd, &no))
	      return FALSE;
	    if (!no)
	      break;
	    count -= no;
	    if (!get_long (abfd, &hunk_number))
	      return FALSE;
	    /* add relocs */
	    for (j=0; j<no; j++) {
	      if (!get_long (abfd, &offset) ||
		  !amiga_add_reloc (abfd, section, offset, NULL, howto,
				    hunk_number))
		return FALSE;
	    }
	  }
	  break;

	default: /* error */
	  bfd_set_error (bfd_error_wrong_format);
	  return FALSE;
	  break;
	}
    }

  return TRUE;
}


/* slurp in relocs, amiga_digest_file left various pointers for us */
static bfd_boolean
amiga_slurp_relocs (abfd, section, symbols)
     bfd *abfd;
     sec_ptr section;
     asymbol **symbols ATTRIBUTE_UNUSED;
{
  amiga_per_section_type *asect=amiga_per_section(section);
  reloc_howto_type *howto;
  amiga_symbol_type *asp;
  raw_reloc_type *relp;
  unsigned long offset,type,n,i;

  if (section->relocation)
    return TRUE;

  for (relp=asect->relocs; relp!=NULL; relp=relp->next)
    if (relp->num && !read_raw_relocs (abfd, section, relp->pos, relp->num))
      return FALSE;

  /* Now step through the raw_symbols and add all relocs in them */
  if (!AMIGA_DATA(abfd)->symbols && !amiga_slurp_symbol_table (abfd))
    return FALSE;

  if (asect->hunk_ext_pos == 0)
    return TRUE;

  if (bfd_seek (abfd, asect->hunk_ext_pos, SEEK_SET))
    return FALSE;

  for (asp=asect->amiga_symbols; get_long (abfd, &n) && n; asp++)
    {
      type = (n>>24) & 0xff;
      n &= 0xffffff;

      /* skip the name */
      if (bfd_seek (abfd, n<<2, SEEK_CUR))
	return FALSE;

      switch (type)
	{
	case EXT_SYMB:
	case EXT_DEF:
	case EXT_ABS: /* no relocs here */
	  if (bfd_seek (abfd, 4, SEEK_CUR))
	    return FALSE;
	  break;

	  /* same as below, but advance lp by one to skip common size */
	case EXT_DEXT32COMMON:
	case EXT_DEXT16COMMON:
	case EXT_DEXT8COMMON:
	  type -= 75; /* convert to EXT_DEXT */
	case EXT_ABSCOMMON:
	  if (bfd_seek (abfd, 4, SEEK_CUR))
	    return FALSE;
	  /* Fall through */
	default: /* reference to something */
	  /* points to num of refs to hunk */
	  if (!get_long (abfd, &n))
	    return FALSE;
	  /* Add relocs to this section, relative to asp */
	  /* determine howto first */
	  if (type==EXT_ABSCOMMON) /* 32 bit ref */
	    howto=&howto_table[R_ABS32];
	  else if (type==EXT_RELREF32)
	    howto=&howto_table[R_PC32];
	  else if (type==EXT_RELREF26)
	    howto=&howto_table[R_PC26];
	  else
	    {
	      type -= EXT_ABSREF32;
	      if (type)
		type--; /* skip EXT_ABSCOMMON gap */
	      howto=&howto_table[R_ABS32+type];
	    }/* of else */
	  for (i=0;i<n;i++) /* refs follow */
	    {
	      if (!get_long (abfd, &offset))
		return FALSE;
	      if (!amiga_add_reloc (abfd, section, offset, abfd->outsymbols ?
				    (amiga_symbol_type *) abfd->outsymbols[asp->index] : asp,
				    howto, -4))
		return FALSE;
	    }
	  break;
	}/* of switch */
    }
  return TRUE;
}/* Of slurp_relocs */


static long
amiga_canonicalize_reloc (abfd, section, relptr, symbols)
     bfd *abfd;
     sec_ptr section;
     arelent **relptr;
     asymbol **symbols;
{
  amiga_reloc_type *src;

  if (!section->relocation && !amiga_slurp_relocs (abfd, section, symbols))
    return -1;

  for (src = (amiga_reloc_type *)section->relocation; src; src = src->next)
    *relptr++ = &src->relent;
  *relptr = NULL;

  return section->reloc_count;
}


/* Set section contents */
/* We do it the following way:
   If this is a bss section ==> error
   Otherwise, we try to allocate space for this section,
   if this has not already been done
   Then we set the memory area to the contents */
static bfd_boolean
amiga_set_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  if ((section->flags&SEC_HAS_CONTENTS)==0) /* BSS */
    {
      bfd_set_error (bfd_error_no_contents);
      return FALSE;
    }

  if ((section->flags&SEC_IN_MEMORY)==0) /* Not in memory, so alloc space */
    {
      section->contents = (bfd_byte *) bfd_zalloc (abfd, section->_raw_size);
      if (section->contents == NULL)
	return FALSE;
      section->flags |= SEC_IN_MEMORY;
      DPRINT(5,("Allocated %lx bytes at %lx\n",section->_raw_size,section->contents));
    }

  /* Copy mem */
  memmove(&section->contents[offset],location,count);

  return TRUE;
}/* Of set_section_contents */


/* FIXME: Is this everything? */
static bfd_boolean
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
	case bfd_mach_m68000:
	case bfd_mach_m68008:
	case bfd_mach_m68010:
	case bfd_mach_m68020:
	case bfd_mach_m68030:
	case bfd_mach_m68040:
	case bfd_mach_m68060:
	case 0:
	  return TRUE;
	default:
	  break;
	}
    }
  return FALSE;
}

static int
amiga_sizeof_headers (ignore_abfd, ignore)
     bfd *ignore_abfd ATTRIBUTE_UNUSED;
     bfd_boolean ignore ATTRIBUTE_UNUSED;
{
  /* The amiga hunk format doesn't have headers. */
  return 0;
}

/* Provided a BFD, a section and an offset into the section, calculate
   and return the name of the source file and the line nearest to the
   wanted location.  */
bfd_boolean
amiga_find_nearest_line (abfd, section, symbols, offset, filename_ptr,
			 functionname_ptr, line_ptr)
     bfd *abfd ATTRIBUTE_UNUSED;
     sec_ptr section ATTRIBUTE_UNUSED;
     asymbol **symbols ATTRIBUTE_UNUSED;
     bfd_vma offset ATTRIBUTE_UNUSED;
     const char **filename_ptr ATTRIBUTE_UNUSED;
     const char **functionname_ptr ATTRIBUTE_UNUSED;
     unsigned int *line_ptr ATTRIBUTE_UNUSED;
{
  /* FIXME (see aoutx.h, for example) */
  return FALSE;
}

static reloc_howto_type *
amiga_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  DPRINT(5,("reloc: %s (%d)\n",bfd_get_reloc_code_name(code),code));
  switch (code)
    {
    case BFD_RELOC_8_PCREL:    return &howto_table[R_PC8];
    case BFD_RELOC_16_PCREL:   return &howto_table[R_PC16];
    case BFD_RELOC_32_PCREL:   return &howto_table[R_PC32];
    case BFD_RELOC_8:          return &howto_table[R_PC8];
    case BFD_RELOC_16:         return &howto_table[R_PC16];
    case BFD_RELOC_32:         return &howto_table[R_ABS32];
    case BFD_RELOC_8_BASEREL:  return &howto_table[R_SD8];
    case BFD_RELOC_16_BASEREL: return &howto_table[R_SD16];
    case BFD_RELOC_32_BASEREL: return &howto_table[R_SD32];
    case BFD_RELOC_CTOR:       return &howto_table[R_ABS32];
      /* FIXME: everything handled? */
    default:                   return NULL;
    }
}

static bfd_boolean
amiga_bfd_copy_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  if (bfd_get_flavour (ibfd) == bfd_target_amiga_flavour
      && bfd_get_flavour (obfd) == bfd_target_amiga_flavour) {
    AMIGA_DATA(obfd)->IsLoadFile = AMIGA_DATA(ibfd)->IsLoadFile;
  }
  return TRUE;
}

static bfd_boolean
amiga_bfd_copy_private_section_data (ibfd, isec, obfd, osec)
     bfd *ibfd ATTRIBUTE_UNUSED;
     sec_ptr isec;
     bfd *obfd ATTRIBUTE_UNUSED;
     sec_ptr osec;
{
  if (bfd_get_flavour (osec->owner) == bfd_target_amiga_flavour
      && bfd_get_flavour (isec->owner) == bfd_target_amiga_flavour) {
    amiga_per_section(osec)->disk_size = amiga_per_section(isec)->disk_size;
    amiga_per_section(osec)->attribute = amiga_per_section(isec)->attribute;
  }
  return TRUE;
}

/* There is no armap in the amiga libraries, so we fill carsym entries
   one by one after having parsed the whole archive. */
static bfd_boolean
amiga_slurp_armap (abfd)
     bfd *abfd;
{
  struct arch_syms *syms;
  carsym *defsyms,*csym;
  unsigned long symcount;

  /* allocate the carsyms */
  syms = amiga_ardata(abfd)->defsyms;
  symcount = amiga_ardata(abfd)->defsym_count;

  defsyms = (carsym *) bfd_alloc (abfd, sizeof (carsym) * symcount);
  if (!defsyms)
    return FALSE;

  bfd_ardata(abfd)->symdefs = defsyms;
  bfd_ardata(abfd)->symdef_count = symcount;

  for (csym = defsyms; syms; syms = syms->next) {
    unsigned long type, len, n;
    char *symblock;
    if (bfd_seek (abfd, syms->offset, SEEK_SET))
      return FALSE;
    symblock = (char *) bfd_alloc (abfd, syms->size);
    if (!symblock)
      return FALSE;
    if (bfd_bread (symblock, syms->size, abfd) != syms->size)
      return FALSE;
    while (n=GL(symblock),n)
      {
	symblock += 4;
	len = n & 0xffffff;
	type = (n>>24) & 0xff;
	switch (type) {
	case EXT_SYMB:
	case EXT_DEF:
	case EXT_ABS:
	  len <<= 2;
	  csym->name = symblock;
	  csym->name[len] = '\0';
	  csym->file_offset = syms->unit_offset;
	  csym++;
	  symblock += len+4; /* name+value */
	  break;
	case EXT_ABSREF32:
	case EXT_RELREF16:
	case EXT_RELREF8:
	case EXT_DEXT32:
	case EXT_DEXT16:
	case EXT_DEXT8:
	case EXT_RELREF32:
	case EXT_RELREF26:
	  symblock += len<<2;
	  symblock += (1+GL (symblock))<<2;
	  break;
	case EXT_ABSCOMMON:
	case EXT_DEXT32COMMON:
	case EXT_DEXT16COMMON:
	case EXT_DEXT8COMMON:
	  symblock += (len<<2)+4;
	  symblock += (1+GL (symblock))<<2;
	  break;
	default: /* error */
	  bfd_msg ("unexpected type %ld(0x%lx) in hunk_ext3 at offset 0x%lx",
		   type, type, bfd_tell (abfd));
	  return FALSE;
	}
      }
  }
  bfd_has_map (abfd) = TRUE;
  return TRUE;
}

static void
amiga_truncate_arname (abfd, pathname, arhdr)
     bfd *abfd ATTRIBUTE_UNUSED;
     const char *pathname ATTRIBUTE_UNUSED;
     char *arhdr ATTRIBUTE_UNUSED;
{
}

static const struct bfd_target *
amiga_archive_p (abfd)
     bfd *abfd;
{
  struct arch_syms *symbols=NULL;
  struct stat stat_buffer;
  symindex symcount=0;
  int units;

  if (bfd_stat (abfd, &stat_buffer) < 0)
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  if (stat_buffer.st_size != 0)
    {
      /* scan the units */
      if (!parse_archive_units (abfd, &units, stat_buffer.st_size, FALSE,
				&symbols, &symcount))
	{
	  bfd_set_error (bfd_error_wrong_format);
	  return NULL;
	}

      /* if there is only one unit, file suffix is not .a and .lib, we
	 consider it an object, not an archive. Obviously it's not
	 always true but taking objects for archives makes ld fail,
	 so we don't have much of a choice */
      if (units == 1)
	{
	  char *p = strrchr (abfd->filename, '.');
	  if (p == NULL || (strcmp (p, ".a") && strcmp (p, ".lib")))
	    {
	      bfd_set_error (bfd_error_wrong_format);
	      return NULL;
	    }
	}
    }

  if (abfd->arelt_data)
    arelt_size (abfd) = bfd_tell (abfd);

  bfd_seek (abfd, 0, SEEK_SET);
  abfd->arch_info = bfd_scan_arch ("m68k:68000");

  if (amiga_mkarchive (abfd))
    {
      bfd_ardata(abfd)->first_file_filepos = 0;
      amiga_ardata(abfd)->filesize = stat_buffer.st_size;
      amiga_ardata(abfd)->defsyms = symbols;
      amiga_ardata(abfd)->defsym_count = symcount;
      if (amiga_slurp_armap (abfd))
	return abfd->xvec;
    }

  return NULL;
}

static bfd *
amiga_openr_next_archived_file (archive, last_file)
     bfd *archive;
     bfd *last_file;
{
  file_ptr filestart;

  if (!last_file)
    filestart = bfd_ardata (archive)->first_file_filepos;
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
  unsigned long start_pos,len;
  char buf[8],*base,*name;

  start_pos = bfd_tell (abfd);
  if (start_pos >= amiga_ardata(abfd)->filesize) {
    bfd_set_error (bfd_error_no_more_archived_files);
    return NULL;
  }

  /* get unit type and name length in long words */
  if (bfd_bread (buf, sizeof(buf), abfd) != sizeof(buf))
    return NULL;

  if (GL (&buf[0]) != HUNK_UNIT) {
    bfd_set_error (bfd_error_malformed_archive);
    return NULL;
  }

  ared = bfd_zalloc (abfd, sizeof (struct areltdata));
  if (ared == NULL)
    return NULL;

  len = GL (&buf[4]) << 2;

  ared->filename = bfd_alloc (abfd, len+1 > 16 ? len+1 : 16);
  if (ared->filename == NULL)
    return NULL;

  switch (len) {
    default:
      if (bfd_bread (ared->filename, len, abfd) != len)
	return NULL;
      ared->filename[len] = '\0';
      /* strip path part */
      base = strchr (name = ared->filename, ':');
      if (base != NULL)
	name = base + 1;
      for (base = name; *name; ++name)
	if (*name == '/')
	  base = name + 1;
      if (*base != '\0') {
	ared->filename = base;
	break;
      }
      /* Fall through */
    case 0: /* fake a name */
      sprintf (ared->filename, "obj-%08lu.o", ++amiga_ardata(abfd)->outnum);
      break;
  }

  if (bfd_seek (abfd, start_pos+4, SEEK_SET))
    return NULL;

  if (!amiga_read_unit (abfd, amiga_ardata(abfd)->filesize))
    return NULL;

  ared->parsed_size = bfd_tell (abfd) - start_pos;
  if (bfd_seek (abfd, start_pos, SEEK_SET))
    return NULL;

  return (PTR) ared;
}

static int
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
  buf->st_mode = 0644;
  buf->st_uid = 0;
  buf->st_gid = 0;
  buf->st_mtime = 2922*24*60*60;
  buf->st_size = arelt_size (abfd);

  return 0;
}

/* Entry points through BFD_JUMP_TABLE_GENERIC */
#define amiga_close_and_cleanup		_bfd_generic_close_and_cleanup
#define amiga_bfd_free_cached_info	_bfd_generic_bfd_free_cached_info
/* amiga_new_section_hook defined above */
/* amiga_get_section_contents defined above */
#define amiga_get_section_contents_in_window _bfd_generic_get_section_contents_in_window

/* Entry points through BFD_JUMP_TABLE_COPY */
#define amiga_bfd_merge_private_bfd_data _bfd_generic_bfd_merge_private_bfd_data
/*#define amiga_bfd_copy_private_section_data _bfd_generic_bfd_copy_private_section_data*/
#define amiga_bfd_copy_private_symbol_data _bfd_generic_bfd_copy_private_symbol_data
#define amiga_bfd_set_private_flags _bfd_generic_bfd_set_private_flags
#define amiga_bfd_print_private_bfd_data _bfd_generic_bfd_print_private_bfd_data

/* Entry points through BFD_JUMP_TABLE_ARCHIVE */
/*#define amiga_slurp_armap		bfd_slurp_armap*/
#define amiga_slurp_extended_name_table	_bfd_slurp_extended_name_table
#define amiga_construct_extended_name_table _bfd_archive_bsd_construct_extended_name_table
/*#define amiga_truncate_arname		bfd_gnu_truncate_arname*/
/*#define amiga_write_armap		bsd_write_armap*/
/*#define amiga_read_ar_hdr		_bfd_generic_read_ar_hdr*/
/*#define amiga_openr_next_archived_file	bfd_generic_openr_next_archived_file*/
#define amiga_get_elt_at_index		_bfd_generic_get_elt_at_index
/*#define amiga_generic_stat_arch_elt	bfd_generic_stat_arch_elt*/
#define amiga_update_armap_timestamp	_bfd_archive_bsd_update_armap_timestamp

/* Entry points through BFD_JUMP_TABLE_SYMBOLS */
/* amiga_get_symtab_upper_bound defined above */
/* amiga_get_symtab defined above */
/* amiga_make_empty_symbol defined above */
/* amiga_print_symbol defined above */
/* amiga_get_symbol_info defined above */
#define amiga_bfd_is_local_label_name	bfd_generic_is_local_label_name
#define amiga_get_lineno		(alent * (*)(bfd *, asymbol *)) bfd_nullvoidptr
/* amiga_find_nearest_line defined above */
#define amiga_bfd_make_debug_symbol	(asymbol * (*)(bfd *, PTR, unsigned long)) bfd_nullvoidptr
#define amiga_read_minisymbols		_bfd_generic_read_minisymbols
#define amiga_minisymbol_to_symbol	_bfd_generic_minisymbol_to_symbol

/* Entry points through BFD_JUMP_TABLE_LINK
   NOTE: We use a special get_relocated_section_contents both in amiga AND in a.out files.
   In addition, we use an own final_link routine, which is nearly identical to _bfd_generic_final_link */
bfd_byte *
get_relocated_section_contents PARAMS ((bfd *, struct bfd_link_info *,
	struct bfd_link_order *, bfd_byte *, bfd_boolean, asymbol **));
#define amiga_bfd_get_relocated_section_contents get_relocated_section_contents
#define amiga_bfd_relax_section		bfd_generic_relax_section
#define amiga_bfd_link_hash_table_create _bfd_generic_link_hash_table_create
#define amiga_bfd_link_hash_table_free	_bfd_generic_link_hash_table_free
#define amiga_bfd_link_add_symbols	_bfd_generic_link_add_symbols
#define amiga_bfd_link_just_syms	_bfd_generic_link_just_syms
bfd_boolean amiga_final_link PARAMS ((bfd *, struct bfd_link_info *));
#define amiga_bfd_final_link		amiga_final_link
#define amiga_bfd_link_split_section	_bfd_generic_link_split_section
#define amiga_bfd_gc_sections		bfd_generic_gc_sections
#define amiga_bfd_merge_sections	bfd_generic_merge_sections
#define amiga_bfd_discard_group		bfd_generic_discard_group

#if defined (amiga)
#undef amiga /* So that the JUMP_TABLE() macros below can work.  */
#endif

const bfd_target amiga_vec =
{
  "amiga",		/* name */
  bfd_target_amiga_flavour,
  BFD_ENDIAN_BIG,	/* data byte order */
  BFD_ENDIAN_BIG,	/* header byte order */
  HAS_RELOC | EXEC_P | HAS_LINENO | HAS_DEBUG | HAS_SYMS | HAS_LOCALS | WP_TEXT, /* object flags */
  SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_CODE | SEC_DATA, /* section flags */
  '_',			/* symbol leading char */
  ' ',			/* ar_pad_char */
  15,			/* ar_max_namelen (15 for UNIX compatibility) */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* hdrs */
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
  NULL,
  NULL
};
