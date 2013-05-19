/* BFD back-end for Commodore-Amiga AmigaOS binaries. Linker routines.
   Copyright (C) 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998
   Free Software Foundation, Inc.
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
	  @code{get_relocated_section_contents}.

This ensures that the link is performed properly, but has the side effect of
loosing performance.

The amiga bfd code uses the same functions since they check for the used flavour.
@@*

The usage of a special linker code has one reason:
The bfd library assumes that a program is always loaded at a known memory
address. This is not a case on an Amiga. So the Amiga format has to take over
some relocs to an executable output file.
This is not the case with a.out formats, so there relocations can be applied at link time,
not at run time, like on the Amiga.
The special routines compensate this: instead of applying the relocations, they are
copied to the output file, if neccessary.
As as consequence, @code{final_link} and @code{get_relocated_section_contents} are nearly identical to
the original routines from @file{linker.c} and @file{reloc.c}.
*/

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "bfdlink.h"
#include "genlink.h"
#include "libamiga.h"

#define bfd_msg (*_bfd_error_handler)

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

/* This one is used by the linker and tells us, if a debug hunk should be
   written out */
int write_debug_hunk = 1;

/* This is also used by the linker to set the attribute of sections */
int amiga_attribute = 0;

/* This one is used to indicate base-relative linking */
int amiga_base_relative = 0;

/* This one is used to indicate -resident linking */
int amiga_resident = 0;

bfd_boolean
default_indirect_link_order PARAMS ((bfd *, struct bfd_link_info *,
	 asection *, struct bfd_link_order *, bfd_boolean));
bfd_byte *
get_relocated_section_contents PARAMS ((bfd *, struct bfd_link_info *,
	struct bfd_link_order *, bfd_byte *, bfd_boolean, asymbol **));
bfd_boolean
amiga_final_link PARAMS ((bfd *, struct bfd_link_info *));
bfd_boolean
aout_amiga_final_link PARAMS ((bfd *, struct bfd_link_info *));

static bfd_reloc_status_type
my_add_to PARAMS ((arelent *, PTR, int, int));
static bfd_reloc_status_type
amiga_perform_reloc PARAMS ((bfd *, arelent *, PTR, sec_ptr, bfd *, char **));
static bfd_reloc_status_type
aout_perform_reloc PARAMS ((bfd *, arelent *, PTR, sec_ptr, bfd *, char **));
static bfd_boolean
amiga_reloc_link_order PARAMS ((bfd *, struct bfd_link_info *, asection *,
	struct bfd_link_order *));

enum { ADDEND_UNSIGNED=0x01, RELOC_SIGNED=0x02 };


/* This one is nearly identical to bfd_generic_get_relocated_section_contents
   in reloc.c */
bfd_byte *
get_relocated_section_contents (abfd, link_info, link_order, data,
				relocateable, symbols)
     bfd *abfd;
     struct bfd_link_info *link_info;
     struct bfd_link_order *link_order;
     bfd_byte *data;
     bfd_boolean relocateable;
     asymbol **symbols;
{
  /* Get enough memory to hold the stuff.  */
  bfd *input_bfd = link_order->u.indirect.section->owner;
  asection *input_section = link_order->u.indirect.section;

  long reloc_size = bfd_get_reloc_upper_bound (input_bfd, input_section);
  arelent **reloc_vector = NULL;
  long reloc_count;
  bfd_reloc_status_type (*reloc_func)(bfd *, arelent *, PTR, sec_ptr,
				      bfd *, char **);

  DPRINT(5,("Entering get_rel_sec_cont\n"));

  if (reloc_size < 0)
    goto error_return;

  if (bfd_get_flavour (input_bfd) == bfd_target_amiga_flavour)
    reloc_func = amiga_perform_reloc;
  else if (bfd_get_flavour (input_bfd) == bfd_target_aout_flavour)
    reloc_func = aout_perform_reloc;
  else
    {
      bfd_set_error (bfd_error_bad_value);
      goto error_return;
    }

  reloc_vector = (arelent **) bfd_malloc ((bfd_size_type) reloc_size);
  if (reloc_vector == NULL && reloc_size != 0)
    goto error_return;

  DPRINT(5,("GRSC: GetSecCont()\n"));
  /* Read in the section.  */
  if (!bfd_get_section_contents (input_bfd,
				 input_section,
				 (PTR) data,
				 (bfd_vma) 0,
				 input_section->_raw_size))
    goto error_return;

  /* We're not relaxing the section, so just copy the size info.  */
  input_section->_cooked_size = input_section->_raw_size;
  input_section->reloc_done = TRUE;

  DPRINT(5,("GRSC: CanReloc\n"));
  reloc_count = bfd_canonicalize_reloc (input_bfd,
					input_section,
					reloc_vector,
					symbols);
  if (reloc_count < 0)
    goto error_return;

  if (reloc_count > 0)
    {
      arelent **parent;

      DPRINT(5,("reloc_count=%ld\n",reloc_count));

      for (parent = reloc_vector; *parent != (arelent *) NULL;
	   parent++)
	{
	  char *error_message = (char *) NULL;
	  bfd_reloc_status_type r;

	  DPRINT(5,("Applying a reloc\nparent=%lx, reloc_vector=%lx, "
		    "*parent=%lx\n",parent,reloc_vector,*parent));
	  r=(*reloc_func) (input_bfd,
			   *parent,
			   (PTR) data,
			   input_section,
			   relocateable ? abfd : (bfd *) NULL,
			   &error_message);
	  if (relocateable)
	    {
	      asection *os = input_section->output_section;

	      DPRINT(5,("Keeping reloc\n"));
	      /* A partial link, so keep the relocs.  */
	      os->orelocation[os->reloc_count] = *parent;
	      os->reloc_count++;
	    }

	  if (r != bfd_reloc_ok)
	    {
	      switch (r)
		{
		case bfd_reloc_undefined:
		  if (!((*link_info->callbacks->undefined_symbol)
			(link_info, bfd_asymbol_name (*(*parent)->sym_ptr_ptr),
			 input_bfd, input_section, (*parent)->address,
			 TRUE)))
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
			(link_info, bfd_asymbol_name (*(*parent)->sym_ptr_ptr),
			 (*parent)->howto->name, (*parent)->addend,
			 input_bfd, input_section, (*parent)->address)))
		    goto error_return;
		  break;
		case bfd_reloc_outofrange:
		default:
		  DPRINT(10,("get_rel_sec_cont fails, perform reloc "
			     "returned $%x\n",r));
		  abort ();
		  break;
		}

	    }
	}
    }
  if (reloc_vector != NULL)
    free (reloc_vector);
  DPRINT(5,("GRSC: Returning ok\n"));
  return data;

error_return:
  DPRINT(5,("GRSC: Error_return\n"));
  if (reloc_vector != NULL)
    free (reloc_vector);
  return NULL;
}


/* Add a value to a location */
static bfd_reloc_status_type
my_add_to (r, data, add, flags)
     arelent *r;
     PTR data;
     int add, flags;
{
  bfd_reloc_status_type ret=bfd_reloc_ok;
  bfd_byte *p=((bfd_byte *)data)+r->address;
  int val;

  DPRINT(5,("Entering add_value\n"));

  switch (r->howto->size)
    {
    case 0: /* byte size */
      if ((flags & ADDEND_UNSIGNED) == 0)
	val = ((*p & 0xff) ^ 0x80) - 0x80 + add;
      else
	val = (*p & 0xff) + add;
      /* check for overflow */
      if ((flags & RELOC_SIGNED) != 0) {
	if (val<-0x80 || val>0x7f)
	  ret = bfd_reloc_overflow;
      }
      else {
	if ((val&0xffffff00)!=0 && (val&0xffffff00)!=0xffffff00)
	  ret=bfd_reloc_overflow;
      }
      /* set the value */
      *p = val & 0xff;
      break;

    case 1: /* word size */
      if ((flags & ADDEND_UNSIGNED) == 0)
	val = bfd_getb_signed_16 (p) + add;
      else
	val = bfd_getb16 (p) + add;
      /* check for overflow */
      if ((flags & RELOC_SIGNED) != 0) {
	if (val<-0x8000 || val>0x7fff)
	  ret = bfd_reloc_overflow;
      }
      else {
	if ((val&0xffff0000)!=0 && (val&0xffff0000)!=0xffff0000)
	  ret=bfd_reloc_overflow;
      }
      /* set the value */
      bfd_putb16 (val, p);
      break;

    case 2: /* long word */
      val = bfd_getb_signed_32 (p) + add;
      /* If we are linking a resident program, then we limit the reloc size
	 to about +/- 1 GB.

	 When linking a shared library all variables defined in other
	 libraries are placed in memory >0x80000000, so if the library
	 tries to use one of those variables an error is output.

	 Without this it would be much more difficult to check for
	 incorrect references. */
      if (amiga_resident &&
	  (val & 0xc0000000)!=0 && (val&0xc0000000)!=0xc0000000) /* Overflow */
	{
	  ret=bfd_reloc_overflow;
	}
      bfd_putb32 (val, p);
      break;

    default: /* Error */
      ret=bfd_reloc_notsupported;
      break;
    }/* Of switch */

  DPRINT(5,("Leaving add_value\n"));
  return ret;
}


/* Perform an Amiga relocation */
static bfd_reloc_status_type
amiga_perform_reloc (abfd, r, data, sec, obfd, error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *r;
     PTR data;
     sec_ptr sec;
     bfd *obfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  asymbol *sym; /* Reloc is relative to sym */
  sec_ptr target_section; /* reloc is relative to this section */
  bfd_reloc_status_type ret;
  bfd_boolean copy;
  int relocation,flags;

  DPRINT(5,("Entering APR\nflavour is %d (amiga_flavour=%d, aout_flavour=%d)\n",
	    bfd_get_flavour (sec->owner), bfd_target_amiga_flavour,
	    bfd_target_aout_flavour));

  /* If obfd==NULL: Apply the reloc, if possible. */
  /* Else: Modify it and return */

  if (obfd!=NULL) /* Only modify the reloc */
    {
      r->address+=sec->output_offset;
      sec->output_section->flags|=SEC_RELOC;
      DPRINT(5,("Leaving amiga_perf_reloc, modified\n"));
      return bfd_reloc_ok;
    }

  /* Try to apply the reloc */

  sym=*(r->sym_ptr_ptr);

  /* FIXME: XXX */
   if (0 && sym->udata.p)
     sym = ((struct generic_link_hash_entry *) sym->udata.p)->sym;

  target_section=sym->section;

  if (bfd_is_und_section(target_section)) /* Error */
    {
      DPRINT(10,("amiga_perf_reloc: target_sec==UND\n"));
      return bfd_reloc_undefined;
    }

  relocation=0; flags=RELOC_SIGNED; copy=FALSE; ret=bfd_reloc_ok;

  DPRINT(5,("%s: size=%u\n",r->howto->name,bfd_get_reloc_size(r->howto)));
  switch (r->howto->type)
    {
    case H_ABS32:
      if (bfd_is_abs_section(target_section)) /* Ref to absolute hunk */
	relocation=sym->value;
      else if (bfd_is_com_section(target_section)) /* ref to common */
	{
	  relocation=0;
	  copy=TRUE;
	}
      else
	{
	  /* If we access a symbol in the .bss section, we have to convert
	     this to an access to .data section */
	  /* This is done through a change to the output section of
	     the symbol.. */
	  if (amiga_base_relative
	      && !strcmp(target_section->output_section->name,".bss"))
	    {
	      /* get value for .data section */
	      bfd *ibfd;
	      sec_ptr s;

	      ibfd=target_section->output_section->owner;
	      for (s=ibfd->sections;s!=NULL;s=s->next)
		if (!strcmp(s->name,".data"))
		  {
		    target_section->output_offset=s->_raw_size;
		    target_section->output_section=s;
		  }
	    }
	  relocation=0;
	  copy=TRUE;
	}
      break;

    case H_PC8: /* pcrel */
    case H_PC16:
    case H_PC32:
      if (bfd_is_abs_section(target_section)) /* Ref to absolute hunk */
	relocation=sym->value;
      else if (bfd_is_com_section(target_section)) /* Error.. */
	{
	  ret=bfd_reloc_undefined;
	}
      else if (sec->output_section!=target_section->output_section) /* Error */
	{
	  DPRINT(5,("pc relative, but out-of-range\n"));
	  ret=bfd_reloc_outofrange;
	}
      else /* Same section */
	{
	  DPRINT(5,("PC relative\n"));
	  relocation = sym->value + target_section->output_offset
	    - (r->address + sec->output_offset);
	}
      break;

    case H_SD8: /* baserel */
    case H_SD16:
    case H_SD32:
      /* Relocs are always relative to the symbol ___a4_init */
      /* Relocs to .bss section are converted to a reloc to .data section,
	 since .bss section contains only COMMON sections...... and should
	 be following .data section.. */
      if (bfd_is_abs_section(target_section))
	relocation=sym->value;
      else if (!AMIGA_DATA(target_section->output_section->owner)->baserel)
	{
	  bfd_msg ("Base symbol for base relative reloc not defined: "
		   "section %s, reloc to symbol %s",sec->name,sym->name);
	  ret=bfd_reloc_notsupported;
	}
      else if ((target_section->flags&SEC_CODE)!=0)
        {
	  bfd_msg ("%s: baserelative text relocation to \"%s\"",
		    abfd->filename, sym->name);
	  ret=bfd_reloc_notsupported;
        }
      else
	{
	  /* If target->out is .bss, add the value of the .data section to
	     sym->value and set new output_section */
	  if (!strcmp(target_section->output_section->name,".bss"))
	    {
	      bfd *ibfd;
	      sec_ptr s;

	      ibfd=target_section->output_section->owner;
	      for (s=ibfd->sections;s!=NULL;s=s->next)
		if (!strcmp(s->name,".data"))
		  {
		    target_section->output_offset=s->_raw_size;
		    target_section->output_section=s;
		  }
	    }

	  relocation = sym->value + target_section->output_offset
	    - (AMIGA_DATA(target_section->output_section->owner))->a4init
	    + r->addend;
	  flags|=ADDEND_UNSIGNED;
	}
      break;

    default:
      bfd_msg ("Error: unsupported reloc: %s(%d)",r->howto->name,r->howto->size);
      ret=bfd_reloc_notsupported;
      break;
    }/* Of switch */

  /* Add in relocation */
  if (relocation!=0)
    ret = my_add_to (r, data, relocation, flags);

  if (copy) /* Copy reloc to output section */
    {
      DPRINT(5,("Copying reloc\n"));
      target_section=sec->output_section;
      r->address+=sec->output_offset;
      target_section->orelocation[target_section->reloc_count++]=r;
      target_section->flags|=SEC_RELOC;
    }
  DPRINT(5,("Leaving amiga_perf_reloc with %d (OK=%d)\n",ret,bfd_reloc_ok));
  return ret;
}


/* Perform an a.out relocation */
static bfd_reloc_status_type
aout_perform_reloc (abfd, r, data, sec, obfd, error_message)
     bfd *abfd;
     arelent *r;
     PTR data;
     sec_ptr sec;
     bfd *obfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  asymbol *sym; /* Reloc is relative to sym */
  sec_ptr target_section; /* reloc is relative to this section */
  bfd_reloc_status_type ret;
  bfd_boolean copy;
  int relocation,flags;

  DPRINT(5,("Entering aout_perf_reloc\n"));

  /* If obfd==NULL: Apply the reloc, if possible. */
  /* Else: Modify it and return */

  if (obfd!=NULL) /* Only modify the reloc */
    {
      r->address+=sec->output_offset;
      DPRINT(5,("Leaving aout_perf_reloc, modified\n"));
      return bfd_reloc_ok;
    }

  /* Try to apply the reloc */

  sym=*(r->sym_ptr_ptr);
  target_section=sym->section;

  if (bfd_is_und_section(target_section)) /* Error */
    {
      if ((sym->flags & BSF_WEAK) == 0)
        {
	  DPRINT(10,("aout_perf_reloc: target_sec==UND\n"));
	  return bfd_reloc_undefined;
	}
      target_section=bfd_abs_section_ptr;
    }

  relocation=0; flags=RELOC_SIGNED; copy=FALSE; ret=bfd_reloc_ok;

  DPRINT(10,("RELOC: %s: size=%u\n",r->howto->name,bfd_get_reloc_size(r->howto)));
  switch (r->howto->type)
    {
    case H_ABS8: /* 8/16 bit reloc, pc relative or absolute */
    case H_ABS16:
      if (bfd_is_abs_section(target_section)) /* Ref to absolute hunk */
	relocation=sym->value;
      else if (bfd_is_com_section(target_section)) /* Error.. */
	{
	  bfd_msg ("pc relative relocation to common symbol \"%s\" in "
		   "section %s",sym->name,sec->name);
	  DPRINT(10,("Ref to common symbol...aout_perf_reloc\n"));
	  ret=bfd_reloc_undefined;
	}
      else if (sec->output_section!=target_section->output_section)
	{
	  if ((target_section->output_section->flags&SEC_DATA)!=0)
	    goto baserel; /* Dirty, but no code duplication.. */
	  bfd_msg ("pc relative relocation out-of-range in section %s. "
		   "Relocation was to symbol %s",sec->name,sym->name);
	  DPRINT(10,("Section %s, target %s: Reloc out-of-range...not same "
		     "section, aout_perf\nsec->out=%s, target->out=%s, "
		     "offset=%lx\n",sec->name,target_section->name,
		     sec->output_section->name,
		     target_section->output_section->name,r->address));
	  ret=bfd_reloc_outofrange;
	}
      else
	{
	  /* Same section, this is a pc relative hunk... */
	  DPRINT(5,("Reloc to same section...\n"));
	  relocation=-(r->address+sec->output_offset);
	}
      break;

    case H_ABS32: /* 32 bit reloc, pc relative or absolute */
      if (bfd_is_abs_section(target_section)) /* Ref to absolute hunk */
	relocation=sym->value;
      else if (bfd_is_com_section(target_section)) /* ref to common */
	{
	  relocation=0;
	  copy=TRUE;
	}
      else
	{
	  /* If we access a symbol in the .bss section, we have to convert
	     this to an access to .data section */
	  /* This is done through a change to the output section of
	     the symbol.. */
	  if (amiga_base_relative
	      && !strcmp(target_section->output_section->name,".bss"))
	    {
	      /* get value for .data section */
	      bfd *ibfd;
	      sec_ptr s;

	      ibfd=target_section->output_section->owner;
	      for (s=ibfd->sections;s!=NULL;s=s->next)
		if (!strcmp(s->name,".data"))
		  {
		    target_section->output_offset+=s->_raw_size;
		    target_section->output_section=s;
		  }
	    }
	  relocation=0;
	  copy=TRUE;
	}
      DPRINT(10,("target->out=%s(%lx), sec->out=%s(%lx), symbol=%s\n",
		 target_section->output_section->name,
		 target_section->output_section,sec->output_section->name,
		 sec->output_section,sym->name));
      break;

    case H_PC8: /* pcrel */
    case H_PC16:
    case H_PC32:
      if (bfd_is_abs_section(target_section)) /* Ref to absolute hunk */
	relocation=sym->value;
      else
	{
	  relocation = sym->value + target_section->output_offset
	    - sec->output_offset;
	}
      break;

    case H_SD16: /* baserel */
    case H_SD32:
    baserel:
      /* We use the symbol ___a4_init as base */
      if (bfd_is_abs_section(target_section))
	relocation=sym->value;
      else if (bfd_is_com_section(target_section)) /* Error.. */
	{
	  bfd_msg ("baserelative relocation to common \"%s\"",sym->name);
	  DPRINT(10,("Ref to common symbol...aout_perf_reloc\n"));
	  ret=bfd_reloc_undefined;
	}
      else if (!AMIGA_DATA(target_section->output_section->owner)->baserel)
	{
	  bfd_msg ("Base symbol for base relative reloc not defined: "
		   "section %s, reloc to symbol %s",sec->name,sym->name);
	  ret=bfd_reloc_notsupported;
	}
      else if ((target_section->flags&SEC_CODE)!=0)
        {
	  bfd_msg ("%s: baserelative text relocation to \"%s\"",
		    abfd->filename, sym->name);
	  ret=bfd_reloc_notsupported;
        }
      else /* Target section and sec need not be the same.. */
	{
	  /* If target->out is .bss, add the value of the .data section to
	     sym->value and set new output_section */
	  if (!strcmp(target_section->output_section->name,".bss"))
	    {
	      bfd *ibfd;
	      sec_ptr s;

	      ibfd=target_section->output_section->owner;
	      for (s=ibfd->sections;s!=NULL;s=s->next)
		if (!strcmp(s->name,".data"))
		  {
		    target_section->output_offset+=s->_raw_size;
		    target_section->output_section=s;
		  }
	    }

	  relocation = sym->value + target_section->output_offset
	    - (AMIGA_DATA(target_section->output_section->owner))->a4init;
	  /* if the symbol is in .bss, subtract the offset that gas has put
	     into the opcode */
	  if (target_section->index == 2)
	    relocation -= adata(abfd).datasec->_raw_size;
	  DPRINT(20,("symbol=%s (0x%lx)\nsection %s (0x%lx; %s; output=0x%lx)"
		     "\nrelocation @0x%lx\n", sym->name, sym->value,
		     target_section->name, target_section,
		     target_section->owner->filename, target_section->output_offset,
		     r->address));
	  flags|=ADDEND_UNSIGNED;
	}
      DPRINT(10,("target->out=%s(%lx), sec->out=%s(%lx), symbol=%s\n",
		 target_section->output_section->name,
		 target_section->output_section,sec->output_section->name,
		 sec->output_section,sym->name));
      break;

    default:
      bfd_msg ("Error: unsupported reloc: %s(%d)",r->howto->name,r->howto->size);
      ret=bfd_reloc_notsupported;
      break;
    }/* Of switch */

  /* Add in relocation */
  if (relocation!=0)
    ret = my_add_to (r, data, relocation, flags);

  if (copy) /* Copy reloc to output section */
    {
      DPRINT(5,("Copying reloc\n"));
      target_section=sec->output_section;
      r->address+=sec->output_offset;
      target_section->orelocation[target_section->reloc_count++]=r;
    }
  DPRINT(5,("Leaving aout_perf_reloc with %d (OK=%d)\n",ret,bfd_reloc_ok));
  return ret;
}


/* The final link routine, used both by Amiga and a.out backend */
/* This is nearly a copy of linker.c/_bfd_generic_final_link */
bfd_boolean
amiga_final_link (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  bfd *sub;
  asection *o;
  struct bfd_link_order *p;
  size_t outsymalloc;
  struct generic_write_global_symbol_info wginfo;
  struct bfd_link_hash_entry *h =
    bfd_link_hash_lookup (info->hash, "___a4_init", FALSE, FALSE, TRUE);

  if (amiga_base_relative && h && h->type == bfd_link_hash_defined) {
    AMIGA_DATA(abfd)->baserel = TRUE;
    AMIGA_DATA(abfd)->a4init = h->u.def.value;
  }
  else
    AMIGA_DATA(abfd)->baserel = FALSE;

  DPRINT(5,("Entering final_link\n"));

  if (bfd_get_flavour (abfd) == bfd_target_aout_flavour)
    return aout_amiga_final_link (abfd, info);

  bfd_get_outsymbols (abfd) = (asymbol **) NULL;
  bfd_get_symcount (abfd) = 0;
  outsymalloc = 0;

  /* Mark all sections which will be included in the output file.  */
  for (o = abfd->sections; o != NULL; o = o->next)
    for (p = o->link_order_head; p != NULL; p = p->next)
      if (p->type == bfd_indirect_link_order)
	p->u.indirect.section->linker_mark = TRUE;

  /* Build the output symbol table.  */
  for (sub = info->input_bfds; sub != (bfd *) NULL; sub = sub->link_next)
    if (! _bfd_generic_link_output_symbols (abfd, sub, info, &outsymalloc))
      return FALSE;

  DPRINT(10,("Did build output symbol table\n"));

  /* Accumulate the global symbols.  */
  wginfo.info = info;
  wginfo.output_bfd = abfd;
  wginfo.psymalloc = &outsymalloc;
  _bfd_generic_link_hash_traverse (_bfd_generic_hash_table (info),
				   _bfd_generic_link_write_global_symbol,
				   (PTR) &wginfo);

  DPRINT(10,("Accumulated global symbols\n"));

  DPRINT(10,("Output bfd is %s(%lx)\n",abfd->filename,abfd));

  if (1)
    {
      /* Allocate space for the output relocs for each section.  */
      /* We also handle base-relative linking special, by setting the .data
	 sections real length to it's length + .bss length */
      /* This is different to bfd_generic_final_link: We ALWAYS alloc space
	 for the relocs, because we may need it anyway */
      for (o = abfd->sections;
	   o != (asection *) NULL;
	   o = o->next)
	{
	  /* If section is .data, find .bss and add that length */
	  if (!info->relocateable && amiga_base_relative &&
	      !strcmp(o->name,".data"))
	    {
	      if (bfd_get_flavour(abfd)!=bfd_target_amiga_flavour) /* oops */
		{
		  bfd_msg ("You can't use base relative linking with "
			   "partial links.");
		}
	      else if (0) /* XXX */
		{
		  asection *act_sec;
		  for (act_sec=abfd->sections; act_sec!=NULL;act_sec=act_sec->next)
		    if (!strcmp(act_sec->name,".bss"))
		      amiga_per_section(o)->disk_size = o->_raw_size +
			act_sec->_raw_size;
		}
	    }/* Of base-relative linking */

	  DPRINT(10,("Section in output bfd is %s (%lx)\n",o->name,o));

	  o->reloc_count = 0;
	  for (p = o->link_order_head;
	       p != (struct bfd_link_order *) NULL;
	       p = p->next)
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

		  DPRINT(10,("\tIndirect section from bfd %s, section is %s(%lx) "
			     "(COM=%lx)\n",
			     input_bfd->filename,input_section->name,input_section,
			     bfd_com_section_ptr));

		  relsize = bfd_get_reloc_upper_bound (input_bfd,
						       input_section);
		  if (relsize < 0)
		    {
		      DPRINT(10,("Relsize<0.I..in bfd %s, sec %s\n",
				 input_bfd->filename, input_section->name));
		      return FALSE;
		    }
		  relocs = (arelent **) bfd_malloc ((bfd_size_type) relsize);
		  if (!relocs && relsize != 0)
		    return FALSE;
		  symbols = _bfd_generic_link_get_symbols (input_bfd);
		  reloc_count = bfd_canonicalize_reloc (input_bfd,
							input_section,
							relocs,
							symbols);
		  free (relocs);
		  if (reloc_count < 0)
		    {
		      DPRINT(10,("Relsize<0.II..in bfd %s, sec %s\n",
				 input_bfd->filename, input_section->name));
		      return FALSE;
		    }
		  BFD_ASSERT ((unsigned long) reloc_count
			      == input_section->reloc_count);
		  o->reloc_count += reloc_count;
		}
	    }
	  if (o->reloc_count > 0)
	    {
	      bfd_size_type amt;

	      amt = o->reloc_count;
	      amt *= sizeof (arelent *);
	      o->orelocation = (arelent **) bfd_alloc (abfd, amt);
	      if (!o->orelocation)
		return FALSE;
	      /* o->flags |= SEC_RELOC; There may be no relocs. This can
		 be determined later only */
	      /* Reset the count so that it can be used as an index
		 when putting in the output relocs.  */
	      o->reloc_count = 0;
	    }
	}
    }

  DPRINT(10,("Got all relocs\n"));

  /* Handle all the link order information for the sections.  */
  for (o = abfd->sections;
       o != (asection *) NULL;
       o = o->next)
    {
      for (p = o->link_order_head;
	   p != (struct bfd_link_order *) NULL;
	   p = p->next)
	{
	  switch (p->type)
	    {
	    case bfd_section_reloc_link_order:
	    case bfd_symbol_reloc_link_order:
	      if (! amiga_reloc_link_order (abfd, info, o, p)) /* We use an own routine */
		return FALSE;
	      break;
	    case bfd_indirect_link_order:
	      if (! default_indirect_link_order (abfd, info, o, p, FALSE))
		/* Calls our get_relocated_section_contents */
		return FALSE;
	      break;
	    default:
	      if (! _bfd_default_link_order (abfd, info, o, p))
		return FALSE;
	      break;
	    }
	}
    }

  if (bfd_get_flavour(abfd)==bfd_target_amiga_flavour&&!info->relocateable)
    AMIGA_DATA(abfd)->IsLoadFile = TRUE;

  DPRINT(10,("Leaving final_link\n"));
  return TRUE;
}


/* Handle reloc link order.
   This is nearly a copy of linker.c/_bfd_generic_reloc_link_order */
static bfd_boolean
amiga_reloc_link_order (abfd, info, sec, link_order)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     struct bfd_link_order *link_order;
{
  arelent *r;

  DPRINT(5,("Entering amiga_reloc_link_order\n"));

  if (sec->orelocation == (arelent **) NULL)
    {
      DPRINT(10,("aborting, since orelocation==NULL\n"));
      abort ();
    }

  /* We generate a new ***AMIGA*** style reloc */
  r = (arelent *) bfd_zalloc (abfd, (bfd_size_type) sizeof (amiga_reloc_type));
  if (r == (arelent *) NULL)
    {
      DPRINT(5,("Leaving amiga_reloc_link, no mem\n"));
      return FALSE;
    }

  r->address = link_order->offset;
  r->howto = bfd_reloc_type_lookup (abfd, link_order->u.reloc.p->reloc);
  if (r->howto == 0)
    {
      bfd_set_error (bfd_error_bad_value);
      DPRINT(5,("Leaving amiga_reloc_link, bad value\n"));
      return FALSE;
    }

  /* Get the symbol to use for the relocation.  */
  if (link_order->type == bfd_section_reloc_link_order)
    r->sym_ptr_ptr = link_order->u.reloc.p->u.section->symbol_ptr_ptr;
  else
    {
      struct generic_link_hash_entry *h;

      h = ((struct generic_link_hash_entry *)
	   bfd_wrapped_link_hash_lookup (abfd, info,
					 link_order->u.reloc.p->u.name,
					 FALSE, FALSE, TRUE));
      if (h == (struct generic_link_hash_entry *) NULL
	  || ! h->written)
	{
	  if (! ((*info->callbacks->unattached_reloc)
		 (info, link_order->u.reloc.p->u.name,
		  (bfd *) NULL, (asection *) NULL, (bfd_vma) 0)))
	    return FALSE;
	  bfd_set_error (bfd_error_bad_value);
	  DPRINT(5,("Leaving amiga_reloc_link, bad value in hash lookup\n"));
	  return FALSE;
	}
      r->sym_ptr_ptr = &h->sym;
    }
  DPRINT(5,("Got symbol for relocation\n"));
  /* Store the addend */
  r->addend = link_order->u.reloc.p->addend;

  /* If we are generating relocateable output, just add the reloc */
  if (info->relocateable)
    {
      DPRINT(5,("Adding reloc\n"));
      sec->orelocation[sec->reloc_count] = r;
      ++sec->reloc_count;
      sec->flags|=SEC_RELOC;
    }
  else /* Try to apply the reloc */
    {
      PTR data=(PTR)sec->contents;
      bfd_reloc_status_type ret;
      char *em=NULL;

      DPRINT(5,("Apply link_order_reloc\n"));

      /* FIXME: Maybe, we have to get the section contents, before we
	  use them, if they have not been set by now.. */
      BFD_ASSERT (data!=NULL);

      if (bfd_get_flavour(abfd)==bfd_target_amiga_flavour)
	ret=amiga_perform_reloc(abfd,r,data,sec,NULL,&em);
      else
	ret=aout_perform_reloc(abfd,r,data,sec,NULL,&em);

      if (ret!=bfd_reloc_ok)
	{
	  DPRINT(5,("Leaving amiga_reloc_link, value FALSE\n"));
	  return FALSE;
	}
    }
  DPRINT(5,("Leaving amiga_reloc_link\n"));
  return TRUE;
}
