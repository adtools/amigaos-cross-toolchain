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

extern enum rs6000_sdata_type rs6000_sdata;

/* Common routine used to check if r13 should be preserved/restored.  */

int
morphos_restore_r13 ()
{
    return ((rs6000_sdata == SDATA_BREL || rs6000_sdata == SDATA_DATA) &&
	    (TARGET_RESTORE_R13 || TARGET_ALWAYS_RESTORE_R13
	     || lookup_attribute ("saveds",
				  TYPE_ATTRIBUTES (TREE_TYPE (current_function_decl)))));
}

/* Return nonzero if IDENTIFIER with arguments ARGS is a valid machine
   specific attribute for TYPE.  The attributes in ATTRIBUTES have previously
   been assigned to TYPE.  */

int
morphos_valid_type_attribute_p (type, attributes, identifier, args)
     tree type, attributes, identifier, args;
{
  if (TREE_CODE (type) == FUNCTION_TYPE || TREE_CODE (type) == METHOD_TYPE)
    {
      if (is_attribute_p ("stackext", identifier) ||
	  is_attribute_p ("saveds", identifier) ||
	  is_attribute_p ("varargs68k", identifier))
	return 1;
    }
  return rs6000_valid_type_attribute_p (type, attributes, identifier, args);
}



/* stack check/extension code */

void morphos_output_prolog(file, size)
     FILE *file;
     int size;
{
  if (TARGET_STACKCHECK || TARGET_STACKEXTEND)
      asm_fprintf(file, "\tmflr 0\n\tbl __stk%s1\n", 
		  TARGET_STACKEXTEND ? "ext" : "chk");
  output_prolog(file, size);
}


/* Initialize a variable CUM of type CUMULATIVE_ARGS
   for a call to a function whose data type is FNTYPE.
   For a library call, FNTYPE is 0.

   For incoming args we set the number of arguments in the prototype large
   so we never return a PARALLEL.  */

void
morphos_init_cumulative_args (cum, fntype, libname, incoming)
     CUMULATIVE_ARGS *cum;
     tree fntype;
     rtx libname ATTRIBUTE_UNUSED;
     int incoming;
{
  init_cumulative_args (cum, fntype, libname, incoming);
  if (fntype && lookup_attribute ("varargs68k", TYPE_ATTRIBUTES (fntype)))
    cum->call_cookie |= CALL_VARARGS68K;
}


/* Determine where to put an argument to a function.
   Value is zero to push the argument on the stack,
   or a hard register in which to store the argument.

   MODE is the argument's machine mode.
   TYPE is the data type of the argument (as a tree).
    This is null for libcalls where that information may
    not be available.
   CUM is a variable of type CUMULATIVE_ARGS which gives info about
    the preceding args and about the function being called.
   NAMED is nonzero if this argument is a named parameter
    (otherwise it is an extra parameter matching an ellipsis).

   On RS/6000 the first eight words of non-FP are normally in registers
   and the rest are pushed.  Under AIX, the first 13 FP args are in registers.
   Under V.4, the first 8 FP args are in registers.

   If this is floating-point and no prototype is specified, we use
   both an FP and integer register (or possibly FP reg and stack).  Library
   functions (when TYPE is zero) always have the proper types for args,
   so we can pass the FP value just in one register.  emit_library_function
   doesn't support PARALLEL anyway.  */

struct rtx_def *
morphos_function_arg (cum, mode, type, named)
     CUMULATIVE_ARGS *cum;
     enum machine_mode mode;
     tree type;
     int named;
{
  /* Return a marker to indicate whether CR1 needs to set or clear the bit
     that V.4 uses to say fp args were passed in registers.  Assume that we
     don't need the marker for software floating point, or compiler generated
     library calls.  */
  if (mode == VOIDmode && cum->call_cookie & CALL_VARARGS68K)
    return GEN_INT (cum->call_cookie);

  return function_arg (cum, mode, type, named);
}


/* Update the data in CUM to advance over an argument
   of mode MODE and data type TYPE.
   (TYPE is null for libcalls where that information may not be available.)  */

void
morphos_function_arg_advance (cum, mode, type, named)
     CUMULATIVE_ARGS *cum;
     enum machine_mode mode;
     tree type;
     int named;
{
  function_arg_advance(cum, mode, type, named);
  if (cum->call_cookie & CALL_VARARGS68K &&
      cum->sysv_gregno + cum->fregno - GP_ARG_MIN_REG - FP_ARG_MIN_REG > cum->orig_nargs)
    {
      cum->sysv_gregno = GP_ARG_MAX_REG + 1;
      cum->fregno = FP_ARG_V4_MAX_REG + 1;
    }
}


/* Perform any needed actions needed for a function that is receiving a
   variable number of arguments.

   CUM is as above.

   MODE and TYPE are the mode and type of the current parameter.

   PRETEND_SIZE is a variable that should be set to the amount of stack
   that must be pushed by the prolog to pretend that our caller pushed
   it.

   Normally, this macro will push all remaining incoming registers on the
   stack and set PRETEND_SIZE to the length of the registers pushed.  */

void
morphos_setup_incoming_varargs (cum, mode, type, pretend_size, no_rtl)
     CUMULATIVE_ARGS *cum;
     enum machine_mode mode;
     tree type;
     int *pretend_size;
     int no_rtl;

{
  CUMULATIVE_ARGS next_cum;
  int reg_size = TARGET_32BIT ? 4 : 8;
  rtx save_area;
  int first_reg_offset;
  tree fntype;
  int stdarg_p;

  if (!(cum->call_cookie & CALL_VARARGS68K)) {
    setup_incoming_varargs (cum, mode, type, pretend_size, no_rtl);
    return;
  }

  fntype = TREE_TYPE (current_function_decl);
  stdarg_p = (TYPE_ARG_TYPES (fntype) != 0
	      && (TREE_VALUE (tree_last (TYPE_ARG_TYPES (fntype)))
		  != void_type_node));

  /* For varargs, we do not want to skip the dummy va_dcl argument.
     For stdargs, we do want to skip the last named argument.  */
  next_cum = *cum;
  if (stdarg_p)
    function_arg_advance (&next_cum, mode, type, 1);

  /* Indicate to allocate space on the stack for varargs save area.  */
  /* ??? Does this really have to be located at a magic spot on the
     stack, or can we allocate this with assign_stack_local instead.  */
  rs6000_sysv_varargs_p = 1;
  if (! no_rtl)
    save_area = plus_constant (virtual_stack_vars_rtx,
			       - RS6000_VARARGS_SIZE);

  first_reg_offset = next_cum.sysv_gregno - GP_ARG_MIN_REG;
}


/* If defined, is a C expression that produces the machine-specific
   code for a call to `__builtin_saveregs'.  This code will be moved
   to the very beginning of the function, before any parameter access
   are made.  The return value of this function should be an RTX that
   contains the value to use as the return of `__builtin_saveregs'.

   The argument ARGS is a `tree_list' containing the arguments that
   were passed to `__builtin_saveregs'.

   If this macro is not defined, the compiler will output an ordinary
   call to the library function `__builtin_saveregs'.

   On the Power/PowerPC return the address of the area on the stack
   used to hold arguments.  Under AIX, this includes the 8 word register
   save area.

   Under V.4, things are more complicated.  We do not have access to
   all of the virtual registers required for va_start to do its job,
   so we construct the va_list in its entirity here, and reduce va_start
   to a block copy.  This is similar to the way we do things on Alpha.  */

struct rtx_def *
morphos_expand_builtin_saveregs (args)
     tree args ATTRIBUTE_UNUSED;
{
  rtx block, mem_gpr_fpr, /*mem_reg_save_area, */mem_overflow, tmp;
  tree fntype;
  int stdarg_p;
  HOST_WIDE_INT words, gpr, bits;

  if (!(current_function_args_info.call_cookie & CALL_VARARGS68K))
    return expand_builtin_saveregs (args);

  fntype = TREE_TYPE (current_function_decl);
  stdarg_p = (TYPE_ARG_TYPES (fntype) != 0
	      && (TREE_VALUE (tree_last (TYPE_ARG_TYPES (fntype)))
		  != void_type_node));

  /* Allocate the va_list constructor.  */
  block = assign_stack_local (BLKmode, 3 * UNITS_PER_WORD, BITS_PER_WORD);
  RTX_UNCHANGING_P (block) = 1;
  RTX_UNCHANGING_P (XEXP (block, 0)) = 1;

  mem_gpr_fpr = change_address (block, word_mode, XEXP (block, 0));
  mem_overflow = change_address (block, ptr_mode,
			         plus_constant (XEXP (block, 0),
						UNITS_PER_WORD));
  /*mem_reg_save_area = change_address (block, ptr_mode,
				      plus_constant (XEXP (block, 0),
						     2 * UNITS_PER_WORD));*/

  /* Construct the two characters of `gpr' and `fpr' as a unit.  */
  words = current_function_args_info.words;
  gpr = current_function_args_info.sysv_gregno - GP_ARG_MIN_REG;

  /* Varargs has the va_dcl argument, but we don't count it.  */
  if (!stdarg_p)
    {
      if (gpr > GP_ARG_NUM_REG)
        words -= 1;
    }

  bits = (GP_ARG_MAX_REG - GP_ARG_MIN_REG + 1) << 8
       | (FP_ARG_MAX_REG - FP_ARG_MIN_REG + 1);
  if (HOST_BITS_PER_WIDE_INT >= BITS_PER_WORD)
    tmp = GEN_INT (bits << (BITS_PER_WORD - 16));
  else
    {
      bits <<= BITS_PER_WORD - HOST_BITS_PER_WIDE_INT - 16;
      tmp = immed_double_const (0, bits, word_mode);
    }

  emit_move_insn (mem_gpr_fpr, tmp);

  /* Find the overflow area.  */
  tmp = expand_binop (Pmode, add_optab, virtual_incoming_args_rtx,
		      GEN_INT (words * UNITS_PER_WORD),
		      mem_overflow, 0, OPTAB_WIDEN);
  if (tmp != mem_overflow)
    emit_move_insn (mem_overflow, tmp);

  /*tmp = expand_binop (Pmode, add_optab, virtual_stack_vars_rtx,
		      GEN_INT (-RS6000_VARARGS_SIZE),
		      mem_reg_save_area, 0, OPTAB_WIDEN);
  if (tmp != mem_reg_save_area)
    emit_move_insn (mem_reg_save_area, tmp);*/

  /* Return the address of the va_list constructor.  */
  return XEXP (block, 0);
}

