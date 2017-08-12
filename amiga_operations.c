#include <clib/exec_protos.h>
#include <clib/alib_protos.h>
#include <proto/dos.h>

#include <stdio.h>

#include <devices/trackdisk.h>

#include "amiga_operations.h"
#include "debug.h"

int VERBOSE;

// Read data from a data in a trackdevice
int Amiga_Read_Adf_Data(int devicenum,int length,int offset,UBYTE ** out)
{
	int sectors = 11;
	struct MsgPort *port;
	struct IOStdReq *ioreq;
	char *devicename = "trackdisk.device";
	UBYTE * buffer = *out;
	int res=0;

	// Check presence of a floppy drive within drive number devicenum
	if (Amiga_Check_FloppyDisk_Presence(devicenum))
		return 0;

	//UBYTE * buffer = AllocMem(length, MEMF_CHIP);

	port = CreatePort(0, 0);
	if (port) 
	{
		ioreq = CreateStdIO(port);
		if (ioreq) 
		{
			if (OpenDevice(devicename, devicenum, (struct IORequest *) ioreq, 0) == 0) 
			{
				int sec;
				ioreq->io_Command = CMD_READ;
				ioreq->io_Length = length;		
			    ioreq->io_Data = buffer;
			    if (VERBOSE) printf("Reading at location %d for %d bytes\n",offset,length);
				ioreq->io_Offset = offset;
				if (DoIO( (struct IORequest *) ioreq))
				{
					fprintf(stderr,"Write failed.  Error %d\n",ioreq->io_Error);
				}
				else res=length;
				printf("%02X %02X %02X %02X %02X %02X \n",buffer[0],buffer[1],buffer[2],buffer[3],buffer[4],buffer[5]);

				ioreq->io_Command = TD_MOTOR;	/* Turn Disk-motor off */
				ioreq->io_Length = 0;
				DoIO( (struct IORequest *) ioreq);

				CloseDevice( (struct IORequest *) ioreq);
			}
			else
			{
				fprintf(stderr,"Unable to open %s unit %d\n", devicename, devicenum);
	    		//DeleteStdIO(ioreq);
			}
			DeleteStdIO(ioreq);
		}
		DeletePort(port);
	}
	return res ;
}

// Write data to a track in a trackdevice
void Amiga_Write_Adf_Track(int track,UBYTE ** buffer,int devicenum)
{
	int sectors = 11;
	struct MsgPort *port;
	struct IOStdReq *ioreq;
	char *devicename = "trackdisk.device";

	port = CreatePort(0, 0);
	if (port) 
	{
		ioreq = CreateStdIO(port);
		if (ioreq) 
		{
			if (OpenDevice(devicename, devicenum, (struct IORequest *) ioreq, 0) == 0) 
			{
				int sec;
				ioreq->io_Command = CMD_WRITE;
				ioreq->io_Length = 512;
				
				
		    	for (sec = 0; sec < sectors; sec++) 
		    	{
					fflush(stdout);
						
			    	ioreq->io_Data = buffer[sec];
			    	if (VERBOSE) printf("Writing at location %d\n",512 * (track * sectors + sec));
					ioreq->io_Offset = 512 * (track * sectors + sec);
					if (DoIO( (struct IORequest *) ioreq))
					{
						fprintf(stderr,"Write failed.  Error %d\n",ioreq->io_Error);
					}
		    	}
				
				ioreq->io_Command = CMD_UPDATE;
		    	DoIO( (struct IORequest *) ioreq);

				ioreq->io_Command = TD_MOTOR;	/* Turn Disk-motor off */
				ioreq->io_Length = 0;
				DoIO( (struct IORequest *) ioreq);

				CloseDevice( (struct IORequest *) ioreq);
			}
			else
			{
				fprintf(stderr,"Unable to open %s unit %d\n", devicename, devicenum);
	    		//DeleteStdIO(ioreq);
			}
			DeleteStdIO(ioreq);
		}
		DeletePort(port);
	}
}

int Amiga_Check_FloppyDisk_Presence(int devicenum)
{
	struct MsgPort *port;
	struct IOStdReq *ioreq;
	char *devicename = "trackdisk.device";
	int flag=-1;

	port = CreatePort(0, 0);
	if (port) 
	{
		ioreq = CreateStdIO(port);
		if (ioreq) 
		{
			if (OpenDevice(devicename, devicenum, (struct IORequest *) ioreq, 0) == 0) 
			{
				ioreq->io_Command = TD_CHANGESTATE;
				ioreq->io_Flags = 0;
				if (DoIO( (struct IORequest *) ioreq))
					fprintf(stderr,"Error in executing changestate\n");
				if (ioreq->io_Actual==0)
					flag=0;
				else
					flag=1;

				CloseDevice( (struct IORequest *) ioreq);
			}
			DeleteStdIO(ioreq);
		}
		DeletePort(port);
	}
	return flag;
}