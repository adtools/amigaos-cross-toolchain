/******************************************************************************/
/*                                                                            */
/* includes                                                                   */
/*                                                                            */
/******************************************************************************/

#include <exec/errors.h>
#include <proto/exec.h>
#include <stabs.h>

/******************************************************************************/
/*                                                                            */
/* exports                                                                    */
/*                                                                            */
/******************************************************************************/

const char DevName[]="simple.device";
const char DevIdString[]="version 1.0";

const UWORD DevVersion=1;
const UWORD DevRevision=0;

/******************************************************************************/
/*                                                                            */
/* global declarations                                                        */
/*                                                                            */
/******************************************************************************/

struct Device *myDevPtr;
struct ExecBase *SysBase;

/******************************************************************************/
/*                                                                            */
/* user device initialization                                                 */
/*                                                                            */
/* !!! CAUTION: This function runs in a forbidden state !!!                   */
/*                                                                            */
/******************************************************************************/

int __UserDevInit(struct Device *myDev)
{
  /* !!! required !!! */
  SysBase = *(struct ExecBase **)4L;

  /* setup your device base - to access device functions over *this* basePtr! */

  myDevPtr = myDev;

  /* now do your initialization */

     /* ... */

  /* return a bool to indicate success */

  return 0;
}

/******************************************************************************/
/*                                                                            */
/* user device cleanup                                                        */
/*                                                                            */
/* !!! CAUTION: This function runs in a forbidden state !!!                   */
/*                                                                            */
/******************************************************************************/

void __UserDevCleanup(void)
{
  /* your cleanup comes here */

     /* ... */

  /* nothing to return */
}

/******************************************************************************/
/*                                                                            */
/* device dependent open function                                             */
/*                                                                            */
/* !!! CAUTION: This function runs in a forbidden state !!!                   */
/*                                                                            */
/******************************************************************************/

int __UserDevOpen(struct IORequest *iorq,ULONG unit,ULONG flags)
{
  int io_err = IOERR_OPENFAIL;

  /* return a bool to indicate success */

  return io_err;
}

/******************************************************************************/
/*                                                                            */
/* device dependent close function                                            */
/*                                                                            */
/* !!! CAUTION: This function runs in a forbidden state !!!                   */
/*                                                                            */
/******************************************************************************/

void __UserDevClose(struct IORequest *iorq)
{
  /* nothing to return */
}

/******************************************************************************/
/*                                                                            */
/* device dependent beginio function                                          */
/*                                                                            */
/******************************************************************************/

ADDTABL_1(__BeginIO,a1);

void __BeginIO(struct IORequest *iorq)
{
};

/******************************************************************************/
/*                                                                            */
/* device dependent abortio function                                          */
/*                                                                            */
/******************************************************************************/

ADDTABL_1(__AbortIO,a1);

void __AbortIO(struct IORequest *iorq)
{
};

/******************************************************************************/
/*                                                                            */
/* additional device dependent functions                                      */
/*                                                                            */
/******************************************************************************/



/******************************************************************************/
/*                                                                            */
/* endtable marker (required!)                                                */
/*                                                                            */
/******************************************************************************/

ADDTABL_END();

/******************************************************************************/
/*                                                                            */
/* end of simpledev.c                                                         */
/*                                                                            */
/******************************************************************************/
