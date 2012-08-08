/* BFD back-end for Commodore-Amiga AmigaOS binaries. Linker routines.
   Copyright (C) 1990-1994 Free Software Foundation, Inc.
   Contributed by Stephan Thesing

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
INODE
amigalink, , implementation, amiga
SECTION
	amigalink

This is the description of the linker routines for the amiga.
In fact, this includes a description of the changes made to the
a.out code, in order to build a working linker for the Amiga.
@menu
@* alterations::
@end menu

INODE 
alterations, , , amigalink
SUBSECTION
	alterations

The file @file{aout-amiga.c} defines the amiga a.out backend. It differs from
the sun3 backend only in these details:
	o The @code{final_link} routine is @code{amiga_final_link}.
	o The routine to get the relocated section contents is
           @code{aout_bfd_get_relocated_section_contents}.

This ensures that the link is performed properly, but has the side effect of loosing
performance.


The amiga bfd code uses the same @code{amiga_final_link} routine, but with a 
different <<get_relocated_section_contents>> entry: <<amiga_bfd_get_relocated_section_contents>>.
The latter  differs from the routine of the a.out backend only in the application of relocs
 to the section contents.
@@*

The usage of a special linker code has one reason:
The bfd library assumes that a program is always loaded at a known memory
address. This is not a case on an Amiga. So the Amiga format has to take over
some relocs to an executable output file. 
This is not the case with a.out formats, so there relocations can be applied at link time,
not at run time, like on the Amiga.
The special routines compensate this: instead of applying the relocations, they are
copied to the output file, if neccessary.
As as consequence, the @code{final_link} and @code{get_relocated_section_contents} are nearly identical to
the original routines from @file{linker.c} and @file{reloc.c}.
*/

#include "bfd.h"
#include "bfdlink.h"
#include "sysdep.h"
#include "genlink.h"
#include "libbfd.h"

#include "libamiga.h"
#undef GET_SWORD
#define aadata ((bfd)->tdata.amiga_data->a)
#undef adata
#include "libaout.h"


#define max(x,y) (((x)<=(y))?(y):(x))

#define DEBUG_AMIGA 10000

#if DEBUG_AMIGA
#include <stdarg.h>
static void
error_print (const char *fmt, ...)
{
  va_list args;

  va_start (args, fmt);
  (void) vfprintf (stderr, fmt, args);
  va_end (args);
}

#define DPRINT(L,x) if (L>=DEBUG_AMIGA) error_print x
#else
#define DPRINT(L,x)
#endif

/* This one is used by the linker and tells us, if a debug hunk should be
   written out*/
int write_debug_hunk = 0;

/* This is also used by the linker to set the attribute of sections */
int amiga_attribute = 0;

/* This one is used to indicate base-relative linking */
int amiga_base_relative = 0;

/* This one is used to indicate -resident linking */
int amiga_resident = 0;

extern boolean default_indirect_link_order
  (bfd *, struct bfd_link_info *, asection *,
   struct bfd_link_order *, boolean);



bfd_byte *get_relocated_section_contents
  (bfd *, struct bfd_link_info *, struct bfd_link_order *, bfd_byte *,
   boolean, asymbol **);
static bfd_reloc_status_type amiga_perform_reloc
  (bfd *, arelent *, PTR, asection *, bfd *, char **);
static bfd_reloc_status_type aout_perform_reloc
  (bfd *, arelent *, PTR, asection *, bfd *, char **);
static boolean amiga_reloc_link_order
  (bfd *, struct bfd_link_info *, asection *, struct bfd_link_order *);


/* This one is nearly identical to bfd_generic_get_relocated_section_contents
   from reloc.c */
bfd_byte *
get_relocated_section_contents (abfd, link_info, link_order, data,
				relocateable, symbols)
     bfd *abfd;
     struct bfd_link_info *link_info;
     struct bfd_link_order *link_order;
     bfd_byte *data;
     boolean relocateable;
     asymbol **symbols;
{
  bfd *input_bfd = link_order->u.indirect.section->owner;
  asection *input_section = link_order->u.indirect.section;

  long reloc_size = bfd_get_reloc_upper_bound (input_bfd, input_section);
  arelent **reloc_vector = NULL;
  long reloc_count;
  bfd_reloc_status_type (*reloc_func) (bfd *, arelent *, PTR, asection *,
				       bfd *, char **);

  DPRINT (5, ("Entering get_rel_sec_cont\n"));

  if (reloc_size < 0)
    goto error_return;

  if (input_bfd->xvec->flavour == bfd_target_amiga_flavour)
    reloc_func = amiga_perform_reloc;
  else if (input_bfd->xvec->flavour == bfd_target_aout_flavour)
    reloc_func = aout_perform_reloc;
  else
    {
      bfd_set_error (bfd_error_bad_value);
      goto error_return;
    }

  reloc_vector = (arelent **) malloc (reloc_size);

  if (reloc_vector == NULL && reloc_size != 0)
    {
      bfd_set_error (bfd_error_no_memory);
      goto error_return;
    }

  DPRINT (5, ("GRSC: GetSecCont()\n"));
  /* read in the section */
  if (!bfd_get_section_contents (input_bfd,
				 input_section,
				 (PTR) data, 0, input_section->_raw_size))
    goto error_return;

  /* We're not relaxing the section, so just copy the size info */
  input_section->_cooked_size = input_section->_raw_size;
  input_section->reloc_done = true;

  DPRINT (5, ("GRSC: CanReloc\n"));
  reloc_count = bfd_canonicalize_reloc (input_bfd,
					input_section, reloc_vector, symbols);
  if (reloc_count < 0)
    goto error_return;

  if (reloc_count > 0)
    {
      arelent **parent;

      DPRINT (5, ("reloc_count=%d\n", reloc_count));

      for (parent = reloc_vector; *parent != (arelent *) NULL; parent++)
	{
	  char *error_message = (char *) NULL;
	  bfd_reloc_status_type r;

	  DPRINT (5, ("Applying a reloc\nparent=%lx, reloc_vector=%lx,"
		      "*parent=%lx\n", parent, reloc_vector, *parent));
	  r = (*reloc_func) (input_bfd,
			     *parent,
			     (PTR) data,
			     input_section,
			     relocateable ? abfd : (bfd *) NULL,
			     &error_message);
	  if (relocateable)
	    {
	      asection *os = input_section->output_section;

	      DPRINT (5, ("Keeping reloc\n"));
	      /* A partial link, so keep the relocs */
	      os->orelocation[os->reloc_count] = *parent;
	      os->reloc_count++;
	    }

	  if (r != bfd_reloc_ok)
	    {
	      switch (r)
		{
		case bfd_reloc_undefined:
		  if (!((*link_info->callbacks->undefined_symbol)
			(link_info,
			 bfd_asymbol_name (*(*parent)->sym_ptr_ptr),
			 input_bfd, input_section, (*parent)->address, true)))
		    goto error_return;
		  break;
		case bfd_reloc_dangerous:
		  BFD_ASSERT (error_message != (char *) NULL);
		  if (!((*link_info->callbacks->reloc_dangerous)
			(link_info, error_message, input_bfd, input_section,
			 (*parent)->address)))
		    goto error_return;
		  break;
		case bfd_reloc_overflow:
		  if (!((*link_info->callbacks->reloc_overflow)
			(link_info,
			 bfd_asymbol_name (*(*parent)->sym_ptr_ptr),
			 (*parent)->howto->name, (*parent)->addend, input_bfd,
			 input_section, (*parent)->address)))
		    goto error_return;
		  break;
		case bfd_reloc_outofrange:
		default:
		  DPRINT (10, ("get_rel_sec_cont fails, perform reloc "
			       "returned $%x\n", r));
		  abort ();
		  break;
		}

	    }
	}
    }
  if (reloc_vector != NULL)
    free (reloc_vector);
  DPRINT (5, ("GRSC: Returning ok\n"));
  return data;

error_return:
  DPRINT (5, ("GRSC: Error_return\n"));
  if (reloc_vector != NULL)
    free (reloc_vector);
  return NULL;
}


/* Add a value to a location */
static bfd_reloc_status_type
my_add_to (data, offset, size, add, sign)
     PTR data;
     int offset, size, add, sign;
{
  signed char *p;
  int val;
  bfd_reloc_status_type ret;

  DPRINT (5, ("Entering add_value\n"));

  ret = bfd_reloc_ok;
  p = ((signed char *) data) + offset;

  switch (size)
    {
    case 0:			/* byte size */
      val = (int) (p[0]) + add;
      /* check for overflow */
      if (sign)
	{
	  if (val < -0x80 || val > 0x7f)
	    ret = bfd_reloc_overflow;
	}
      else
	{
	  if ((val & 0xffffff00) != 0 && (val & 0xffffff00) != 0xffffff00)
	    ret = bfd_reloc_overflow;
	}
      /* set the value */
      p[0] = val & 0xff;
      break;

    case 1:			/* word size */
      val = (int) ((p[1] & 0xff) | (p[0] << 8)) + add;
      /* check for overflow */
      if (sign)
	{
	  if (val < -0x8000 || val > 0x7fff)
	    ret = bfd_reloc_overflow;
	}
      else
	{
	  if ((val & 0xffff0000) != 0 && (val & 0xffff0000) != 0xffff0000)
	    ret = bfd_reloc_overflow;
	}
      /* set the value */
      p[1] = val & 0xff;
      p[0] = ((val & 0xff00) >> 8) & 0xff;
      break;

    case 2:			/* long word */
      val = bfd_getb_signed_32 (p) + add;
      /* If we are linking a resident program, then we limit the reloc size
         to about +/- 1 GB.

         When linking a shared library all variables defined in other
         libraries are placed in memory >0x80000000, so if the library
         tries to use one of those variables an error is output.

         Without this it would be much more difficult to check for
         incorrect references.
       */
      if (amiga_resident && (val & 0xc0000000) != 0 && (val & 0xc0000000) != 0xc0000000)	/* Overflow */
	{
	  ret = bfd_reloc_overflow;
	}
      bfd_putb32 (val, p);
      break;

    default:			/* Error */
      ret = bfd_reloc_notsupported;
      break;
    }				/* Of switch */

  DPRINT (5, ("Leaving add_value\n"));
  return (ret);
}

/* Set a value to a location */
static bfd_reloc_status_type
my_set_to (data, offset, size, val, sign)
     PTR data;
     int offset, size, val, sign;
{
  signed char *p;
  bfd_reloc_status_type ret;

  DPRINT (5, ("Entering add_value\n"));

  ret = bfd_reloc_ok;
  p = ((signed char *) data) + offset;

  switch (size)
    {
    case 0:			/* byte size */
      /* check for overflow */
      if (sign)
	{
	  if (val < -0x80 || val > 0x7f)
	    ret = bfd_reloc_overflow;
	}
      else
	{
	  if ((val & 0xffffff00) != 0 && (val & 0xffffff00) != 0xffffff00)
	    ret = bfd_reloc_overflow;
	}
      /* set the value */
      p[0] = val & 0xff;
      break;

    case 1:			/* word size */
      /* check for overflow */
      if (sign)
	{
	  if (val < -0x8000 || val > 0x7fff)
	    ret = bfd_reloc_overflow;
	}
      else
	{
	  if ((val & 0xffff0000) != 0 && (val & 0xffff0000) != 0xffff0000)
	    ret = bfd_reloc_overflow;
	}
      /* set the value */
      p[1] = val & 0xff;
      p[0] = ((val & 0xff00) >> 8) & 0xff;
      break;

    case 2:			/* long word */
      /* If we are linking a resident program, then we limit the reloc size
         to about +/- 1 GB.

         When linking a shared library all variables defined in other
         libraries are placed in memory >0x80000000, so if the library
         tries to use one of those variables an error is output.

         Without this it would be much more difficult to check for
         incorrect references.
       */
      if (amiga_resident && (val & 0xc0000000) != 0 && (val & 0xc0000000) != 0xc0000000)	/* Overflow */
	{
	  ret = bfd_reloc_overflow;
	}
      p[3] = val & 0xff;
      p[2] = (val >> 8) & 0xff;
      p[1] = (val >> 16) & 0xff;
      p[0] = (val >> 24) & 0xff;
      break;

    default:			/* Error */
      ret = bfd_reloc_notsupported;
      break;
    }				/* Of switch */

  DPRINT (5, ("Leaving set_value\n"));
  return (ret);
}

/* Perform an Amiga relocation */
static bfd_reloc_status_type
amiga_perform_reloc (abfd, r, data, sec, obfd, error_message)
     bfd *abfd;
     arelent *r;
     PTR data;
     asection *sec;
     bfd *obfd;
     char **error_message;
{
  asymbol *sym;			/* Reloc is relative to sym */
  asection *target_section;	/* reloc is relative to this section */
  int relocation;
  boolean copy;
  bfd_reloc_status_type ret;
  int size = 2;
  int sign = false;

  DPRINT (5, ("Entering APR\nflavour is %d (aflavour=%d, aout_flavour=%d)\n",
	      sec->owner->xvec->flavour, bfd_target_amiga_flavour,
	      bfd_target_aout_flavour));

  /* If obfd==NULL: Apply the reloc, if possible. */
  /* Else: Modify it and return */

  if (obfd != NULL)		/* Only modify the reloc */
    {
      r->address += sec->output_offset;
      sec->output_section->flags |= SEC_RELOC;
      DPRINT (5, ("Leaving APR, modified case \n"));
      return bfd_reloc_ok;
    }

  /* Try to apply the reloc */

  sym = *(r->sym_ptr_ptr);

#if 0
  /* FIXME */
  if (sym->udata.p)
    sym = ((struct generic_link_hash_entry *) sym->udata.p)->sym;
#endif

  target_section = sym->section;

  if (target_section == bfd_und_section_ptr)	/* Error */
    {
      DPRINT (10, ("perform_reloc: Target is undefined section\n"));
      return bfd_reloc_undefined;
    }

  relocation = 0;
  copy = false;
  ret = bfd_reloc_ok;

  switch (r->howto->type)
    {
    case HUNK_RELOC32:		/* 32 bit reloc */
      DPRINT (5, ("RELOC32\n"));
      size = 2;
      if (target_section == bfd_abs_section_ptr)	/* Ref to absolute hunk */
	relocation = sym->value;
      else if (target_section == bfd_com_section_ptr)	/* ref to common */
	{
	  relocation = 0;
	  copy = true;
	}
      else
	{
	  /* If we access a symbol in the .bss section, we have to convert
	     this to an access to .data section */
	  /* This is done through a change to the symbol... */
	  if (amiga_base_relative
	      && (strcmp (sym->section->output_section->name, ".bss") == 0))
	    {
	      /* get value for .data section */
	      bfd *ibfd;
	      asection *s;

	      ibfd = target_section->output_section->owner;
	      for (s = ibfd->sections; s != NULL; s = s->next)
		if (strcmp (s->name, ".data") == 0)
		  {
		    sym->section->output_offset = s->_raw_size;
		    sym->section->output_section = s;
		  }
	    }

	  relocation = 0;
	  copy = true;
	}
      break;

    case HUNK_RELOC8:
    case HUNK_RELOC16:
      DPRINT (5, ("RELOC16/8\n"));
      size = (r->howto->type == HUNK_RELOC8) ? 0 : 1;
      if (target_section == bfd_abs_section_ptr)	/* Ref to absolute hunk */
	relocation = sym->value;
      else
	{
	  if (target_section == bfd_com_section_ptr)	/* Error.. */
	    {
	      relocation = 0;
	      copy = false;
	      ret = bfd_reloc_undefined;
	    }
	  else
	    {
	      DPRINT (5, ("PC relative\n"));
	      /* This is a pc relative hunk... */
	      if (sec->output_section != target_section->output_section)	/* Error */
		{
		  DPRINT (10, ("pc relative, but out of range I\n"));
		  relocation = 0;
		  copy = false;
		  ret = bfd_reloc_outofrange;
		}
	      else
		{		/* Same section */
		  relocation = -(r->address + sec->output_offset);
		  copy = false;
		}
	    }
	}
      break;

    case HUNK_DREL32:		/* baserel relocs */
    case HUNK_DREL16:
    case HUNK_DREL8:
      DPRINT (5, ("HUNK_BASEREL relocs\n"));

      /* Relocs are always relative to the symbol ___a4_init */
      /* Relocs to .bss section are converted to a reloc to .data section, since
         .bss section contains only COMMON sections...... and should be
         following .data section.. */

      size =
	(r->howto->type ==
	 HUNK_DREL32) ? 2 : ((r->howto->type == HUNK_DREL16) ? 1 : 0);

      if (target_section == bfd_abs_section_ptr)
	{
	  relocation = sym->value;
	}
      else
	{
	  if (!(AMIGA_DATA (target_section->output_section->owner)->baserel))
	    {
	      fprintf (stderr,
		       "Base symbol for base relative reloc not defined,"
		       "section %s, reloc to symbol %s\n", sec->name,
		       sym->name);
	      copy = false;
	      ret = bfd_reloc_notsupported;
	      break;
	    }

	  /* If target->out is .bss, add the value of the .data section to sym->value and
	     set output_section new to .data section.... */
	  if (strcmp (target_section->output_section->name, ".bss") == 0)
	    {
	      bfd *ibfd;
	      asection *s;

	      ibfd = target_section->output_section->owner;
	      for (s = ibfd->sections; s != NULL; s = s->next)
		if (strcmp (s->name, ".data") == 0)
		  {
		    sym->section->output_section = s;
		    sym->section->output_offset = s->_raw_size;
		  }
	    }

	  relocation = sym->value + sym->section->output_offset
	    - (AMIGA_DATA (target_section->output_section->owner))->a4init
	    + r->addend;
	  copy = false;
	  sign = true;
	}
      break;

    default:
      fprintf (stderr, "Error:Not supported reloc type:%d\n", r->howto->type);
      copy = false;
      relocation = 0;
      ret = bfd_reloc_notsupported;
      break;
    }				/* Of switch */

  /* Add in relocation */
  if (relocation != 0)
    ret = my_add_to (data, r->address, size, relocation, sign);

  if (copy)			/* Copy reloc to output section */
    {
      DPRINT (5, ("Copying reloc\n"));
      target_section = sec->output_section;
      r->address += sec->output_offset;
      target_section->orelocation[target_section->reloc_count++] = r;
      sec->output_section->flags |= SEC_RELOC;
    }
  DPRINT (5, ("Leaving a_perform_reloc\n"));
  return ret;
}


/* Perform an a.out reloc */
static bfd_reloc_status_type
aout_perform_reloc (abfd, r, data, sec, obfd, error_message)
     bfd *abfd;
     arelent *r;
     PTR data;
     asection *sec;
     bfd *obfd;
     char **error_message;
{
  asymbol *sym;			/* Reloc is relative to this */
  asection *target_section;	/* reloc is relative to this section */
  int relocation;
  boolean copy, addval = true;
  bfd_reloc_status_type ret;
  int size = 2;
  int sign = false;

  /* If obfd==NULL: Apply the reloc, if possible. */
  /* Else: Modify it and return */
  DPRINT (5, ("Entering aout_perf_reloc\n"));
  if (obfd != NULL)		/* Only modify the reloc */
    {
      r->address += sec->output_offset;
      DPRINT (5, ("Leaving aout_perf_reloc, modified\n"));
      return bfd_reloc_ok;
    }

  sym = *(r->sym_ptr_ptr);
  target_section = sym->section;

  if (target_section == bfd_und_section_ptr)	/* Error */
    {
      DPRINT (10, ("target_sec=UND, aout_perf_rel\n"));
      return bfd_reloc_undefined;
    }
  relocation = 0;
  copy = false;
  ret = bfd_reloc_ok;

  switch (r->howto->type)
    {
    case 0:			/* 8 bit reloc, pc relative or absolute */
    case 1:			/* 16 bit reloc */
      DPRINT (10, ("16/8 bit relative\n"));
      size = r->howto->type;
      if (target_section == bfd_abs_section_ptr)	/* Ref to absolute hunk */
	relocation = sym->value;
      else if (target_section == bfd_com_section_ptr)	/* Error.. */
	{
	  relocation = 0;
	  copy = false;
	  ret = bfd_reloc_undefined;
	  fprintf (stderr, "Pc relative relocation to  common symbol \"%s\" "
		   "in section %s\n", sym->name, sec->name);
	  DPRINT (10, ("Ref to common symbol...aout_perf_reloc\n"));
	}
      else
	{
	  /* This is a pc relative hunk... or a baserel... */
	  if (sec->output_section != target_section->output_section)
	    /* Error or baserel */
	    {
	      if (target_section->output_section->flags & SEC_DATA != 0)
		/* Baserel reloc */
		{
		  goto baserel;	/* Dirty, but no code doubling.. */
		}		/* Of is baserel */

	      relocation = 0;
	      copy = false;
	      ret = bfd_reloc_outofrange;
	      fprintf (stderr,
		       "pc relative relocation out of range in section"
		       "%s. Relocation was to symbol %s\n", sec->name,
		       sym->name);

	      DPRINT (10, ("Section%s, target %s: Reloc out of range..."
			   "not same section, aout_perf\nsec->out=%s, target->out"
			   "=%s, offset=%lx\n", sec->name,
			   target_section->name, sec->output_section->name,
			   target_section->output_section->name, r->address));
	    }
	  else
	    {			/* Same section */
	      relocation = -(r->address + sec->output_offset);
	      copy = false;
	      DPRINT (5, ("Reloc to same section...\n"));
	    }
	}
      break;

    case 4:			/* 8 bit displacement */
    case 5:			/* 16 bit displacement */
    case 6:			/* 32 bit displacement */
      size = r->howto->type - 4;
      sign = true;
      if (target_section == bfd_abs_section_ptr)	/* Ref to absolute hunk */
	relocation = sym->value;
      else
	{
	  relocation = (sym->value + target_section->output_offset) -
	    (r->address + sec->output_offset);
	  if (size == 0)
	    relocation--;
	  addval = false;
	  copy = 0;
	}
      break;

    case 2:			/* 32 bit reloc, abs. or relative */
      DPRINT (10, ("32 bit\n"));
      size = 2;
      if (target_section == bfd_abs_section_ptr)	/* Ref to absolute hunk */
	relocation = sym->value;
      else if (target_section == bfd_com_section_ptr)	/* ref to common */
	{
	  relocation = 0;
	  copy = true;
	}
      else
	{
	  /* If we access a symbol in the .bss section, we have to convert
	     this to an access to .data section */
	  /* This is done through a change to the output section of
	     the symbol... */
	  if (amiga_base_relative
	      && (strcmp (sym->section->output_section->name, ".bss") == 0))
	    {
	      /* get value for .data section */
	      bfd *ibfd;
	      asection *s;

	      ibfd = target_section->output_section->owner;
	      for (s = ibfd->sections; s != NULL; s = s->next)
		if (strcmp (s->name, ".data") == 0)
		  {
		    sym->section->output_section = s;
		    sym->section->output_offset += s->_raw_size;
		  }
	    }
	  relocation = 0;
	  copy = true;
	}
      DPRINT (10, ("target->out=%s(%lx), sec->out=%s(%lx), symbol=%s\n",
		   target_section->output_section->name,
		   target_section->output_section, sec->output_section->name,
		   sec->output_section, sym->name));
      break;

    case 9:			/* 16 bit base relative */
    case 10:			/* 32 bit base relative */
      DPRINT (10, ("32/16 bit baserel\n"));
      /* We use the symbol ___a4_init as base */
      size = r->howto->type - 8;

    baserel:
      if (target_section == bfd_abs_section_ptr)
	{
	  relocation = sym->value;
	}
      else if (target_section == bfd_com_section_ptr)	/* Error.. */
	{
	  relocation = 0;
	  copy = false;
	  ret = bfd_reloc_undefined;
	  fprintf (stderr, "Baserelative relocation to common \"%s\"\n",
		   sym->name);

	  DPRINT (10, ("Ref to common symbol...aout_perf_reloc\n"));
	}
      else			/* Target section and sec need not be the same... */
	{
	  if (!(AMIGA_DATA (target_section->output_section->owner)->baserel))

	    {
	      fprintf (stderr,
		       "Base symbol for base relative reloc not defined,"
		       "section %s, reloc to symbol %s\n", sec->name,
		       sym->name);
	      copy = false;
	      ret = bfd_reloc_notsupported;
	      DPRINT (10,
		      ("target->out=%s(%lx), sec->out=%s(%lx), symbol=%s\n",
		       target_section->output_section->name,
		       target_section->output_section,
		       sec->output_section->name, sec->output_section,
		       sym->name));

	      break;
	    }

	  /* If target->out is .bss, add the value of the .data section....
	     to sym, set new output_section */
	  if (strcmp (target_section->output_section->name, ".bss") == 0)
	    {
	      bfd *ibfd;
	      asection *s;

	      ibfd = target_section->output_section->owner;

	      for (s = ibfd->sections; s != NULL; s = s->next)
		if (strcmp (s->name, ".data") == 0)
		  {
		    sym->section->output_offset += s->_raw_size;
		    sym->section->output_section = s;
		  }
	    }

	  relocation = sym->value + target_section->output_offset - 0x7ffe;

	  /* if the symbol is in bss, subtract the offset that gas has put
	     into the opcode */
	  if (target_section->index == 2)
	    relocation -= abfd->tdata.aout_data->a.datasec->_raw_size;

	  addval = true;
	  copy = false;
	  sign = true;

	  DPRINT (20,
		  ("symbol=%s (0x%lx)\nsection %s (0x%lx; %s; output=0x%lx)"
		   "\nrelocation @0x%lx\n\n", sym->name, sym->value,
		   sym->section->name, (unsigned long) sym->section,
		   sym->section->owner->filename, sym->section->output_offset,
		   r->address));
	}
      DPRINT (10, ("target->out=%s(%lx), sec->out=%s(%lx), symbol=%s\n",
		   target_section->output_section->name,
		   target_section->output_section, sec->output_section->name,
		   sec->output_section, sym->name));
      break;

    default:
      copy = false;
      relocation = 0;
      ret = bfd_reloc_notsupported;
      fprintf (stderr, "Unsupported reloc: %d\n", r->howto->type);
      break;

    }				/* Of switch */

  /* Add in relocation */
  if (relocation != 0)
    if (addval)
      ret = my_add_to (data, r->address, size, relocation, sign);
    else
      ret = my_set_to (data, r->address, size, relocation, sign);

  if (copy)			/* Copy reloc to output section */
    {
      DPRINT (5, ("Copying reloc\n"));
      target_section = sec->output_section;
      r->address += sec->output_offset;
      target_section->orelocation[target_section->reloc_count++] = r;
    }
  DPRINT (5,
	  ("Leaving aout_perf_reloc with %d (OK=%d)\n", ret, bfd_reloc_ok));
  return ret;
}


/* The final link routine, used both by Amiga and a.out backend*/
/* This is nearly a copy of _bfd_generic_final_link */
boolean
amiga_final_link (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  bfd *sub;
  asection *o, *act_sec;
  struct bfd_link_order *p;
  size_t outsymalloc;
  struct generic_write_global_symbol_info wginfo;
  struct bfd_link_hash_entry *h =
    bfd_link_hash_lookup (info->hash, "___a4_init", false, false, true);

  if (h && h->type == bfd_link_hash_defined)
    {
      AMIGA_DATA (abfd)->baserel = true;
      AMIGA_DATA (abfd)->a4init = h->u.def.value;
    }
  else
    AMIGA_DATA (abfd)->baserel = false;

  DPRINT (5, ("Entering final_link\n"));

  if (abfd->xvec->flavour == bfd_target_aout_flavour)
    return amiga_aout_bfd_final_link (abfd, info);

  abfd->outsymbols = (asymbol **) NULL;
  abfd->symcount = 0;
  outsymalloc = 0;

  /* Mark all sections which will be included in the output file.  */
  for (o = abfd->sections; o != NULL; o = o->next)
    for (p = o->link_order_head; p != NULL; p = p->next)
      if (p->type == bfd_indirect_link_order)
	p->u.indirect.section->linker_mark = true;

  /* Build the output symbol table.  */
  for (sub = info->input_bfds; sub != (bfd *) NULL; sub = sub->link_next)
    if (!_bfd_generic_link_output_symbols (abfd, sub, info, &outsymalloc))
      return false;

  DPRINT (10, ("Did build output symbol table\n"));

  /* Accumulate the global symbols.  */
  wginfo.info = info;
  wginfo.output_bfd = abfd;
  wginfo.psymalloc = &outsymalloc;
  _bfd_generic_link_hash_traverse (_bfd_generic_hash_table (info),
				   _bfd_generic_link_write_global_symbol,
				   (PTR) & wginfo);

  DPRINT (10, ("Accumulated global symbols\n"));

  DPRINT (10, ("Output bfd is %s(%lx)\n", abfd->filename, abfd));

  /* Allocate space for the output relocs for each section.  */
  /* We also handle base-relative linking special, by setting the .data
     sections real length to it's length + .bss length */
  /* This is different to bfd_generic_final_link: We ALWAYS alloc space for
     the relocs, because we may need it anyway */
  for (o = abfd->sections; o != (asection *) NULL; o = o->next)
    {
      /* If section is .data, find .bss and add that length */
      if (!info->relocateable && amiga_base_relative &&
	  (strcmp (o->name, ".data") == 0))
	{
	  if (abfd->xvec->flavour != bfd_target_amiga_flavour)	/* oops */
	    {
	      fprintf (stderr, "You can't use base relative linking with"
		       " partial links.\n");
	    }
	  else
	    {
#if 0
	      for (act_sec = abfd->sections; act_sec != NULL;
		   act_sec = act_sec->next)
		if (strcmp (act_sec->name, ".bss") == 0)
		  amiga_per_section (o)->disk_size = o->_raw_size +
		    act_sec->_raw_size;
#endif
	    }
	}			/* Of base-relative linking */

      DPRINT (10, ("Section in output bfd is %s (%lx)\n", o->name, o));

      o->reloc_count = 0;
      for (p = o->link_order_head;
	   p != (struct bfd_link_order *) NULL; p = p->next)
	{
	  if (p->type == bfd_section_reloc_link_order
	      || p->type == bfd_symbol_reloc_link_order)
	    ++o->reloc_count;
	  else if (p->type == bfd_indirect_link_order)
	    {
	      asection *input_section;
	      bfd *input_bfd;
	      long relsize;
	      arelent **relocs;
	      asymbol **symbols;
	      long reloc_count;

	      input_section = p->u.indirect.section;
	      input_bfd = input_section->owner;

	      DPRINT (10,
		      ("\tIndirect section from bfd %s, section is %s(%lx)"
		       " (COM=%lx)\n", input_bfd->filename,
		       input_section->name, input_section,
		       bfd_com_section_ptr));

	      relsize = bfd_get_reloc_upper_bound (input_bfd, input_section);
	      if (relsize < 0)
		{
		  DPRINT (10, ("Relsize<0.I..in bfd %s, sec %s\n",
			       input_bfd->filename, input_section->name));
		  return false;
		}

	      relocs = (arelent **) malloc ((size_t) relsize);

	      if (!relocs && relsize != 0)
		{
		  bfd_set_error (bfd_error_no_memory);
		  return false;
		}
	      symbols = _bfd_generic_link_get_symbols (input_bfd);
	      reloc_count = bfd_canonicalize_reloc (input_bfd,
						    input_section,
						    relocs, symbols);
	      if (reloc_count < 0)
		{
		  DPRINT (10, ("Relsize<0.II..in bfd %s, sec %s\n",
			       input_bfd->filename, input_section->name));
		  return false;
		}

	      BFD_ASSERT (reloc_count == input_section->reloc_count);
	      o->reloc_count += reloc_count;
	      free (relocs);
	    }
	}
      if (o->reloc_count > 0)
	{
	  o->orelocation = ((arelent **)
			    bfd_alloc (abfd,
				       (o->reloc_count
					* sizeof (arelent *))));

	  if (!o->orelocation)
	    {
	      bfd_set_error (bfd_error_no_memory);
	      return false;
	    }
	  /* o->flags |= SEC_RELOC; There may be no relocs. This can be
	     determined only later */
	  /* Reset the count so that it can be used as an index
	     when putting in the output relocs.  */
	  o->reloc_count = 0;
	}
    }

  DPRINT (10, ("Got all relocs\n"));

  /* Handle all the link order information for the sections.  */
  for (o = abfd->sections; o != (asection *) NULL; o = o->next)
    {
      for (p = o->link_order_head;
	   p != (struct bfd_link_order *) NULL; p = p->next)
	{
	  switch (p->type)
	    {
	    case bfd_section_reloc_link_order:
	    case bfd_symbol_reloc_link_order:
	      if (!amiga_reloc_link_order (abfd, info, o, p))	/* We use an own routine */
		return false;
	      break;
	    case bfd_indirect_link_order:
	      if (!default_indirect_link_order (abfd, info, o, p, false))
		/* Calls our get_relocated_section_contents */
		return false;
	      break;
	    default:
	      if (!_bfd_default_link_order (abfd, info, o, p))
		return false;
	      break;
	    }
	}
    }
  if (abfd->xvec->flavour == bfd_target_amiga_flavour && !info->relocateable)
    AMIGA_DATA (abfd)->IsLoadFile = true;

  DPRINT (10, ("Leaving final_link\n"));
  return true;
}


/* Handle reloc link order . This is nearly a copy from generic_reloc_link_order
   in linker.c*/
static boolean
amiga_reloc_link_order (abfd, info, sec, link_order)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     struct bfd_link_order *link_order;
{
  amiga_reloc_type *r;

  DPRINT (5, ("Entering amiga_reloc_link_order\n"));

  /* We generate a new AMIGA style reloc */
  BFD_ASSERT (sec->orelocation != NULL);

  if (sec->orelocation == (arelent **) NULL)
    {
      DPRINT (10, ("aborting, since orelocation==NULL\n"));
      abort ();
    }

  r = (amiga_reloc_type *) bfd_zalloc (abfd, sizeof (amiga_reloc_type));
  if (r == NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      DPRINT (5, ("Leaving amiga_reloc_link, no mem\n"));
      return false;
    }

  r->relent.address = link_order->offset;
  r->relent.howto =
    bfd_reloc_type_lookup (abfd, link_order->u.reloc.p->reloc);
  if (r->relent.howto == NULL)
    {
      bfd_set_error (bfd_error_bad_value);
      DPRINT (5, ("Leaving amiga_reloc_link, bad value\n"));
      return false;
    }

  /* Get the symbol to use for the relocation.  */
  if (link_order->type == bfd_section_reloc_link_order)
    r->relent.sym_ptr_ptr = link_order->u.reloc.p->u.section->symbol_ptr_ptr;
  else
    {
      struct generic_link_hash_entry *h;

      h = _bfd_generic_link_hash_lookup (_bfd_generic_hash_table (info),
					 link_order->u.reloc.p->u.name,
					 false, false, true);
      if (h == (struct generic_link_hash_entry *) NULL || !h->written)
	{
	  if (!((*info->callbacks->unattached_reloc)
		(info, link_order->u.reloc.p->u.name,
		 (bfd *) NULL, (asection *) NULL, (bfd_vma) 0)))
	    return false;
	  bfd_set_error (bfd_error_bad_value);
	  DPRINT (5,
		  ("Leaving amiga_reloc_link, bad value in hash lookup\n"));
	  return false;
	}
      r->relent.sym_ptr_ptr = &h->sym;
    }
  DPRINT (5, ("Got symbol for relocation\n"));
  /* Store the addend */
  r->relent.addend = link_order->u.reloc.p->addend;


  /* If we are generating relocateable output, just add the reloc */
  if (info->relocateable)
    {
      DPRINT (5, ("Adding reloc\n"));
      sec->orelocation[sec->reloc_count] = (arelent *) r;
      ++sec->reloc_count;
      sec->flags |= SEC_RELOC;
    }
  else
    {				/* Try to apply the reloc */
      char *em = "";
      PTR data;
      bfd_reloc_status_type ret;

      DPRINT (5, ("Apply link_order_reloc\n"));
      /*FIXME: Maybe, we have to get the section contents, before we use them,
         if they have not been
         set by now.. */

      BFD_ASSERT (sec->contents != NULL);
      data = (PTR) (sec->contents);

      if (bfd_get_flavour (abfd) == bfd_target_amiga_flavour)
	ret = amiga_perform_reloc (abfd, (arelent *) r, data, sec, NULL, &em);
      else
	ret = aout_perform_reloc (abfd, (arelent *) r, data, sec, NULL, &em);

      if (ret != bfd_reloc_ok)
	{
	  DPRINT (5, ("Leaving amiga_reloc_link, value false\n"));
	  return false;
	}
    }
  DPRINT (5, ("Leaving amiga_reloc_link\n"));
  return true;
}
