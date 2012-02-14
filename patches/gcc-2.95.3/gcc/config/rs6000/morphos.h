/* Definitions of target machine for GNU compiler.  Amiga powerpc version.
   Copyright (C) 1996 Free Software Foundation, Inc.

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

#include "rs6000/sysv4.h"

/* Enable some special Amiga only features.  FIXME: This define should
   be eliminated in favor of more general ways to enable the features. */

#define TARGET_MORPHOS 1

#undef PROCESSOR_DEFAULT
#define PROCESSOR_DEFAULT PROCESSOR_PPC604e

#undef DEFAULT_ABI
#define DEFAULT_ABI ABI_V4

/* Put jump tables in text section to suppress some relocations */
#undef JUMP_TABLES_IN_TEXT_SECTION
#define JUMP_TABLES_IN_TEXT_SECTION 1


extern enum rs6000_sdata_type rs6000_sdata;

#undef CPP_SPEC
#define CPP_SPEC \
  "%{!ansi:" \
    "%{!noixemul:-Dixemul} " \
    "%{noixemul:-Dlibnix}} " \
  "%{mcpu=603e:-D__603e__} " \
  "%{mcpu=604e:-D__604e__} " \
  "%{!noixemul:-D__ixemul__ -D__ixemul} " \
  "%{noixemul:-D__libnix__ -D__libnix} " \
  "%{malways-restore-r13:-Derrno=(*ixemul_errno)} " \
  "%{malways-restore-a4:-Derrno=(*ixemul_errno)} " \
  "%{mrestore-r13:-Derrno=(*ixemul_errno)} " \
  "%{mrestore-a4:-Derrno=(*ixemul_errno)} " \
  "%{msoft-float: -D_SOFT_FLOAT} "

#undef CPP_PREDEFINES
#define CPP_PREDEFINES \
  "-D__PPC__ -D_CALL_SYSV -Damiga -Damigaos -DMCH_AMIGA -DAMIGA " \
  "-Asystem(morphos) --Acpu(powerpc) -Amachine(powerpc) " \
  "-D__MORPHOS__ -Dmorphos -D__powerpc__ " \
  "-D__saveds=__attribute__((__saveds__)) "\
  "-D__stackext=__attribute__((__stackext__)) " \


/* Choose the right startup file, depending on whether we use base relative
   code, base relative code with automatic relocation (-resident), or plain
   crt0.o. 
  
   Profiling is currently only available for plain startup.
   mcrt0.o does not (yet) exist. */

#undef STARTFILE_SPEC
#define STARTFILE_SPEC							\
  "%{!noixemul:"							\
    "%{mbaserel:%{!mresident:bcrt0.o%s}}"				\
    "%{mresident:rcrt0.o%s}"						\
    "%{mbaserel32:%{!mresident32:lcrt0.o%s}}"				\
    "%{mresident32:scrt0.o%s}"						\
    "%{!mresident:%{!mbaserel:%{!mresident32:%{!mbaserel32:"		\
      "%{pg:gcrt0.o%s}%{!pg:%{p:mcrt0.o%s}%{!p:crt0.o%s}}}}}}}"		\
  "%{noixemul:libnix/startup.o%s} "

#undef ENDFILE_SPEC
/*#define ENDFILE_SPEC \
  "%{noixemul:libnix/end.o%s} "*/

/* Automatically search libamiga.a for AmigaOS specific functions.  Note
   that we first search the standard C library to resolve as much as
   possible from there, since it has names that are duplicated in libamiga.a
   which we *don't* want from there.  Then search the standard C library
   again to resolve any references that libamiga.a might have generated.
   This may only be a temporary solution since it might be better to simply
   remove the things from libamiga.a that should be pulled in from libc.a
   instead, which would eliminate the first reference to libc.a. */

#undef LIB_SPEC
#define LIB_SPEC							\
  "%{!noixemul:"							\
    "%{!p:%{!pg:-lc -lamiga -lamigastubs -lc}}"				\
    "%{p:-lc_p}%{pg:-lc_p}}"						\
  "%{noixemul:-lc -lamiga -lamigastubs"					\
    "%{mstackcheck:-lstack} "						\
    "%{mstackextend:-lstack}} -lsyscall "

#undef LINK_SPEC
#define LINK_SPEC							\
  "%{h*} %{v:-V} %{G*} %{Wl,*:%*} %{YP,*} %{R*} %{Qy:} %{!Qn:-Qy} "	\
  "%{noixemul:-fl libnix} "						\
  "%{mbaserel:%{!mresident:-m morphos_baserel -G 1000000 -fl libb}} "	\
  "%{mresident:-m morphos_baserel -datadata-reloc -G 100000 -fl libb} "	\
  "%{mbaserel32:%{!mresident32:-m morphos_baserel -fl libb32}} "	\
  "%{mresident32:-m morphos_baserel -datadata-reloc -fl libb32} "	

/* The place to find libc.a, libamiga.c and crt0.o depends on the runtime
** environment being used. As usual, default to the ixemul environment.
*/

/*#undef LINK_PATH_SPEC
#define LINK_PATH_SPEC \
 "%{!runtime=*:ixemul%s} %{runtime=ixemul:ixemul%s} \
 %{runtime=libnix:libnix%s} %{runtime=librilc:librilc%s}"*/

/*#undef LINK_SYSCALL:
  #define LINK_SYSCALL "-lsyscall "*/

/* Translate '-resident' to '-fbaserel' (they differ in linking stage only).
   Don't put function addresses in registers for PC-relative code.  */

#undef CC1_SPEC
#define CC1_SPEC							\
  "%{G*} "								\
  "%{mbaserel:-msdata=sysv -G 100000} "					\
  "%{mbaserel32:-msdata=baserel} "					\
  "%{mresident:-msdata=sysv -G 100000} "					\
  "%{mresident32:-msdata=baserel} "						

/* Define the name of the runtime environment (libc, etc.) used so it can be
   tested for during compilation.
   Abuse (?) CPP_OS_DEFAULT_SPEC for this purpose. -rask */

/*#undef CPP_OS_DEFAULT_SPEC
#define CPP_OS_DEFAULT_SPEC \
 "%{!runtime=*:-D__ixemul__} %{runtime=ixemul:-D__ixemul__} \
 %{runtime=libnix:-D__libnix__} %{runtime=librilc:-D__librilc__}"*/

#undef EXTRA_SPECS
#undef MULTILIB_DEFAULTS

/* Compile with stack extension.  */

#define MASK_STACKEXTEND 0x00800000
#define TARGET_STACKEXTEND ((target_flags & MASK_STACKEXTEND)		\
  || lookup_attribute ("stackext",					\
		       TYPE_ATTRIBUTES (TREE_TYPE (current_function_decl))))

/* Compile with stack checking.  */

#define MASK_STACKCHECK 0x00400000
#define TARGET_STACKCHECK ((target_flags & MASK_STACKCHECK)		\
  && !(target_flags & MASK_STACKEXTEND)					\
  && !lookup_attribute ("stackext",					\
			TYPE_ATTRIBUTES (TREE_TYPE (current_function_decl))))

/* Compile with r13 restoring in public functions.  */

#define MASK_RESTORE_R13 0x00200000
#define TARGET_RESTORE_R13						\
  ((target_flags & MASK_RESTORE_R13) && TREE_PUBLIC (current_function_decl))

/* Compile with r13 restoring in all functions.  */

#define MASK_ALWAYS_RESTORE_R13 0x00100000
#define TARGET_ALWAYS_RESTORE_R13 (target_flags & MASK_ALWAYS_RESTORE_R13)

#undef EXTRA_SUBTARGET_SWITCHES
#define EXTRA_SUBTARGET_SWITCHES					\
    { "baserel", 0, "16-bits relative data access"},			\
    { "baserel32", 0, "32-bits relative data access"},			\
    { "resident", 0, "make a reentrant executable with <= 64k data"},	\
    { "resident32", 0, "make reentrant executable"},			\
    { "stackcheck", MASK_STACKCHECK, "check for stack overflow"},	\
    { "no-stackcheck", - MASK_STACKCHECK, "don't check for stack overflow"},	\
    { "stackextend", MASK_STACKEXTEND, "automatically extend stack"},	\
    { "no-stackextend", - MASK_STACKEXTEND, "don't extend stack"},	\
    { "fixedstack", - (MASK_STACKCHECK|MASK_STACKEXTEND), "don't extend the stack" },\
    { "restore-r13", MASK_RESTORE_R13},					\
    { "no-restore-r13", - MASK_RESTORE_R13},				\
    { "always-restore-r13", MASK_ALWAYS_RESTORE_R13},			\
    { "no-always-restore-r13", - MASK_ALWAYS_RESTORE_R13},		\
    { "restore-a4", MASK_RESTORE_R13},					\
    { "no-restore-a4", - MASK_RESTORE_R13},				\
    { "always-restore-a4", MASK_ALWAYS_RESTORE_R13},			\
    { "no-always-restore-a4", - MASK_ALWAYS_RESTORE_R13},


/* This macro generates the assembly code for function entry.
   FILE is a stdio stream to output the code to.
   SIZE is an int: how many units of temporary storage to allocate.
   Refer to the array `regs_ever_live' to determine which registers
   to save; `regs_ever_live[I]' is nonzero if register number I
   is ever used in the function.  This macro is responsible for
   knowing which registers should not be saved even if used.  */

#undef FUNCTION_PROLOGUE
#define FUNCTION_PROLOGUE(FILE, SIZE) morphos_output_prolog (FILE, SIZE)


/* This is (almost;-)) BSD, so it wants DBX format.  */

#undef PREFERRED_DEBUGGING_TYPE
#define PREFERRED_DEBUGGING_TYPE DBX_DEBUG
#define DBX_DEBUGGING_INFO
#define	DWARF_DEBUGGING_INFO

#if 0
/* To maintain the binary compatibility with the m68k structures, we need
   to align 32-bit integers and pointers only to 16 bits in structures.  */

#undef ADJUST_FIELD_ALIGN
#define ADJUST_FIELD_ALIGN(FIELD, COMPUTED) \
  (DECL_MODE (FIELD) != SImode ? (COMPUTED) : MIN ((COMPUTED), 16))
#endif

/* Accesses to floats must be at least 4 bytes-aligned */
#define GET_MIN_MODE_ALIGNMENT(mode) (mode == DImode || FLOAT_MODE_P(mode) ? 32 : 8)
#define GET_MIN_TYPE_ALIGNMENT(type) (FLOAT_TYPE_P(type) ? 32 : 8)

/* Use sjlj exceptions until DWARF2 unwind info works */
//#define DWARF2_UNWIND_INFO	0
#undef DWARF2_UNWIND_INFO

/* If defined, a C expression whose value is nonzero if IDENTIFIER
   with arguments ARGS is a valid machine specific attribute for TYPE.
   The attributes in ATTRIBUTES have previously been assigned to TYPE.  */

#undef VALID_MACHINE_TYPE_ATTRIBUTE
#define VALID_MACHINE_TYPE_ATTRIBUTE(TYPE, ATTRIBUTES, NAME, ARGS) \
  (morphos_valid_type_attribute_p (TYPE, ATTRIBUTES, NAME, ARGS))

#define EXTRA_STACK_INFO \
	if (morphos_restore_r13()) {			\
	    info_ptr->calls_p = 1;			\
	    info_ptr->lr_save_p = 1;			\
	    regs_ever_live[65] = 1;			\
	    if (info_ptr->first_gp_reg_save > 13) {	\
		info_ptr->gp_size += reg_size;		\
		if (info_ptr->first_gp_reg_save == 14)	\
		    --info_ptr->first_gp_reg_save;	\
	    }                           		\
	}

#define EXTRA_PROLOG \
	if (morphos_restore_r13() && info->first_gp_reg_save > 14) \
	    asm_fprintf (file, store_reg,	\
		 reg_names[13],			\
		 info->gp_save_offset + sp_offset + (32 - info->first_gp_reg_save) * reg_size, \
		 reg_names[sp_reg]);

#define EXTRA_PROLOG_END \
	if (morphos_restore_r13()) \
		asm_fprintf (file, "\tbl __restore_r13\n");

#define EXTRA_EPILOG \
	if (morphos_restore_r13() && info->first_gp_reg_save > 14) \
	    asm_fprintf (file, load_reg,	\
		reg_names[13],			\
		info->gp_save_offset + sp_offset + (32 - info->first_gp_reg_save) * info->reg_size, \
		reg_names[sp_reg]);


#define CALL_VARARGS68K		0x80000000 /* Use 68k-style varargs */


/* Initialize a variable CUM of type CUMULATIVE_ARGS
   for a call to a function whose data type is FNTYPE.
   For a library call, FNTYPE is 0.  */

#undef INIT_CUMULATIVE_ARGS
#define INIT_CUMULATIVE_ARGS(CUM,FNTYPE,LIBNAME,INDIRECT) \
  morphos_init_cumulative_args (&CUM, FNTYPE, LIBNAME, FALSE)

/* Similar, but when scanning the definition of a procedure.  We always
   set NARGS_PROTOTYPE large so we never return an EXPR_LIST.  */

#undef INIT_CUMULATIVE_INCOMING_ARGS
#define INIT_CUMULATIVE_INCOMING_ARGS(CUM,FNTYPE,LIBNAME) \
  morphos_init_cumulative_args (&CUM, FNTYPE, LIBNAME, TRUE)

/* Update the data in CUM to advance over an argument
   of mode MODE and data type TYPE.
   (TYPE is null for libcalls where that information may not be available.)  */

#undef FUNCTION_ARG_ADVANCE
#define FUNCTION_ARG_ADVANCE(CUM, MODE, TYPE, NAMED)	\
  morphos_function_arg_advance (&CUM, MODE, TYPE, NAMED)

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
   and the rest are pushed.  The first 13 FP args are in registers.

   If this is floating-point and no prototype is specified, we use
   both an FP and integer register (or possibly FP reg and stack).  Library
   functions (when TYPE is zero) always have the proper types for args,
   so we can pass the FP value just in one register.  emit_library_function
   doesn't support EXPR_LIST anyway.  */

#undef FUNCTION_ARG
#define FUNCTION_ARG(CUM, MODE, TYPE, NAMED) \
  morphos_function_arg (&CUM, MODE, TYPE, NAMED)

/* Perform any needed actions needed for a function that is receiving a
   variable number of arguments.

   CUM is as above.

   MODE and TYPE are the mode and type of the current parameter.

   PRETEND_SIZE is a variable that should be set to the amount of stack
   that must be pushed by the prolog to pretend that our caller pushed
   it.

   Normally, this macro will push all remaining incoming registers on the
   stack and set PRETEND_SIZE to the length of the registers pushed.  */

#undef SETUP_INCOMING_VARARGS
#define SETUP_INCOMING_VARARGS(CUM,MODE,TYPE,PRETEND_SIZE,NO_RTL) \
  morphos_setup_incoming_varargs (&CUM, MODE, TYPE, &PRETEND_SIZE, NO_RTL)

/* If defined, is a C expression that produces the machine-specific
   code for a call to `__builtin_saveregs'.  This code will be moved
   to the very beginning of the function, before any parameter access
   are made.  The return value of this function should be an RTX that
   contains the value to use as the return of `__builtin_saveregs'.

   The argument ARGS is a `tree_list' containing the arguments that
   were passed to `__builtin_saveregs'.

   If this macro is not defined, the compiler will output an ordinary
   call to the library function `__builtin_saveregs'.  */

#undef EXPAND_BUILTIN_SAVEREGS
#define EXPAND_BUILTIN_SAVEREGS(ARGS) \
  morphos_expand_builtin_saveregs (ARGS)


/* begin-GG-local: dynamic libraries */

/* This macro is used to check if all collect2 facilities should be used.
   We need a few special ones, like stripping after linking.  */

#define DO_COLLECTING (do_collecting || morphos_do_collecting())

/* This macro is called in collect2 for every GCC argument name.
   ARG is a part of commandline (without '\0' at the end).  */

#define COLLECT2_GCC_OPTIONS_HOOK(ARG) morphos_gccopts_hook(ARG)

/* This macro is called in collect2 for every ld's "-l" or "*.o" or "*.a"
   argument.  ARG is a complete argument, with '\0' at the end.  */

#define COLLECT2_LIBNAME_HOOK(ARG) morphos_libname_hook(ARG)

/* This macro is called at collect2 exit, to clean everything up.  */

#define COLLECT2_EXTRA_CLEANUP morphos_collect2_cleanup

/* This macro is called just before the first linker invocation.
   LD1_ARGV is "char** argv", which will be passed to "ld".  STRIP is an
   *address* of "strip_flag" variable.  */

#define COLLECT2_PRELINK_HOOK(LD1_ARGV, STRIP) \
  morphos_prelink_hook((LD1_ARGV), (STRIP))

/* This macro is called just after the first linker invocation, in place of
   "nm" and "ldd".  OUTPUT_FILE is the executable's filename.  */

#define COLLECT2_POSTLINK_HOOK(OUTPUT_FILE) morphos_postlink_hook(OUTPUT_FILE)
/* end-GG-local */


extern void morphos_init_cumulative_args ();
extern void morphos_function_arg_advance ();
extern struct rtx_def *morphos_function_arg ();
extern void morphos_setup_incoming_varargs ();
extern struct rtx_def *morphos_expand_builtin_saveregs ();

