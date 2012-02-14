/* Configuration for GNU C-compiler for Commodore Amiga, running AmigaOS.
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

#include "rs6000/xm-sysv4.h"

/* Define various things that the Amiga host has. */

#undef HAVE_VPRINTF
#define HAVE_VPRINTF
#undef HAVE_PUTENV
#define HAVE_PUTENV
#undef HAVE_STRERROR
#define HAVE_STRERROR
#undef HAVE_ATEXIT
#define HAVE_ATEXIT
#undef HAVE_RENAME
#define HAVE_RENAME

/* Amiga specific headers, such as from the Native Developer Update kits,
   go in SYSTEM_INCLUDE_DIR.  STANDARD_INCLUDE_DIR is the equivalent of
   Unix "/usr/include".  All other include paths are set in Makefile. */

#define SYSTEM_INCLUDE_DIR	"GG:os-include"
#define STANDARD_INCLUDE_DIR	"GG:include"

#define STANDARD_EXEC_PREFIX "/gg/lib/gcc-lib/"
#define STANDARD_EXEC_PREFIX_1 "/gg/lib/gcc/"
#define STANDARD_STARTFILE_PREFIX "/gg/lib/"
#define STANDARD_STARTFILE_PREFIX_1 "/gg/lib/"
#define STANDARD_STARTFILE_PREFIX_2 "/gg/lib/"
#define TOOLDIR_BASE_PREFIX "/gg/"

/* The AmigaOS stores file names with regard to upper/lower case, but actions
   on existing files are case independent on the standard filesystems.

   A good example of where this causes problems is the conflict between the C
   include file <string.h> and the C++ include file <String.h>, where the C++
   include file dir is searched first and thus causes includes of <string.h>
   to include <String.h> instead.

   In order to solve this problem we define the macro OPEN_CASE_SENSITIVE as
   the name of the function that takes the same args as open() and does case
   dependent opens.  */

#define OPEN_CASE_SENSITIVE(NAME, FLAGS, MODE) open ((NAME), (FLAGS) | O_CASE, (MODE))

/* On the Amiga, there are two pathname separators, '/' (DIR_SEPARATOR)
   and ':' (VOL_SEPARATOR).  DIR_SEPARATOR defaults to the correct
   character, so we don't have to explicitly set it. */

#define DIR_SEPARATOR '/'
#define VOL_SEPARATOR ':'
#define DIR_SEPARATOR_2 VOL_SEPARATOR

/* Determine whether a '\0'-terminated file name is absolute or not.

   This checks for both, '/' as the first character, since we're running under
   ixemul.library which provides for this unix'ism, and for the usual 
   logical-terminator, ':', somewhere in the filename.  */

#define FILE_NAME_ABSOLUTE_P(NAME) ((NAME)[0] == '/' || index ((NAME), ':'))


#if  0

/* Fork one piped subcommand.  SEARCH_FLAG is the system call to use
   (either execv or execvp).  ARGV is the arg vector to use.
   NOT_LAST is nonzero if this is not the last subcommand
   (i.e. its output should be piped to the next one.)  */

#define PEXECUTE(SEARCH_FLAG,PROGRAM,ARGV,NOT_LAST) \
({int (*_func)() = (SEARCH_FLAG ? execv : execvp);			\
  int _pid;								\
  int _pdes[2];								\
  int _input_desc = last_pipe_input;					\
  int _output_desc = STDOUT_FILE_NO;					\
  int _retries, _sleep_interval, _result;				\
									\
  /* If this isn't the last process, make a pipe for its output,	\
     and record it as waiting to be the input to the next process.  */	\
									\
  if (NOT_LAST)								\
    {									\
      if (pipe (_pdes) < 0)						\
	pfatal_with_name ("pipe");					\
      _output_desc = _pdes[WRITE_PORT];					\
      last_pipe_input = _pdes[READ_PORT];				\
    }									\
  else									\
    last_pipe_input = STDIN_FILE_NO;					\
									\
  /* Fork a subprocess; wait and retry if it fails.  */			\
  _sleep_interval = 1;							\
  for (_retries = 0; _retries < 4; _retries++)				\
    {									\
      _pid = vfork ();							\
      if (_pid >= 0)							\
	break;								\
      sleep (_sleep_interval);						\
      _sleep_interval *= 2;						\
    }									\
									\
  switch (_pid)								\
    {									\
    case -1:								\
      pfatal_with_name ("vfork");					\
      /* NOTREACHED */							\
      _result = 0;							\
      break;								\
									\
    case 0: /* child */							\
      /* Move the input and output pipes into place, if nec.  */	\
      if (_input_desc != STDIN_FILE_NO)					\
	{								\
	  close (STDIN_FILE_NO);					\
	  dup (_input_desc);						\
	  close (_input_desc);						\
	}								\
      if (_output_desc != STDOUT_FILE_NO)				\
	{								\
	  close (STDOUT_FILE_NO);					\
	  dup (_output_desc);						\
	  close (_output_desc);						\
	}								\
									\
      /* Close the parent's descs that aren't wanted here.  */		\
      if (last_pipe_input != STDIN_FILE_NO)				\
	close (last_pipe_input);					\
									\
      /* Exec the program.  */						\
      (*_func) (PROGRAM, ARGV);						\
      perror_exec (PROGRAM);						\
      exit (-1);							\
      /* NOTREACHED */							\
      _result = 0;							\
      break;								\
									\
    default:								\
      /* In the parent, after forking.					\
	 Close the descriptors that we made for this child.  */		\
      if (_input_desc != STDIN_FILE_NO)					\
	close (_input_desc);						\
      if (_output_desc != STDOUT_FILE_NO)				\
	close (_output_desc);						\
									\
      /* Return child's process number.  */				\
      _result = _pid;							\
      break;								\
    } 									\
_result; })								\

#define PEXECUTE_RESULT(STATUS, COMMAND) \
  ({ wait (& STATUS); })

/* the following macros are stolen more or less from xm-vms.h ... */

/* This macro is used to help compare filenames in cp-lex.c.

   We also need to make sure that the names are all lower case, because
   we must be able to compare filenames to determine if a file implements
   a class.  */

#define FILE_NAME_NONDIRECTORY(C)				\
({								\
   extern char *rindex();					\
   char * pnt_ = (C), * pnt1_;					\
   pnt1_ = pnt_ - 1;						\
   while (*++pnt1_)						\
     if ((*pnt1_ >= 'A' && *pnt1_ <= 'Z')) *pnt1_ |= 0x20;	\
   pnt1_ = rindex (pnt_, '/'); 					\
   pnt1_ = (pnt1_ == 0 ? rindex (pnt_, ':') : pnt1_);		\
   (pnt1_ == 0 ? pnt_ : pnt1_ + 1);				\
 })

/* Macro to generate the name of the cross reference file.  The standard
   one does not work, since it was written assuming that the conventions
   of a unix style filesystem will work on the host system.
 
   Contrary to VMS, I'm using the original unix filename, there's no reason
   not to use this under AmigaOS. */

#define XREF_FILE_NAME(BUFF, NAME)	\
  s = FILE_NAME_NONDIRECTORY (NAME);			\
  if (s == NAME) sprintf(BUFF, ".%s.gxref", NAME);	\
  else {						\
    unsigned char ch = *s; /* could be Latin1 char.. */	\
    /* temporary: cut the filename from the directory */\
    *s = 0;						\
    sprintf (BUFF, "%s.%c%s.gxref", NAME, ch, s+1);	\
    /* and restore the filename */			\
    *s = ch;						\
  }							\

/* Macro that is used in cp-xref.c to determine whether a file name is
   absolute or not.

   This checks for both, '/' as first character, since we're running under
   ixemul.library which provides for this unix'ism, and for the usual 
   logical-terminator, ':', somewhere in the filename. */

#define FILE_NAME_ABSOLUTE_P(NAME) (NAME[0] == '/' || index(NAME, ':'))

/* the colon conflicts with the name space of logicals */

#define PATH_SEPARATOR ','

/* Phil.B 17-Apr-95 Added stack checking code submitted by Kriton Kyrimis
   (kyrimis@theseas.ntua.gr) on 10-Feb-95, modified for inclusion into gcc

   What stackcheck does is to add the following code at the beginning of
   each function:
	cmpl __StackBottom,sp
	bccs .+8
	jmp __StackOverflow

   _StackBottom and _StackOverflow() are defined in stackchecksetup.c.
   _StackBottom is set to the bottom of the stack plus some leeway for
   subroutine arguments (128 bytes).  _StackOverflow() sets the stack
   pointer to a sane value, prints an error message and exits.

   Even with stack checking, you cannot be completely safe, as overflows
   are detected on function entry, rather than during function execution.
   E.g.,
	crash(){
	  char scribble[100000];
	  int i;
	  for (i=0; i<100000; i++) scribble[i]=0;
	  check(scribble);
	}
	check(char *x){}
   In the above example, stack overflow and its side-effects will occur in
   crash(), but it will be detected in check(), when it is too late. (The
   same thing happens with SAS/C's stack checking, BTW.)
*/

/* #defined AMIGA_STACK_CHECKING */

#endif
