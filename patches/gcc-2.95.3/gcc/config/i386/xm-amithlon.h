/* Configuration for GNU C-compiler for m68k Amiga, running AmigaOS.
   Copyright (C) 1992, 93-96, 1997 Free Software Foundation, Inc.
   Contributed by Markus M. Wild (wild@amiga.physik.unizh.ch).

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

/* First include the generic header, then modify some parts.  */

#include "i386/xm-i386.h"

#ifndef _FCNTL_H_
#include <fcntl.h>
#endif

/* Define various things that the AmigaOS host has.  */

#define HAVE_ATEXIT
#define HAVE_RENAME

/* AmigaOS specific headers, such as from the Native Developer Update kits,
   go in SYSTEM_INCLUDE_DIR.  STANDARD_INCLUDE_DIR is the equivalent of
   Unix "/usr/include".  All other include paths are set in Makefile.  */

#define SYSTEM_INCLUDE_DIR	"/gg/os-include"
#define STANDARD_INCLUDE_DIR	"/gg/include"

#define STANDARD_EXEC_PREFIX_1 "/gg/lib/gcc/"
#define STANDARD_STARTFILE_PREFIX_1 "/gg/lib/"
#define STANDARD_STARTFILE_PREFIX_2 "/gg/lib/"

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

/* On the AmigaOS, there are two pathname separators, '/' (DIR_SEPARATOR)
   and ':' (VOL_SEPARATOR).  DIR_SEPARATOR defaults to the correct
   character, so we don't have to explicitly set it.  */

#define DIR_SEPARATOR '/'
#define VOL_SEPARATOR ':'

/* Do *not* use this define, otherwise Amiga-devicesnames ('DEV:') won't 
   work:  */
// #define DIR_SEPARATOR_2 VOL_SEPARATOR


/* Determine whether a '\0'-terminated file name is absolute or not.

   This checks for both, '/' as the first character, since we're running under
   ixemul.library which provides for this unix'ism, and for the usual 
   logical-terminator, ':', somewhere in the filename.  */

#define FILE_NAME_ABSOLUTE_P(NAME) ((NAME)[0] == '/' || index ((NAME), ':'))

/* Like the above, but the file name is not '\0'-terminated.  */

#define FILE_NAME_ABSOLUTE_N_P(NAME, LEN) amigaos_file_name_absolute_n ((NAME), (LEN))

/* Return the file name part of the path name.  */

#define FILE_NAME_NONDIRECTORY(NAME)					\
  (rindex ((NAME), '/') ? rindex ((NAME), '/') + 1			\
			: (rindex ((NAME), ':') ? rindex ((NAME), ':') + 1 \
						: (NAME)))

/* Generate the name of the cross reference file.  */

#define XREF_FILE_NAME(BUFF, NAME)					\
do									\
  {									\
    char *filesrc, *filedst;						\
    strcpy ((BUFF), (NAME));						\
    filesrc = FILE_NAME_NONDIRECTORY (NAME);				\
    filedst = (BUFF) + (filesrc - (NAME));				\
    sprintf (filedst, ".%s.gxref", filesrc);				\
  }									\
while (0)
