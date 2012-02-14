/*  Linker front end for BeOS.

    Copyright (C) 1997 Free Software Foundation, Inc.

    BeOS uses a proprietary Apple executable format called PEF.  However the
    Metrowerks linker that is supplied with the system also reads XCOFF so
    the gcc port generates XCOFF object files, which can be linked together,
    along with other XCOFF objects from libraries, using the GNU linker with
    the "-r" option to generate a single relocatable object file.  That file
    can then be passed to the Metrowerks linker to add the BeOS runtime
    support and generate a PEF executable.

    This front end is compiled and installed in place of the regular GNU
    linker, which is moved to ld-coff.  It first runs ld-coff with the args
    that it is given, except that -o options are redirected to a temporary
    file, and then it runs mwld to covert the relocatable temporary file
    into a PEF executable in the file specified by the -o option (or "a.out"
    as the default).

    This front end was written by Fred Fish (fnf@cygnus.com) using various
    pieces from gcc's "collect2.c".  Contributers to collect2.c include
    Chris Smith (csmith@convex.com), Michael Meissner (meissner@cygnus.com),
    Per Bothner (bothner@cygnus.com), and John Gilmore (gnu@cygnus.com).

    This is expected to be only a temporary solution.  I have formally
    requested (9/96) a license from Apple to incorporate PEF support into
    BFD using information from their licensed PEF documentation, and to have
    that code covered *only* by the GNU General Public License.  That
    request is still pending.  If PEF support is installed, then this useful
    hack will go away.  Or perhaps Be will migrate to more open standards
    first.

This file is part of GLD, the Gnu Linker.

GLD is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GLD is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GLD; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "ansidecl.h"
#include "config.h"

#ifndef CROSS_LINKER
#define CROSS_LINKER 0
#endif

#define TEMP_FILE_TEMPLATE "/tmp/ldhackXXXXXX"

#ifndef LDNAME
#define LDNAME "ld-coff"		/* The renamed GNU linker */
#endif

#ifndef TARGET_ALIAS
#define TARGET_ALIAS ""
#endif

#define MW_LD "mwld"			/* The Metrowerks linker */
#define DEFAULT_OFILE "a.out"		/* Default output file name */

static int gflag = 0;			/* Non-zero if user specified -g flag */
static int vflag = 0;			/* Non-zero if user specified -v flag */
static int rflag = 0;			/* Non-zero if user specified -r flag or we are a cross linker */
static char *ofile = NULL;		/* Output file name */
static char *scratchfile;		/* Temporary file name */

static void do_wait PARAMS ((char *));
static void fork_execute PARAMS ((char *, char **));
static void collect_exit PARAMS ((int));

extern void *malloc ();
extern char *getenv ();


/* Die when sys call fails.  */

static void
fatal_perror (string, arg1, arg2, arg3)
     char *string, *arg1, *arg2, *arg3;
{
  int e = errno;

  fprintf (stderr, "ld: ");
  fprintf (stderr, string, arg1, arg2, arg3);
  fprintf (stderr, ": %s\n", strerror (e));
  collect_exit (1);
}

/* Just die.  */

static void
fatal (string, arg1, arg2, arg3)
     char *string, *arg1, *arg2, *arg3;
{
  fprintf (stderr, "ld: ");
  fprintf (stderr, string, arg1, arg2, arg3);
  fprintf (stderr, "\n");
  collect_exit (1);
}

/* Write error message.  */

static void
error (string, arg1, arg2, arg3, arg4)
     char *string, *arg1, *arg2, *arg3, *arg4;
{
  fprintf (stderr, "ld: ");
  fprintf (stderr, string, arg1, arg2, arg3, arg4);
  fprintf (stderr, "\n");
}

main (argc, argv)
     int argc;
     char *argv[];
{
  char **argp;
  char **ldargs;
  char **ldargp;
  char **tmp_ofile;
  char *ldname;
  int offset;

  argp = &argv[1];

  /* Find the GNU linker. */

  ldname = malloc (strlen (TARGET_ALIAS) + strlen (LDNAME) + 2);
  if (strlen (TARGET_ALIAS) > 0)
    {
      sprintf (ldname, "%s-%s", TARGET_ALIAS, LDNAME);
    }
  else
    {
      strcpy (ldname, LDNAME);
    }

  /* Build the temporary file name, being careful to ensure that the
     template is writable and not in const data. */

  scratchfile = malloc (strlen (TEMP_FILE_TEMPLATE));
  strcpy (scratchfile, TEMP_FILE_TEMPLATE);
  mktemp (scratchfile);

  /* Just add enough slop that we don't have to worry about being
     off a little bit. */
  ldargs = malloc ((argc + 16) * sizeof (char *));
  ldargp = ldargs;

  /* The first arg is the name of the GNU linker */

  *ldargp++ = ldname;

  while (*argp)
    {
      if (strcmp ("-g", *argp) == 0)
	{
	  gflag = 1;
	}
      else if (strcmp ("-v", *argp) == 0)
	{
	  vflag = 1;
	}
      else if (strcmp ("-r", *argp) == 0)
	{
	  /* Always pass explicit -r option on to GNU linker */
	  rflag = 1;
	  *ldargp++ = "-r";
	}
      else if (strcmp ("-o", *argp) == 0)
	{
	  /* Save the real output filename in ofile and
	     substitute our temporary file name. */
	  *ldargp++ = *argp++;
	  ofile = *argp++;
	  tmp_ofile = ldargp;
	  *ldargp++ = scratchfile;
	  /* Already handled args, skip to next */
	  continue;
	}
      *ldargp++ = *argp++;
    }

  /* Now check to see if we saw an explicit -o option.  If so, it currently
     points to the temporary file.  Leave it alone unless we also saw a -r
     option, in which case redirect it back to the real output file since
     we won't be running mwld.  If no explicit -o, need to add one unless
     we are only running the GNU linker and not mwld. */

  if (ofile == NULL)
    {
      /* No explicit -o option.  If not generating a relocatable
	 file then need to use the temporary file between passes. */
      if (!rflag && !CROSS_LINKER)
	{
	  *ldargp++ = "-o";
	  *ldargp++ = scratchfile;
	}
    }
  else
    {
      /* Got explicit -o option so it currently points to our scratch file.
	 If also got -r option, need to point it back to our real output
	 file. */
      if (rflag || CROSS_LINKER)
	{
	  *tmp_ofile = ofile;
	}
    }
  
  /* Generally we always want to supply a "-r" option to the GNU linker,
     unless we either got one on the input command line and thus already
     have one, or else the environment variable BEOS_STUBS is given
     which means that we are probably trying to find out what symbols are
     undefined in the BeOS runtime by supplying a stub object that defines
     the ones that are, and thus we don't want to supply "-r". */

  if (!rflag)
    {
      char *stubs = getenv ("BEOS_STUBS");
      *ldargp++ = stubs ? stubs : "-r";
    }

  /* Terminate the arg list to the GNU ld and run it. */

  *ldargp++ = NULL;
  fork_execute (ldargs[0], ldargs);

  /* If we are not generating a relocatable output file, then we need
     to run the Metrowerks linker to generate the final executable. */

  if (!rflag && !CROSS_LINKER)
    {
      char *mapfile;
      ldargp = ldargs;
      *ldargp++ = MW_LD;
      *ldargp++ = "-o";
      *ldargp++ = ofile;
      if (gflag)
	{
	  *ldargp++ = "-g";
	  mapfile = malloc (strlen (ofile) + 6);
	  sprintf (mapfile, "%s.xMAP", ofile);
	  *ldargp++ = "-map";
	  *ldargp++ = mapfile;
	}
      *ldargp++ = "-nodup";
      *ldargp++ = scratchfile;
      *ldargp++ = NULL;
      fork_execute (ldargs[0], ldargs);

      /* Since mwld doesn't make a file executable, we need to do that ourselves. */
      ldargp = ldargs;
      *ldargp++ = "chmod";
      *ldargp++ = "+x";
      *ldargp++ = ofile;
      *ldargp++ = NULL;
      fork_execute (ldargs[0], ldargs);
    }

  collect_exit (0);
}

/* Wait for a process to finish, and exit if a non-zero status is found.  */

int
collect_wait (prog)
     char *prog;
{
  int status;

  wait (&status);
  if (status)
    {
      if (WIFSIGNALED (status))
	{
	  int sig = WTERMSIG (status);
	  error ("%s terminated with signal %d %s",
		 prog,
		 sig,
		 (status & 0200) ? ", core dumped" : "");
	  collect_exit (127);
	}

      if (WIFEXITED (status))
	return WEXITSTATUS (status);
    }
  return 0;
}

static void
do_wait (prog)
     char *prog;
{
  int ret = collect_wait (prog);
  if (ret != 0)
    {
      error ("%s returned %d exit status", prog, ret);
      collect_exit (ret);
    }
}

/* Fork and execute a program, and wait for the reply.  */

static void
collect_execute (prog, argv, redir)
     char *prog;
     char **argv;
     char *redir;
{
  int pid;

  if (vflag)
    {
      char **p_argv;
      char *str;

      if (argv[0])
	fprintf (stderr, "%s", argv[0]);
      else
	fprintf (stderr, "[cannot find %s]", prog);

      for (p_argv = &argv[1]; (str = *p_argv) != (char *) 0; p_argv++)
	fprintf (stderr, " %s", str);

      fprintf (stderr, "\n");
    }

  fflush (stdout);
  fflush (stderr);

  /* If we can't find a program we need, complain error.  Do this here
     since we might not end up needing something that we couldn't find.  */

  if (argv[0] == 0)
    fatal ("cannot find `%s'", prog);

  pid = fork ();
  if (pid == -1)
    {
      fatal_perror ("fork");
    }

  if (pid == 0)			/* child context */
    {
      if (redir)
	{
	  unlink (redir);
	  if (freopen (redir, "a", stdout) == NULL)
	    fatal_perror ("redirecting stdout");
	  if (freopen (redir, "a", stderr) == NULL)
	    fatal_perror ("redirecting stderr");
	}

      execvp (argv[0], argv);
      fatal_perror ("executing %s", prog);
    }
}

static void
fork_execute (prog, argv)
     char *prog;
     char **argv;
{
  collect_execute (prog, argv, NULL);
  do_wait (prog);
}

/* Delete tempfiles and exit function.  */

void
collect_exit (status)
     int status;
{
  if (status > 0)
    {
      unlink (ofile);
    }
  unlink (scratchfile);
  exit (status);
}

