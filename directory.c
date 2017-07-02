/*
    Copyright © 1995-2002, The AROS Development Team. All rights reserved.
    $Id: ExNext.c 33409 2010-05-31 13:45:26Z mazze $

    Test ExNext() function
*/

#include <proto/dos.h>
#include <proto/exec.h>

#include <dos/dos.h>
#include <dos/exall.h>
#include <dos/rdargs.h>
#include <dos/var.h>
#include <exec/memory.h>
#include <exec/types.h>

#include <utility/tagitem.h>
#include <stdio.h>


#define ARG_TEMPLATE    "DIRECTORY"
#define TOTAL_ARGS      1

static const char version[] = "$VER: ExNext 41.1 (30.01.2000)\n";

int main(int argc, char *argv[])
{
    struct RDArgs	* rda;
    IPTR		* args[TOTAL_ARGS];
    int			  Return_Value;
    BPTR		  lock;

    Return_Value = RETURN_OK;

    rda = ReadArgs(ARG_TEMPLATE, (IPTR *)args, NULL);
    if (rda)
    {
	if (!args[0])
	    lock = Lock("", ACCESS_READ);
	else
	    lock = Lock((STRPTR)args[0], ACCESS_READ);

	if (lock)
	{
	    struct FileInfoBlock * FIB;
	    BOOL success;
	    FIB = AllocVec(sizeof(struct FileInfoBlock), MEMF_CLEAR);
	    if (FIB)
	    {
	      success = Examine(lock, FIB);
	      printf("calling ExNext()...\n");
	      success = ExNext(lock, FIB);
	      printf("called ExNext()\n");
	      while (success != DOSFALSE)
	      {
		/* don't show dirs */
		if (FIB->fib_DirEntryType < 0)
		    printf("%s\n",FIB->fib_FileName);
		else
		    printf("%s (not a file)\n", FIB->fib_FileName);
		printf("calling ExNext()...\n");
		success = ExNext(lock, FIB);
		printf("called ExNext()\n");
	      }
	      FreeVec(FIB);
	    }
	    UnLock(lock);
	}
	else
	{
	    PrintFault(IoErr(), "ExNext");
	    Return_Value = RETURN_FAIL;
	}
    }
    else
    {
        PrintFault(IoErr(), "ExNext");
        Return_Value = RETURN_ERROR;
    }

    if (rda)
        FreeArgs(rda);

    return (Return_Value);
} /* main */