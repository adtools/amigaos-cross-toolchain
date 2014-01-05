/* This example works even for Kickstart 1.3 ! */

#include <proto/exec.h>
#include <proto/dos.h>

int __nocommandline = 1;
int __initlibraries = 0;

struct DosLibrary *DOSBase = NULL;

int main() {
  if ((DOSBase = (struct DosLibrary *) OpenLibrary("dos.library", 34))) {
    Write(Output(), "Hello world!\n", 13);
    CloseLibrary((struct Library *)DOSBase);
  }
  return 0;
}
