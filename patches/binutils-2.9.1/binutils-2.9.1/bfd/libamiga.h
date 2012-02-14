/* BFD back-end data structures for AmigaOS.
   Copyright (C) 1992-1994 Free Software Foundation, Inc.
   Contributed by Leonard Norrgard.
   Extended by Stephan Thesing Nov 94

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

#ifdef __STDC__
#define CAT3(a,b,c) a##b##c
#else
#define CAT3(a,b,c) a/**/b/**/c
#endif

#define GET_WORD bfd_h_get_32
#define GET_SWORD (int32_type)GET_WORD
#define PUT_WORD bfd_h_put_32
#define NAME(x,y) CAT3(x,_32_,y)
#define JNAME(x) CAT(x,_32)
#define BYTES_IN_WORD 4

/* Hunk ID numbers.*/  
#define HUNK_UNIT		999
#define HUNK_NAME		1000
#define HUNK_CODE		1001
#define HUNK_DATA		1002
#define HUNK_BSS		1003
#define HUNK_RELOC32		1004
#define HUNK_ABSRELOC32		HUNK_RELOC32
#define HUNK_RELOC16		1005
#define HUNK_RELRELOC16		HUNK_RELOC16
#define HUNK_RELOC8		1006
#define HUNK_RELRELOC8		HUNK_RELOC8
#define HUNK_EXT		1007
#define HUNK_SYMBOL		1008
#define HUNK_DEBUG		1009
#define HUNK_END		1010
#define HUNK_HEADER		1011
#define HUNK_HEADER_POS		(0x00001000 | 1011)
#define HUNK_OVERLAY		1013
#define HUNK_BREAK		1014
#define HUNK_DREL32		1015
#define HUNK_DREL16		1016
#define HUNK_DREL8		1017
#define HUNK_LIB		1018
#define HUNK_INDEX		1019
#define HUNK_RELOC32SHORT	1020
#define HUNK_RELRELOC32		1021
#define HUNK_ABSRELOC16		1022

/* The hunk ID part.  */

#define HUNK_VALUE(hunk_id)	((hunk_id) & 0x3fffffff)

/* Attributes of a hunk.  */

#define HUNK_ATTRIBUTE(hunk_id)	((hunk_id) >> 30)
#define HUNK_ATTR_CHIP 		0x01	/* Hunk contents must go into chip ram.  */
#define HUNK_ATTR_FAST		0x02	/* fast */
#define HUNK_ATTR_FOLLOWS	0x03	/* Mem id follows */

#define EXT_SYMB		0
#define EXT_DEF			1
#define EXT_ABS			2
#define EXT_RES			3
#define EXT_REF32		129
#define EXT_ABSREF32		EXT_REF32
#define EXT_COMMON		130
#define EXT_ABSCOMMON		EXT_COMMON
#define EXT_REF16		131
#define EXT_RELREF16		EXT_REF16
#define EXT_REF8		132
#define EXT_RELREF8		EXT_REF8
#define EXT_DEXT32		133
#define EXT_DREF32		EXT_DEXT32
#define EXT_DEXT16		134
#define EXT_DREF16		EXT_DEXT16
#define EXT_DEXT8		135
#define EXT_DREF8		EXT_DEXT8
#define EXT_RELREF32		136
#define EXT_RELCOMMON		137
#define EXT_ABSREF16		138
#define EXT_ABSREF8		139


typedef struct amiga_reloc {
  arelent relent;
  struct amiga_reloc *next;
  struct amiga_symbol *symbol;
  long target_hunk;
} amiga_reloc_type;

typedef struct amiga_symbol {
  asymbol symbol;
/*  struct amiga_symbol *next;*/
  unsigned short hunk_number;
  long index;
  /* these come from a.out. Not used yet, but needed to compile */
  short desc;
  char other;
  unsigned char type;
} amiga_symbol_type;

struct amiga_raw_symbol {
  struct amiga_raw_symbol *next;
  unsigned long data[1];
};

typedef struct amiga_per_section
{
  amiga_reloc_type *reloc_tail; /* last reloc */ /* first is in section->relocation */
  int attribute; /* Memory type required by this section */
  unsigned long disk_size;	/* Section size on disk. _raw_size may be larger than this */
  int max_raw_relocs; /* Size of array */
  unsigned long int num_raw_relocs8, num_raw_relocs16, num_raw_relocs32;
  unsigned long  raw_relocs8, raw_relocs16, raw_relocs32;
  struct amiga_raw_symbol *first;
  struct amiga_raw_symbol *last; /* tail */

  /* the symbols for this section */
  amiga_symbol_type *amiga_symbols;

  unsigned long hunk_ext_pos;	/* offset of hunk_ext in the bfd file */
  unsigned long hunk_symbol_pos; /* offset of hunk_symbol in the bfd file */
} amiga_per_section_type;
#define amiga_per_section(x) ((amiga_per_section_type *)((x)->used_by_bfd))

/* The `tdata' struct for all a.out-like object file formats.
   Various things depend on this struct being around any time an a.out
   file is being handled.  An example is dbxread.c in GDB.  */

struct amiga_data {
  struct internal_exec *hdr;		/* exec file header */
  amiga_symbol_type *symbols;		/* symtab for input bfd */

  /* Filler, so we can pretend to be an a.out to GDB.  */
  asection *textsec;
  asection *datasec;
  asection *bsssec;
  int nb_hunks;			/* number of hunks in the file */
  /* The positions of the string table and symbol table.  */
  file_ptr sym_filepos;
  file_ptr str_filepos;

  unsigned int n_symbols;               /* number of symbols */

  /* Size of a relocation entry in external form */
  unsigned dummy_reloc_entry_size;

  /* Size of a symbol table entry in external form */
  unsigned symbol_entry_size;

  unsigned exec_bytes_size;
  unsigned vma_adjusted : 1;
};

typedef struct  amiga_data_struct {
  struct amiga_data a;

  unsigned long symtab_size;
  unsigned long stringtab_size;

  unsigned long *first_byte;
  unsigned long *file_end;
  unsigned long *file_pointer;
  amiga_symbol_type *symbols;
  amiga_symbol_type *symbol_tail;
  boolean IsLoadFile; /* If true, this is a load file (for output bfd only) */
  int maxsymbols;     /* Used by final_link routine to add symbols to output bfd.
                         This is the # of entries, allocated in abdfd->osymbols */
  int nb_hunks;
  /* The next two fields are set at final_link time
     for the output bfd only */
  boolean baserel;    /* true if there is ___init_a4 in the global hash table */
  bfd_vma a4init;     /* cache the value for efficiency */
} amiga_data_type;

struct arch_syms {
  unsigned long offset;		/* disk offset in the archive */
  unsigned long size;		/* size of the block of symbols */
  unsigned long unit_offset;	/* start of unit on disk */
  struct arch_syms *next;	/* linked list */
};

#define	adata(bfd)		((bfd)->tdata.amiga_data->a)

/* We take the address of the first element of an asymbol to ensure that the
   macro is only ever applied to an asymbol */
#define amiga_symbol(asymbol) ((amiga_symbol_type *)(&(asymbol)->the_bfd))

#define AMIGA_DATA(abfd) ((abfd)->tdata.amiga_data)

#define HUNKB_ADVISORY		29
#define HUNKB_CHIP		30
#define HUNKB_FAST		31
#define HUNKF_ADVISORY		(1L << HUNKB_ADVISORY)
#define HUNKF_CHIP		(1L << HUNKB_CHIP)
#define HUNKF_FAST		(1L << HUNKB_FAST)

#define MEMF_ANY		(0L)
#define MEMF_PUBLIC		(1L << 0)
#define MEMF_CHIP		(1L << 1)
#define MEMF_FAST		(1L << 2)
#define MEMF_LOCAL		(1L << 8)
#define MEMF_24BITDMA		(1L << 9)
#define	MEMF_KICK		(1L << 10)
#define MEMF_CLEAR		(1L << 16)
#define MEMF_LARGEST		(1L << 17)
#define MEMF_REVERSE		(1L << 18)
#define MEMF_TOTAL		(1L << 19)
#define	MEMF_NO_EXPUNGE		(1L << 31)
