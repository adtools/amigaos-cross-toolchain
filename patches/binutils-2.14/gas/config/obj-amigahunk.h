/* obj-amigahunk.h, AmigaOS object file format for gas, the assembler.
   Copyright (C) 1992, 1993, 1994, 1995 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2,
   or (at your option) any later version.

   GAS is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
   the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA. */

/* Tag to validate an amiga object file format processing */
#define OBJ_AMIGAHUNK 1

#include "targ-cpu.h"

#ifdef BFD_ASSEMBLER

#include "bfd/libamiga.h"

#define OUTPUT_FLAVOR bfd_target_amiga_flavour

/* SYMBOL TABLE */
/* Symbol table macros and constants */

#define S_SET_OTHER(S,V) (amiga_symbol (symbol_get_bfdsym (S))->other = (V))
#define S_SET_TYPE(S,T)	 (amiga_symbol (symbol_get_bfdsym (S))->type = (T))
#define S_SET_DESC(S,D)	 (amiga_symbol (symbol_get_bfdsym (S))->desc = (D))
#define S_GET_TYPE(S)	 (amiga_symbol (symbol_get_bfdsym (S))->type)

#define obj_frob_symbol(S,PUNT) obj_amiga_frob_symbol (S, &PUNT)
extern void obj_amiga_frob_symbol PARAMS ((symbolS *, int *));

#define obj_frob_file_before_fix() obj_amiga_frob_file_before_fix ()
extern void obj_amiga_frob_file_before_fix PARAMS ((void));

#define obj_sec_sym_ok_for_reloc(SEC)	(1)

#endif /* BFD_ASSEMBLER */

#define obj_read_begin_hook()		{;}
#define obj_symbol_new_hook(s)		{;}
#define EMIT_SECTION_SYMBOLS		(0)

#define AOUT_STABS
