/* Configuration for GNU C-compiler for m68k Amiga, running AmigaOS.
   Copyright (C) 1992, 93-96, 1997 Free Software Foundation, Inc.
   Contributed by Markus M. Wild (wild@amiga.physik.unizh.ch).
   Heavily modified by Kamil Iskra (iskra@student.uci.agh.edu.pl).

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "rtl.h"
#include "output.h"
#include "tree.h"
#include "flags.h"
#include "expr.h"

/* Baserel support.  */

/* Does operand (which is a symbolic_operand) live in text space? If
   so SYMBOL_REF_FLAG, which is set by ENCODE_SECTION_INFO, will be true.

   This function is used in base relative code generation. */

int
read_only_operand (operand)
     rtx operand;
{
  if (GET_CODE (operand) == CONST)
    operand = XEXP (XEXP (operand, 0), 0);
  if (GET_CODE (operand) == SYMBOL_REF)
    return SYMBOL_REF_FLAG (operand) || CONSTANT_POOL_ADDRESS_P (operand);
  return 1;
}

/* This function is used while generating a base relative code.
   It returns 1 if a decl is not relocatable, i. e., if it can be put
   in the text section.
   Currently, it's very primitive: it just checks if the object size
   is less than 4 bytes (i. e., if it can hold a pointer).  It also
   supports arrays and floating point types.  */

int
amigaos_put_in_text (decl)
     tree decl;
{
  tree type = TREE_TYPE (decl);
  if (TREE_CODE (type) == ARRAY_TYPE)
    type = TREE_TYPE (type);
  return (TREE_INT_CST_HIGH (TYPE_SIZE (type)) == 0
	  && TREE_INT_CST_LOW (TYPE_SIZE (type)) < 32)
	 || FLOAT_TYPE_P (type);
}

/* Common routine used to check if a4 should be preserved/restored.  */

int
amigaos_restore_a4 ()
{
  return (flag_pic >= 3 &&
	  (TARGET_RESTORE_A4 || TARGET_ALWAYS_RESTORE_A4
	   || lookup_attribute ("saveds",
				TYPE_ATTRIBUTES (TREE_TYPE (current_function_decl)))));
}

void
amigaos_alternate_pic_setup (stream)
     FILE *stream;
{
  if (TARGET_RESTORE_A4 || TARGET_ALWAYS_RESTORE_A4)
    asm_fprintf (stream, "\tjbsr %U__restore_a4\n");
  else if (lookup_attribute ("saveds",
			     TYPE_ATTRIBUTES (TREE_TYPE (current_function_decl))))
    asm_fprintf (stream, "\tlea %U__a4_init,%Ra4\n");
}

/* Attributes support.  */

#define AMIGA_CHIP_SECTION_NAME ".datachip"

/* Return nonzero if IDENTIFIER with arguments ARGS is a valid machine
   specific attribute for DECL.  The attributes in ATTRIBUTES have previously
   been assigned to DECL. */

int
valid_amigaos_decl_attribute (decl, identifier)
     tree decl, identifier;
{
  if (TREE_CODE (decl) == VAR_DECL)
    if (is_attribute_p ("chip", identifier))
#ifdef ASM_OUTPUT_SECTION_NAME
      {
	if (! TREE_STATIC (decl) && ! DECL_EXTERNAL (decl))
	  error ("`chip' attribute cannot be specified for local variables");
	else
	  {
	    /* The decl may have already been given a section attribute from
	       a previous declaration.  Ensure they match.  */
	    if (DECL_SECTION_NAME (decl) == NULL_TREE)
	      DECL_SECTION_NAME (decl) =
		build_string (strlen (AMIGA_CHIP_SECTION_NAME) + 1,
			      AMIGA_CHIP_SECTION_NAME);
	    else if (strcmp (TREE_STRING_POINTER (DECL_SECTION_NAME (decl)),
			     AMIGA_CHIP_SECTION_NAME) != 0)
	      {
		error_with_decl (decl,
			"`chip' for `%s' conflicts with previous declaration");
		return 0;
	      }
	    return 1;
	  }
      }
#else
      error ("`chip' attribute is not supported for this target");
#endif

  return 0;
}

/* Return nonzero if IDENTIFIER with arguments ARGS is a valid machine
   specific attribute for TYPE.  The attributes in ATTRIBUTES have previously
   been assigned to TYPE.  */

int
valid_amigaos_type_attribute (type, attributes, identifier, args)
     tree type, attributes, identifier, args;
{
  if (valid_m68k_type_attribute (type, attributes, identifier, args))
    return 1;
  else if (TREE_CODE (type) == FUNCTION_TYPE || TREE_CODE (type) == METHOD_TYPE)
    {
      if (is_attribute_p ("stackext", identifier))
	{
	  if (lookup_attribute ("interrupt", attributes))
	    {
	      error ("`stackext' and `interrupt' are mutually exclusive");
	      return 0;
	    }
	  return 1;
	}
      if (is_attribute_p ("interrupt", identifier))
	{
	  if (lookup_attribute ("stackext", attributes))
	    {
	      error ("`stackext' and `interrupt' are mutually exclusive");
	      return 0;
	    }
	  return 1;
	}
      if (is_attribute_p ("saveds", identifier))
	return 1;
    }

  return 0;
}

/* Stack checking and automatic extension support.  */

void
amigaos_prologue_begin_hook (stream, fsize)
     FILE *stream;
     int fsize;
{
  if (TARGET_STACKCHECK)
    {
      if (fsize < 256)
	asm_fprintf (stream, "\tcmpl %s,%Rsp\n"
			     "\tjcc 0f\n"
			     "\tjra %U__stkovf\n"
			     "\t0:\n",
		     (flag_pic == 3 ? "a4@(___stk_limit:W)" :
				      (flag_pic == 4 ? "a4@(___stk_limit:L)" :
						       "___stk_limit")));
      else if (fsize < 0x8000)
	asm_fprintf (stream, "\tmovel #%d:W,%Rd0\n\tjbsr %U__stkchk_d0\n",
		     fsize);
      else
	asm_fprintf (stream, "\tmovel #%d,%Rd0\n\tjbsr %U__stkchk_d0\n",
		     fsize);
    }
}

void
amigaos_alternate_frame_setup_f (stream, fsize)
     FILE *stream;
     int fsize;
{
  if (fsize < 128)
    asm_fprintf (stream, "\tcmpl %s,%Rsp\n"
			 "\tjcc 0f\n"
			 "\tmoveq #%d,%Rd0\n"
			 "\tmoveq #0,%Rd1\n"
			 "\tjbsr %U__stkext_f\n"
			 "0:\tlink %Ra5,#%d:W\n",
		 (flag_pic == 3 ? "a4@(___stk_limit:W)" :
				  (flag_pic == 4 ? "a4@(___stk_limit:L)" :
						   "___stk_limit")),
		 fsize, -fsize);
  else if (fsize < 0x8000)
    asm_fprintf (stream, "\tmovel #%d:W,%Rd0\n\tjbsr %U__link_a5_d0_f\n",
		 fsize);
  else
    asm_fprintf (stream, "\tmovel #%d,%Rd0\n\tjbsr %U__link_a5_d0_f\n",
		 fsize);
}

void
amigaos_alternate_frame_setup (stream, fsize)
     FILE *stream;
     int fsize;
{
  if (! fsize)
    asm_fprintf (stream, "\tcmpl %s,%Rsp\n"
			 "\tjcc 0f\n"
			 "\tmoveq #0,%Rd0\n"
			 "\tmoveq #0,%Rd1\n"
			 "\tjbsr %U__stkext_f\n"
			 "0:\n",
		 (flag_pic == 3 ? "a4@(___stk_limit:W)" :
				  (flag_pic == 4 ? "a4@(___stk_limit:L)" :
						   "___stk_limit")));
  else if (fsize + 4 < 128)
    asm_fprintf (stream, "\tcmpl %s,%Rsp\n"
			 "\tjcc 0f\n"
			 "\tmoveq #%d,%Rd0\n"
			 "\tmoveq #0,%Rd1\n"
			 "\tjbsr %U__stkext_f\n"
			 "0:\taddw #%d,%Rsp\n",
		 (flag_pic == 3 ? "a4@(___stk_limit:W)" :
				  (flag_pic == 4 ? "a4@(___stk_limit:L)" :
						   "___stk_limit")),
		 fsize + 4, -(fsize + 4));
  else if (fsize + 4 < 0x8000)
    asm_fprintf (stream, "\tmovel #%d:W,%Rd0\n\tjbsr %U__sub_d0_sp_f\n",
		 fsize + 4);
  else
    asm_fprintf (stream, "\tmovel %0I%d,%Rd0\n\tjbsr %U__sub_d0_sp_f\n",
		 fsize + 4);
}

static rtx
gen_stack_management_call (stack_pointer, arg, func)
     rtx stack_pointer;
     rtx arg;
     char *func;
{
  rtx call_insn, call, seq, name;
  start_sequence ();

  /* Move arg to d0.  */
  emit_move_insn (gen_rtx (REG, SImode, 0), arg);

  /* Generate the function reference.  */
  name = gen_rtx (SYMBOL_REF, Pmode, func);
  SYMBOL_REF_FLAG (name) = 1;
  /* If optimizing, put it in a psedo so that several loads can be merged
     into one.  */
  if (optimize && ! flag_no_function_cse)
    name = copy_to_reg (name);

  /* Generate the function call.  */
  call = gen_rtx (CALL, VOIDmode, gen_rtx (MEM, FUNCTION_MODE, name),
		  const0_rtx);
  /* If we are doing stack extension, notify about the sp change.  */
  if (stack_pointer)
    call = gen_rtx (SET, VOIDmode, stack_pointer, call);

  /* Generate the call instruction.  */
  call_insn = emit_call_insn (call);
  /* Stack extension does not change memory in an unpredictable way.  */
  CONST_CALL_P (call_insn) = 1;
  /* We pass an argument in d0.  */
  CALL_INSN_FUNCTION_USAGE (call_insn) = gen_rtx (EXPR_LIST, VOIDmode,
	gen_rtx (USE, VOIDmode, gen_rtx (REG, SImode, 0)), 0);

  seq = gen_sequence ();
  end_sequence ();
  return seq;
}

rtx
gen_stack_cleanup_call (stack_pointer, sa)
     rtx stack_pointer;
     rtx sa;
{
  return gen_stack_management_call (stack_pointer, sa, "__move_d0_sp");
}

void
amigaos_alternate_allocate_stack (operands)
     rtx *operands;
{
  if (TARGET_STACKEXTEND)
    emit_insn ((rtx)gen_stack_management_call (stack_pointer_rtx, operands[1],
					       "__sub_d0_sp"));
  else
    {
      if (TARGET_STACKCHECK)
	emit_insn ((rtx)gen_stack_management_call (0, operands[1],
						   "__stkchk_d0"));
      anti_adjust_stack (operands[1]);
    }
  emit_move_insn (operands[0], virtual_stack_dynamic_rtx);
}

/* begin-GG-local: explicit register specification for parameters */

/* Initialize a variable CUM of type CUMULATIVE_ARGS
   for a call to a function whose data type is FNTYPE.
   For a library call, FNTYPE is 0.  */

void
amigaos_init_cumulative_args(CUMULATIVE_ARGS *cum, tree fntype)
{
  m68k_init_cumulative_args(cum, fntype);

  if (fntype)
    cum->formal_type=TYPE_ARG_TYPES(fntype);
  else /* Call to compiler-support function. */
    cum->formal_type=0;
}

/* Update the data in CUM to advance over an argument.  */

void
amigaos_function_arg_advance(CUMULATIVE_ARGS *cum)
{
  m68k_function_arg_advance(cum);

  if (cum->formal_type)
    cum->formal_type=TREE_CHAIN((tree)cum->formal_type);
}

/* A C expression that controls whether a function argument is passed
   in a register, and which register. */

struct rtx_def *
amigaos_function_arg(CUMULATIVE_ARGS *cum, enum machine_mode mode,
  tree type)
{
  tree asmtree;
  if (cum->formal_type && TREE_VALUE((tree)cum->formal_type)
      && (asmtree=lookup_attribute("asm",
			TYPE_ATTRIBUTES(TREE_VALUE((tree)cum->formal_type)))))
    {
      int i;
#if 0
      /* See c-decl.c/push_parm_decl for an explanation why this doesn't work.
       */
      cum->last_arg_reg=TREE_INT_CST_LOW(TREE_VALUE(TREE_VALUE(asmtree)));
#else
      cum->last_arg_reg=TREE_INT_CST_LOW(TREE_VALUE(asmtree));
#endif
      cum->last_arg_len=HARD_REGNO_NREGS(cum->last_arg_reg, mode);

      for (i=0; i<cum->last_arg_len; i++)
	if (cum->regs_already_used & (1 << cum->last_arg_reg+i))
	  {
	    error("two parameters allocated for one register");
	    break;
	  }
      return gen_rtx(REG, mode, cum->last_arg_reg);
    }
  else
    return (struct rtx_def *)m68k_function_arg(cum, mode, type);
}

/* Return zero if the attributes on TYPE1 and TYPE2 are incompatible,
   one if they are compatible, and two if they are nearly compatible
   (which causes a warning to be generated). */

int
comp_amigaos_type_attributes(tree type1, tree type2)
{
  int ret;
  if ((ret=comp_m68k_type_attributes(type1, type2))!=1)
    return ret;

  /* Functions or methods are incompatible if they specify mutually exclusive
     ways of passing arguments. */
  if (TREE_CODE(type1)==FUNCTION_TYPE || TREE_CODE(type1)==METHOD_TYPE)
    {
      tree arg1, arg2;
      arg1=TYPE_ARG_TYPES(type1);
      arg2=TYPE_ARG_TYPES(type2);
      for (; arg1 && arg2; arg1=TREE_CHAIN(arg1), arg2=TREE_CHAIN(arg2))
	if (TREE_VALUE(arg1) && TREE_VALUE(arg2))
	  {
	    tree asm1, asm2;
	    asm1=lookup_attribute("asm", TYPE_ATTRIBUTES(TREE_VALUE(arg1)));
	    asm2=lookup_attribute("asm", TYPE_ATTRIBUTES(TREE_VALUE(arg2)));
	    if (asm1 && asm2)
	      {
		if (TREE_INT_CST_LOW(TREE_VALUE(asm1))!=
		    TREE_INT_CST_LOW(TREE_VALUE(asm2)))
		  return 0; /* Two different registers specified. */
	      }
	    else
	      if (asm1 || asm2)
		return 0; /* "asm" used in only one type. */
	  }
    }
  return 1;
}

/* end-GG-local */
