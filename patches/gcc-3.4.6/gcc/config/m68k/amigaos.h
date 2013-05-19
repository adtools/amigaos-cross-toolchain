/* Configuration for GNU C-compiler for m68k Amiga, running AmigaOS.
   Copyright (C) 1992, 1993, 1994, 1995, 1996, 1997, 1998, 2003
   Free Software Foundation, Inc.  
   Contributed by Markus M. Wild (wild@amiga.physik.unizh.ch).
   Heavily modified by Kamil Iskra (iskra@student.uci.agh.edu.pl).

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */


/* Specs, switches.  */

/* amiga/amigaos are the new "standard" defines for the Amiga.
   MCH_AMIGA, AMIGA, __chip etc. are used in other compilers and are
   provided for compatibility reasons.
   When creating shared libraries, use different 'errno'.  */

#undef TARGET_OS_CPP_BUILTINS
#define TARGET_OS_CPP_BUILTINS()					\
  do									\
    {									\
      builtin_define ("__chip=__attribute__((__chip__))");		\
      builtin_define ("__saveds=__attribute__((__saveds__))");		\
      builtin_define ("__interrupt=__attribute__((__interrupt__))");	\
      builtin_define ("__stackext=__attribute__((__stackext__))");	\
      builtin_define ("__regargs=__attribute__((__regparm__))");	\
      builtin_define ("__stdargs=__attribute__((__stkparm__))");	\
      builtin_define ("__aligned=__attribute__((__aligned__(4)))");	\
      if (target_flags & (MASK_RESTORE_A4|MASK_ALWAYS_RESTORE_A4))	\
        builtin_define ("errno=(*ixemul_errno)");			\
      builtin_define_std ("amiga");					\
      builtin_define_std ("amigaos");					\
      builtin_define_std ("AMIGA");					\
      builtin_define_std ("MCH_AMIGA");					\
      builtin_assert ("system=amigaos");				\
    }									\
  while (0)

/* Inform the program which CPU we compile for.  */

#undef TARGET_CPU_CPP_BUILTINS
#define TARGET_CPU_CPP_BUILTINS()					\
  do									\
    {									\
      if (TARGET_68040_ONLY)						\
	{								\
	  if (TARGET_68060)						\
	    builtin_define_std ("mc68060");				\
	  else								\
	    builtin_define_std ("mc68040");				\
	}								\
      else if (TARGET_68030 && !TARGET_68040)				\
	builtin_define_std ("mc68030");					\
      else if (TARGET_68020)						\
	builtin_define_std ("mc68020");					\
      builtin_define_std ("mc68000");					\
      if (flag_pic > 2)							\
	{								\
	  builtin_define ("__pic__");					\
	  if (flag_pic > 3)						\
	    builtin_define ("__PIC__");					\
	}								\
      builtin_assert ("cpu=m68k");					\
      builtin_assert ("machine=m68k");					\
    }									\
  while (0)

/* Define __HAVE_68881__ in preprocessor according to the -m flags.
   This will control the use of inline 68881 insns in certain macros.
   Note: it should be set in TARGET_CPU_CPP_BUILTINS but TARGET_68881
         isn't the same -m68881 since its also true for -m680[46]0 ...
   Differentiate between libnix and ixemul.  */

#define CPP_SPEC							\
  "%{m68881:-D__HAVE_68881__} "						\
  "%{noixemul:%{!ansi:%{!std=*:-Dlibnix}%{std=gnu*:-Dlibnix}} -D__libnix -D__libnix__} " \
  "%{!noixemul:%{!ansi:%{!std=*:-Dixemul}%{std=gnu*:-Dixemul}} -D__ixemul -D__ixemul__}"

/* Translate '-resident' to '-fbaserel' (they differ in linking stage only).
   Don't put function addresses in registers for PC-relative code.  */

#define CC1_SPEC							\
  "%{resident:-fbaserel} "						\
  "%{resident32:-fbaserel32} "						\
  "%{msmall-code:-fno-function-cse}"

/* Various -m flags require special flags to the assembler.  */

#define ASM_SPEC							\
  "%(asm_cpu) %(asm_cpu_default) %{msmall-code:-sc}"

#define ASM_CPU_SPEC							\
  "%{m68000|mc68000:-m68010} "						\
  "%{m6802*|mc68020:-m68020} "						\
  "%{m68030} "								\
  "%{m68040} "								\
  "%{m68060}"

#define ASM_CPU_DEFAULT_SPEC						\
  "%{!m680*:%{!mc680*:-m68010}}"

/* If debugging, tell the linker to output amiga-hunk symbols *and* a BSD
   compatible debug hunk.
   Also, pass appropriate linker flavours depending on user-supplied
   commandline options.  */

#define LINK_SPEC							\
  "%{noixemul:-fl libnix} "						\
  "%{resident*:-amiga-datadata-reloc} "					\
  "%{resident|fbaserel:-m amiga_bss -fl libb} "				\
  "%{resident32|fbaserel32:-m amiga_bss -fl libb32} "			\
  "%{g:-amiga-debug-hunk} "						\
  "%(link_cpu)"

#define LINK_CPU_SPEC							\
  "%{m6802*|mc68020|m68030|m68040|m68060:-fl libm020} "			\
  "%{m68881:-fl libm881}"

/* Choose the right startup file, depending on whether we use base relative
   code, base relative code with automatic relocation (-resident), their
   32-bit versions, libnix, profiling or plain crt0.o.  */

#define STARTFILE_SPEC							\
  "%{!noixemul:"							\
    "%{fbaserel:%{!resident:bcrt0.o%s}}"				\
    "%{resident:rcrt0.o%s}"						\
    "%{fbaserel32:%{!resident32:lcrt0.o%s}}"				\
    "%{resident32:scrt0.o%s}"						\
    "%{!resident:%{!fbaserel:%{!resident32:%{!fbaserel32:"		\
      "%{pg:gcrt0.o%s}%{!pg:%{p:mcrt0.o%s}%{!p:crt0.o%s}}}}}}}"		\
  "%{noixemul:"								\
    "%{resident:libnix/nrcrt0.o%s} "					\
    "%{!resident:%{fbaserel:libnix/nbcrt0.o%s}%{!fbaserel:libnix/ncrt0.o%s}}}"

#define ENDFILE_SPEC							\
  "%{noixemul:-lstubs}"

/* Automatically search libamiga.a for AmigaOS specific functions.  Note
   that we first search the standard C library to resolve as much as
   possible from there, since it has names that are duplicated in libamiga.a
   which we *don't* want from there.  Then search libamiga.a for any calls
   that were not generated inline, and finally search the standard C library
   again to resolve any references that libamiga.a might have generated.
   This may only be a temporary solution since it might be better to simply
   remove the things from libamiga.a that should be pulled in from libc.a
   instead, which would eliminate the first reference to libc.a.  Note that
   if we don't search it automatically, it is very easy for the user to try
   to put in a -lamiga himself and get it in the wrong place, so that (for
   example) calls like sprintf come from -lamiga rather than -lc. */

#define LIB_SPEC							\
  "%{!noixemul:"							\
    "%{p|pg:-lc_p}"							\
    "%{!p:%{!pg:-lc -lamiga -lc}}}"					\
  "%{noixemul:"								\
    "-lnixmain -lnix -lamiga %{mstackcheck|mstackextend:-lstack}}"

/* This macro defines names of additional specifications to put in the specs
   that can be used in various specifications like CC1_SPEC.  Its definition
   is an initializer with a subgrouping for each command option.

   Each subgrouping contains a string constant, that defines the
   specification name, and a string constant that used by the GCC driver
   program.

   Do not define this macro if it does not need to do anything.  */

#define EXTRA_SPECS							\
  { "asm_cpu",		ASM_CPU_SPEC },					\
  { "asm_cpu_default",	ASM_CPU_DEFAULT_SPEC },				\
  { "link_cpu",		LINK_CPU_SPEC }

/* Compile with stack extension.  */

#define MASK_STACKEXTEND 0x40000000 /* 1 << 30 */
#define TARGET_STACKEXTEND (((target_flags & MASK_STACKEXTEND)		\
  && !lookup_attribute ("interrupt",					\
			TYPE_ATTRIBUTES (TREE_TYPE (current_function_decl)))) \
  || lookup_attribute ("stackext",					\
		       TYPE_ATTRIBUTES (TREE_TYPE (current_function_decl))))

/* Compile with stack checking.  */

#define MASK_STACKCHECK 0x20000000 /* 1 << 29 */
#define TARGET_STACKCHECK ((target_flags & MASK_STACKCHECK)		\
  && !(target_flags & MASK_STACKEXTEND)					\
  && !lookup_attribute ("interrupt",					\
			TYPE_ATTRIBUTES (TREE_TYPE (current_function_decl))) \
  && !lookup_attribute ("stackext",					\
			TYPE_ATTRIBUTES (TREE_TYPE (current_function_decl))))

/* Compile with a4 restoring in public functions.  */

#define MASK_RESTORE_A4 0x10000000 /* 1 << 28 */
#define TARGET_RESTORE_A4						\
  ((target_flags & MASK_RESTORE_A4) && TREE_PUBLIC (current_function_decl))

/* Compile with a4 restoring in all functions.  */

#define MASK_ALWAYS_RESTORE_A4 0x8000000 /* 1 << 27 */
#define TARGET_ALWAYS_RESTORE_A4 (target_flags & MASK_ALWAYS_RESTORE_A4)

/* Provide a dummy entry for the '-msmall-code' switch.  This is used by
   the assembler and '*_SPEC'.  */

#undef SUBTARGET_SWITCHES
#define SUBTARGET_SWITCHES						\
    { "small-code", 0,							\
      "" /* Undocumented. */ },						\
    { "stackcheck", MASK_STACKCHECK,					\
      N_("Generate stack-check code") },				\
    { "no-stackcheck", - MASK_STACKCHECK,				\
      N_("Do not generate stack-check code") },				\
    { "stackextend", MASK_STACKEXTEND,					\
      N_("Generate stack-extension code") },				\
    { "no-stackextend", - MASK_STACKEXTEND,				\
      N_("Do not generate stack-extension code") },			\
    { "fixedstack", - (MASK_STACKCHECK|MASK_STACKEXTEND),		\
      N_("Do not generate stack-check/stack-extension code") },		\
    { "restore-a4", MASK_RESTORE_A4,					\
      N_("Restore a4 in public functions") },				\
    { "no-restore-a4", - MASK_RESTORE_A4,				\
      N_("Do not restore a4 in public functions") },			\
    { "always-restore-a4", MASK_ALWAYS_RESTORE_A4,			\
      N_("Restore a4 in all functions") },				\
    { "no-always-restore-a4", - MASK_ALWAYS_RESTORE_A4,			\
      N_("Do not restore a4 in all functions") },

#undef SUBTARGET_OVERRIDE_OPTIONS
#define SUBTARGET_OVERRIDE_OPTIONS					\
do									\
  {									\
    if (!TARGET_68020 && flag_pic==4)					\
      error ("-fbaserel32 is not supported on the 68000 or 68010\n");	\
  }									\
while (0)

/* Various ABI issues.  */

/* This is (almost;-) BSD, so it wants DBX format.  */

#define DBX_DEBUGGING_INFO

/* GDB goes mad if it sees the function end marker.  */

#define NO_DBX_FUNCTION_END 1

/* Allow folding division by zero.  */

#define REAL_INFINITY

/* Don't try using XFmode since we don't have appropriate runtime software
   support.  */
#undef LONG_DOUBLE_TYPE_SIZE
#define LONG_DOUBLE_TYPE_SIZE 64

/* Use A5 as framepointer instead of A6, since the AmigaOS ABI requires A6
   to be used as a shared library base pointer in direct library calls.  */

#undef FRAME_POINTER_REGNUM
#define FRAME_POINTER_REGNUM 13

/* We use A4 for the PIC pointer, not A5, which is the framepointer.  */

#undef PIC_OFFSET_TABLE_REGNUM
#define PIC_OFFSET_TABLE_REGNUM (flag_pic ? 12 : INVALID_REGNUM)

/* The AmigaOS ABI does not define how structures should be returned, so,
   contrary to 'm68k.h', we prefer a multithread-safe solution.  */

#undef PCC_STATIC_STRUCT_RETURN

/* Setup a default shell return value for those (gazillion..) programs that
   (inspite of ANSI-C) declare main() to be void (or even VOID...) and thus
   cause the shell to randomly caugh upon executing such programs (contrary
   to Unix, AmigaOS scripts are terminated with an error if a program returns
   with an error code above the `error' or even `failure' level
   (which is configurable with the FAILAT command)).  */

#define DEFAULT_MAIN_RETURN c_expand_return (integer_zero_node)

#undef WCHAR_TYPE
#define WCHAR_TYPE "unsigned int"

/* XXX: section support */
#if 0 
/* Support sections in chip memory, currently '.datachip' only.  */
#undef TARGET_ASM_NAMED_SECTION
#define TARGET_ASM_NAMED_SECTION amiga_named_section

/* We define TARGET_ASM_NAMED_SECTION, but we don't support arbitrary sections,
   including '.gcc_except_table', so we emulate the standard behaviour.  */
#undef TARGET_ASM_EXCEPTION_SECTION
#define TARGET_ASM_EXCEPTION_SECTION amiga_exception_section

#undef TARGET_ASM_EH_FRAME_SECTION
#define TARGET_ASM_EH_FRAME_SECTION amiga_eh_frame_section
#endif

/* Use sjlj exceptions until problems with DWARF2 unwind info on a.out
   targets using GNU ld are fixed.  */
/*
#define DWARF2_UNWIND_INFO	0
*/
#define NO_DWARF2_UNWIND_INFO

/* GAS supports alignment up to 32768 bytes.  */
#undef ASM_OUTPUT_ALIGN
#define ASM_OUTPUT_ALIGN(FILE, LOG)					\
do									\
  {									\
    if ((LOG) == 1)							\
      fprintf ((FILE), "\t.even\n");					\
    else								\
      fprintf ((FILE), "\t.align %d\n", (LOG));				\
  }									\
while (0)

#define MAX_OFILE_ALIGNMENT ((1 << 15)*BITS_PER_UNIT)

/* Call __flush_cache() after building the trampoline: it will call
   an appropriate OS cache-clearing routine.  */

#undef FINALIZE_TRAMPOLINE
#define FINALIZE_TRAMPOLINE(TRAMP)					\
  emit_library_call (gen_rtx_SYMBOL_REF (Pmode, "__flush_cache"),	\
		     0, VOIDmode, 2, (TRAMP), Pmode,			\
		     GEN_INT (TRAMPOLINE_SIZE), SImode)

/* Baserel support.  */

/* Given that symbolic_operand(X), return TRUE if no special
   base relative relocation is necessary */

#define LEGITIMATE_BASEREL_OPERAND_P(X)					\
  (flag_pic >= 3 && read_only_operand (X))

#undef LEGITIMATE_PIC_OPERAND_P
#define LEGITIMATE_PIC_OPERAND_P(X)					\
  (! symbolic_operand (X, VOIDmode) || LEGITIMATE_BASEREL_OPERAND_P (X))

/* Define this macro if references to a symbol must be treated
   differently depending on something about the variable or
   function named by the symbol (such as what section it is in).

   The macro definition, if any, is executed immediately after the
   rtl for DECL or other node is created.
   The value of the rtl will be a `mem' whose address is a
   `symbol_ref'.

   The usual thing for this macro to do is to a flag in the
   `symbol_ref' (such as `SYMBOL_REF_FLAG') or to store a modified
   name string in the `symbol_ref' (if one bit is not enough
   information).

   On the Amiga we use this to indicate if references to a symbol should be
   absolute or base relative.  */

#undef TARGET_ENCODE_SECTION_INFO
#define TARGET_ENCODE_SECTION_INFO amigaos_encode_section_info

#define LIBCALL_ENCODE_SECTION_INFO(FUN)				\
do									\
  {									\
    if (flag_pic >= 3)							\
      SYMBOL_REF_FLAG (FUN) = 1;					\
  }									\
while (0)

/* Select and switch to a section for EXP.  */

#undef TARGET_ASM_SELECT_SECTION
#define TARGET_ASM_SELECT_SECTION amigaos_select_section

/* Preserve A4 for baserel code if necessary.  */

#define EXTRA_SAVE_REG(REGNO)						\
do {									\
  if (flag_pic && flag_pic >= 3 && REGNO == PIC_OFFSET_TABLE_REGNUM	\
      && amigaos_restore_a4())						\
    return true;							\
} while (0)

/* Predicate for ALTERNATE_PIC_SETUP.  */

#define HAVE_ALTERNATE_PIC_SETUP (flag_pic >= 3)

/* Make a4 point at data hunk.  */

#define ALTERNATE_PIC_SETUP(STREAM)					\
  (amigaos_alternate_pic_setup (STREAM))

/* Attribute support.  */

/* Generate the test of d0 before return to set cc register in 'interrupt'
   function.  */

#define EPILOGUE_END_HOOK(STREAM)					\
do									\
  {									\
    if (lookup_attribute ("interrupt",					\
			  TYPE_ATTRIBUTES (TREE_TYPE (current_function_decl)))) \
      asm_fprintf ((STREAM), "\ttstl %Rd0\n");				\
  }									\
while (0)

/* begin-GG-local: explicit register specification for parameters */

/* Note: this is an extension of m68k_args */
struct amigaos_args
{
  int num_of_regs;
  long regs_already_used;
  int last_arg_reg;
  int last_arg_len;
  void *formal_type; /* New field: formal type of the current argument.  */
};

/* A C type for declaring a variable that is used as the first
   argument of `FUNCTION_ARG' and other related values.  */

#undef CUMULATIVE_ARGS
#define CUMULATIVE_ARGS struct amigaos_args

/* Initialize a variable CUM of type CUMULATIVE_ARGS
   for a call to a function whose data type is FNTYPE.
   For a library call, FNTYPE is 0.  */

#undef INIT_CUMULATIVE_ARGS
#define INIT_CUMULATIVE_ARGS(CUM, FNTYPE, LIBNAME, INDIRECT, N_NAMED_ARGS) \
  (amigaos_init_cumulative_args(&(CUM), (FNTYPE)))

/* Update the data in CUM to advance over an argument
   of mode MODE and data type TYPE.
   (TYPE is null for libcalls where that information may not be available.)  */

#undef FUNCTION_ARG_ADVANCE
#define FUNCTION_ARG_ADVANCE(CUM, MODE, TYPE, NAMED)			\
  (amigaos_function_arg_advance (&(CUM)))

/* A C expression that controls whether a function argument is passed
   in a register, and which register. */

#undef FUNCTION_ARG
#define FUNCTION_ARG(CUM, MODE, TYPE, NAMED) \
  (amigaos_function_arg (&(CUM), (MODE), (TYPE)))

/* end-GG-local */

/* Stack checking and automatic extension support.  */

#define PROLOGUE_BEGIN_HOOK(STREAM, FSIZE)				\
  (amigaos_prologue_begin_hook ((STREAM), (FSIZE)))

#define HAVE_ALTERNATE_FRAME_SETUP_F(FSIZE) TARGET_STACKEXTEND

#define ALTERNATE_FRAME_SETUP_F(STREAM, FSIZE)				\
  (amigaos_alternate_frame_setup_f ((STREAM), (FSIZE)))

#define HAVE_ALTERNATE_FRAME_SETUP(FSIZE) TARGET_STACKEXTEND

#define ALTERNATE_FRAME_SETUP(STREAM, FSIZE)				\
  (amigaos_alternate_frame_setup ((STREAM), (FSIZE)))

#define HAVE_ALTERNATE_FRAME_DESTR_F(FSIZE)				\
  (TARGET_STACKEXTEND && current_function_calls_alloca)

#define ALTERNATE_FRAME_DESTR_F(STREAM, FSIZE)				\
  (asm_fprintf ((STREAM), "\tjra %U__unlk_a5_rts\n"))

#define HAVE_ALTERNATE_RETURN						\
  (TARGET_STACKEXTEND && frame_pointer_needed &&			\
   current_function_calls_alloca)

#define ALTERNATE_RETURN(STREAM)

#define HAVE_restore_stack_nonlocal TARGET_STACKEXTEND
#define gen_restore_stack_nonlocal gen_stack_cleanup_call

#define HAVE_restore_stack_function TARGET_STACKEXTEND
#define gen_restore_stack_function gen_stack_cleanup_call

#define HAVE_restore_stack_block TARGET_STACKEXTEND
#define gen_restore_stack_block gen_stack_cleanup_call

#undef TARGET_ALTERNATE_ALLOCATE_STACK
#define TARGET_ALTERNATE_ALLOCATE_STACK 1

#define ALTERNATE_ALLOCATE_STACK(OPERANDS)				\
do									\
  {									\
    amigaos_alternate_allocate_stack (OPERANDS);			\
    DONE;								\
  }									\
while (0)

/* begin-GG-local: dynamic libraries */

extern int amigaos_do_collecting (void);
extern void amigaos_gccopts_hook (const char *);
extern void amigaos_libname_hook (const char* arg);
extern void amigaos_collect2_cleanup (void);
extern void amigaos_prelink_hook (const char **, int *);
extern void amigaos_postlink_hook (const char *);

/* This macro is used to check if all collect2 facilities should be used.
   We need a few special ones, like stripping after linking.  */

#define DO_COLLECTING (do_collecting || amigaos_do_collecting())

/* This macro is called in collect2 for every GCC argument name.
   ARG is a part of commandline (without '\0' at the end).  */

#define COLLECT2_GCC_OPTIONS_HOOK(ARG) amigaos_gccopts_hook(ARG)

/* This macro is called in collect2 for every ld's "-l" or "*.o" or "*.a"
   argument.  ARG is a complete argument, with '\0' at the end.  */

#define COLLECT2_LIBNAME_HOOK(ARG) amigaos_libname_hook(ARG)

/* This macro is called at collect2 exit, to clean everything up.  */

#define COLLECT2_EXTRA_CLEANUP amigaos_collect2_cleanup

/* This macro is called just before the first linker invocation.
   LD1_ARGV is "char** argv", which will be passed to "ld".  STRIP is an
   *address* of "strip_flag" variable.  */

#define COLLECT2_PRELINK_HOOK(LD1_ARGV, STRIP) \
amigaos_prelink_hook((const char **)(LD1_ARGV), (STRIP))

/* This macro is called just after the first linker invocation, in place of
   "nm" and "ldd".  OUTPUT_FILE is the executable's filename.  */

#define COLLECT2_POSTLINK_HOOK(OUTPUT_FILE) amigaos_postlink_hook(OUTPUT_FILE)
/* end-GG-local */

/* Don't use any specific register allocation order.  */
#undef REG_ALLOC_ORDER
