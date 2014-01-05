/* Taken from: http://aros.sourceforge.net/documentation/developers/zune-application-development.php */

#include <exec/types.h>
#include <libraries/mui.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <clib/muimaster_protos.h>
#include <clib/alib_protos.h>

/* Otherwise auto open will try version 37, and muimaster.library has version
 * 19.x for MUI 3.8 */
int __oslibversion = 0;

/* We don't use command line arguments. */
int __nocommandline = 1;

int main(void) {
  Object *wnd, *app, *but;

  // GUI creation
  app = ApplicationObject,
      SubWindow, wnd = WindowObject,
        MUIA_Window_Title, "Hello world!",
        WindowContents, VGroup,
          Child, TextObject,
            MUIA_Text_Contents, "\33cHello world!\nHow are you?",
          End,
          Child, but = SimpleButton("_Ok"),
          End,
        End,
      End;

  if (app != NULL) {
    ULONG sigs = 0;

    // Click Close gadget or hit Escape to quit
    DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             (APTR)app, 2,
             MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

    // Click the button to quit
    DoMethod(but, MUIM_Notify, MUIA_Pressed, FALSE,
             (APTR)app, 2,
             MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

    // Open the window
    set(wnd, MUIA_Window_Open, TRUE);

    while((LONG)DoMethod(app, MUIM_Application_NewInput, (APTR)&sigs)
          != MUIV_Application_ReturnID_Quit) {
      if (sigs) {
        sigs = Wait(sigs | SIGBREAKF_CTRL_C);
        if (sigs & SIGBREAKF_CTRL_C)
          break;
      }
    }

    // Destroy our application and all its objects
    MUI_DisposeObject(app);
  }

  return 0;
}
