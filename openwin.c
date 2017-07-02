#include <exec/types.h>
#include <intuition/intuition.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <stdlib.h>
#include <dos/var.h>



#include <dos/dos.h>
#include <proto/dos.h>
#include <stdio.h>


#define INTUITION_REV 0
#define MILLION 1000000

struct Library *IntuitionBase;

int main(int argc, char **argv)
{
  struct NewWindow NewWindow;
  struct Window *Window;
struct FileInfoBlock * FIB;
BPTR		  lock;
lock = Lock("games:", ACCESS_READ);
printf("Apro games:");
FIB = AllocVec(sizeof(struct FileInfoBlock), MEMF_CLEAR);
  LONG i;
BOOL success;
success = Examine(lock, FIB);
success = ExNext(lock, FIB);
while (success != DOSFALSE)
{
	if (FIB->fib_DirEntryType < 0)
	    printf("%s\n",FIB->fib_FileName);
	else
	    printf("%s (not a file)\n", FIB->fib_FileName);
	success = ExNext(lock, FIB);
}
FreeVec(FIB);
UnLock(lock);

  IntuitionBase = (struct Library *) OpenLibrary("intuition.library", INTUITION_REV);
  if (IntuitionBase == NULL) exit(FALSE);

  NewWindow.LeftEdge = 20;
  NewWindow.TopEdge = 20;
  NewWindow.Width = 300;
  NewWindow.Height = 100;
  NewWindow.DetailPen = 0;
  NewWindow.BlockPen = 1;
  NewWindow.Title = "A Simple Window";
  NewWindow.Flags = WINDOWCLOSE | SMART_REFRESH | ACTIVATE |
    WINDOWSIZING | WINDOWDRAG | WINDOWDEPTH | NOCAREREFRESH;
  NewWindow.IDCMPFlags = CLOSEWINDOW;
  NewWindow.Type = WBENCHSCREEN;
  NewWindow.FirstGadget = NULL;
  NewWindow.CheckMark = NULL;
  NewWindow.Screen = NULL;
  NewWindow.BitMap = NULL;
  NewWindow.MinWidth = 0;
  NewWindow.MinHeight = 0;
  NewWindow.MaxWidth = 0;
  NewWindow.MaxHeight = 0;

  if ((Window = (struct Window *) OpenWindow(&NewWindow)) == NULL) {
    exit(FALSE);
  }
  Wait(1 << Window->UserPort->mp_SigBit);
  CloseWindow(Window);
  CloseLibrary(IntuitionBase);
  return 0;
}
