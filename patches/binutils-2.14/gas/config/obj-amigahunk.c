/* AmigaOS object file format
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
02111-1307, USA.  */

#include "as.h"

enum {
  N_UNDF=0,
/*N_ABS=2,*/
/*N_TEXT=4,*/
/*N_DATA=6,*/
/*N_BSS=8,*/
  N_INDR=0xa,
/*N_COMM=0x12,*/
  N_SETA=0x14,
  N_SETT=0x16,
  N_SETD=0x18,
  N_SETB=0x1a,
/*N_SETV=0x1c,*/
  N_WARNING=0x1e,
/*N_FN=0x1f*/
  N_EXT=1,
  N_TYPE=0x1e,
/*N_STAB=0xe0,*/
};

static void obj_amiga_line PARAMS ((int));
static void obj_amiga_weak PARAMS ((int));

const pseudo_typeS obj_pseudo_table[] =
{
  {"line", obj_amiga_line, 0},	/* source code line number */
  {"weak", obj_amiga_weak, 0},	/* mark symbol as weak.  */

  /* other stuff */
  {"ABORT", s_abort, 0},

  {NULL, NULL, 0}		/* end sentinel */
};

#ifdef BFD_ASSEMBLER

void
obj_amiga_frob_symbol (sym, punt)
     symbolS *sym;
     int *punt ATTRIBUTE_UNUSED;
{
  sec_ptr sec = S_GET_SEGMENT (sym);
  unsigned int type = amiga_symbol (symbol_get_bfdsym (sym))->type;

  /* Only frob simple symbols this way right now.  */
  if (! (type & ~ (N_TYPE | N_EXT)))
    {
      if (type == (N_UNDF | N_EXT)
	  && sec == &bfd_abs_section)
	{
	  sec = bfd_und_section_ptr;
	  S_SET_SEGMENT (sym, sec);
	}

      if ((type & N_TYPE) != N_INDR
	  && (type & N_TYPE) != N_SETA
	  && (type & N_TYPE) != N_SETT
	  && (type & N_TYPE) != N_SETD
	  && (type & N_TYPE) != N_SETB
	  && type != N_WARNING
	  && (sec == &bfd_abs_section
	      || sec == &bfd_und_section))
	return;
      if (symbol_get_bfdsym (sym)->flags & BSF_EXPORT)
	type |= N_EXT;

      switch (type & N_TYPE)
	{
	case N_SETA:
	case N_SETT:
	case N_SETD:
	case N_SETB:
	  /* Set the debugging flag for constructor symbols so that
	     BFD leaves them alone.  */
	  symbol_get_bfdsym (sym)->flags |= BSF_DEBUGGING;

	  /* You can't put a common symbol in a set.  The way a set
	     element works is that the symbol has a definition and a
	     name, and the linker adds the definition to the set of
	     that name.  That does not work for a common symbol,
	     because the linker can't tell which common symbol the
	     user means.  FIXME: Using as_bad here may be
	     inappropriate, since the user may want to force a
	     particular type without regard to the semantics of sets;
	     on the other hand, we certainly don't want anybody to be
	     mislead into thinking that their code will work.  */
	  if (S_IS_COMMON (sym))
	    as_bad (_("Attempt to put a common symbol into set %s"),
		    S_GET_NAME (sym));
	  /* Similarly, you can't put an undefined symbol in a set.  */
	  else if (! S_IS_DEFINED (sym))
	    as_bad (_("Attempt to put an undefined symbol into set %s"),
		    S_GET_NAME (sym));

	  break;
	case N_INDR:
	  /* Put indirect symbols in the indirect section.  */
	  S_SET_SEGMENT (sym, bfd_ind_section_ptr);
	  symbol_get_bfdsym (sym)->flags |= BSF_INDIRECT;
	  if (type & N_EXT)
	    {
	      symbol_get_bfdsym (sym)->flags |= BSF_EXPORT;
	      symbol_get_bfdsym (sym)->flags &=~ BSF_LOCAL;
	    }
	  break;
	case N_WARNING:
	  /* Mark warning symbols.  */
	  symbol_get_bfdsym (sym)->flags |= BSF_WARNING;
	  break;
	}
    }
  else
    {
      symbol_get_bfdsym (sym)->flags |= BSF_DEBUGGING;
    }

  amiga_symbol (symbol_get_bfdsym (sym))->type = type;

  /* Double check weak symbols.  */
  if (S_IS_WEAK (sym))
    {
      if (S_IS_COMMON (sym))
	as_bad (_("Symbol `%s' can not be both weak and common"),
		S_GET_NAME (sym));
    }
}

void
obj_amiga_frob_file_before_fix ()
{
  /* Relocation processing may require knowing the VMAs of the sections.
     Since writing to a section will cause the BFD back end to compute the
     VMAs, fake it out here....  */
  bfd_byte b = 0;
  bfd_boolean x = TRUE;
  if (bfd_section_size (stdoutput, text_section) != 0)
    {
      x = bfd_set_section_contents (stdoutput, text_section, &b, (file_ptr) 0,
				    (bfd_size_type) 1);
    }
  else if (bfd_section_size (stdoutput, data_section) != 0)
    {
      x = bfd_set_section_contents (stdoutput, data_section, &b, (file_ptr) 0,
				    (bfd_size_type) 1);
    }
  assert (x);
}

#endif /* BFD_ASSEMBLER */

static void
obj_amiga_line (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  /* Assume delimiter is part of expression.
     BSD4.2 as fails with delightful bug, so we
     are not being incompatible here.  */
  new_logical_line ((char *) NULL, (int) (get_absolute_expression ()));
  demand_empty_rest_of_line ();
}				/* obj_amiga_line() */

/* Handle .weak.  This is a GNU extension.  */

static void
obj_amiga_weak (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  char *name;
  int c;
  symbolS *symbolP;

  do
    {
      name = input_line_pointer;
      c = get_symbol_end ();
      symbolP = symbol_find_or_make (name);
      *input_line_pointer = c;
      SKIP_WHITESPACE ();
      S_SET_WEAK (symbolP);
      if (c == ',')
	{
	  input_line_pointer++;
	  SKIP_WHITESPACE ();
	  if (*input_line_pointer == '\n')
	    c = '\n';
	}
    }
  while (c == ',');
  demand_empty_rest_of_line ();
}
