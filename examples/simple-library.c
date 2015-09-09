/******************************************************************************/
/*                                                                            */
/* includes                                                                   */
/*                                                                            */
/******************************************************************************/

#include <dos/dosextens.h>
#include <proto/exec.h>
#include <stabs.h>

/******************************************************************************/
/*                                                                            */
/* exports                                                                    */
/*                                                                            */
/******************************************************************************/

const char LibName[]="simple.library";
const char LibIdString[]="version 1.0";

const UWORD LibVersion=1;
const UWORD LibRevision=0;

/******************************************************************************/
/*                                                                            */
/* global declarations                                                        */
/*                                                                            */
/******************************************************************************/

struct Library *myLibPtr;
struct ExecBase *SysBase;
struct DosLibrary *DOSBase;

/******************************************************************************/
/*                                                                            */
/* user library initialization                                                */
/*                                                                            */
/* !!! CAUTION: This function may run in a forbidden state !!!                */
/*                                                                            */
/******************************************************************************/

int __UserLibInit(struct Library *myLib)
{
  /* setup your library base - to access library functions over *this* basePtr! */

  myLibPtr = myLib;

  /* !!! required !!! */
  SysBase = *(struct ExecBase **)4L;

  return (DOSBase=(struct DosLibrary *)OpenLibrary("dos.library",33L))==NULL;
}

/******************************************************************************/
/*                                                                            */
/* user library cleanup                                                       */
/*                                                                            */
/* !!! CAUTION: This function runs in a forbidden state !!!                   */
/*                                                                            */
/******************************************************************************/

void __UserLibCleanup(void)
{
  CloseLibrary(&DOSBase->dl_lib);
}

/******************************************************************************/
/*                                                                            */
/* library dependent function(s)                                              */
/*                                                                            */
/******************************************************************************/

ADDTABL_1(__UserFunc,d0); /* One Argument in d0 */

int __UserFunc(long a)
{
  return a*2;
}

/******************************************************************************/
/*                                                                            */
/* endtable marker (required!)                                                */
/*                                                                            */
/******************************************************************************/

ADDTABL_END();

/******************************************************************************/
/*                                                                            */
/* end of simplelib.c                                                         */
/*                                                                            */
/******************************************************************************/
