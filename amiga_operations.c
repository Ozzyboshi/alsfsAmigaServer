#include <clib/exec_protos.h>
#include <clib/alib_protos.h>
#include <proto/dos.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// Read all files and directory from a location
struct ContentInfo* getContentList(char* path)
{
	struct FileInfoBlock * FIB;
	BPTR lock;
	BOOL success;
	struct ContentInfo* newObj;
	struct ContentInfo* ptrHead;
	struct ContentInfo* continfoHead=NULL;
	int found=0;
	struct VolumeInfo* ptr;
	struct VolumeInfo* freePtr;
	struct VolumeInfo* volinfoHead=NULL;

	volinfoHead=getVolumes(DLT_VOLUME);
	ptr=volinfoHead;
	while (!found&&ptr)
	{
		if (!strncmp(ptr->name,path,strlen(ptr->name))) found=1;
		ptr=ptr->next;
	}

	ptr=volinfoHead;
	while (ptr!=NULL)
	{
		freePtr=ptr->next;
		free(ptr);
		ptr=freePtr;
	}
	if (!found) return NULL;

	lock = Lock(path, ACCESS_READ);
	if (lock==0) return NULL;
	FIB = AllocVec(sizeof(struct FileInfoBlock), MEMF_CLEAR);
	if (FIB)
	{
		Examine(lock, FIB);
		success = ExNext(lock, FIB);
		while (success != DOSFALSE)
		{
			newObj=malloc(sizeof(struct ContentInfo));
			strcpy(newObj->fileName,FIB->fib_FileName);
			if (FIB->fib_DirEntryType < 0)
			    newObj->type=0;
			else
			    newObj->type=1;
			newObj->next=NULL;
			if (continfoHead==NULL) continfoHead=newObj;
			else
			{
				ptrHead=continfoHead;
				while(ptrHead->next)
					ptrHead=ptrHead->next;
				ptrHead->next=newObj;
			}
			success = ExNext(lock, FIB);
		}
		FreeVec(FIB);
	}
	UnLock(lock);
	return continfoHead;	
}

// Read all volumes from dos.library
struct VolumeInfo* getVolumes(const int flag)
{
	struct DosLibrary* dosLib;
	struct RootNode *root;
	struct DosLibrary *DOSBase;
	struct DosInfo* dosInfoPtr;
	struct DevInfo* devInfoPtr;
	struct DevInfo* navigator;
	static struct VolumeInfo volInfoOut[10];
	struct VolumeInfo* newObj;
	struct VolumeInfo* ptrHead;
	struct VolumeInfo* volinfoHead=NULL;
	
	int cont=0;
	for (cont=0;cont<10;cont++)
		BSTR2C(0,volInfoOut[cont].name);
	cont=0;

	if ((DOSBase = (struct DosLibrary*)OpenLibrary("dos.library",37)))
	{
		root = DOSBase->dl_Root;
		if (root)
		{
			dosInfoPtr=(struct DosInfo*)BADDR(root->rn_Info);
			if (dosInfoPtr)
			{

				devInfoPtr=(struct DevInfo*)BADDR(dosInfoPtr->di_DevInfo);
				if (devInfoPtr)
				{

					for (navigator = devInfoPtr;navigator;navigator=BADDR(navigator->dvi_Next))
					{
						//if (navigator->dvi_Type==DLT_VOLUME)
						if (navigator->dvi_Type==flag)
						{
							/*BSTR2C(navigator->dvi_Name,tmp);
							printf("%s\n",tmp);*/
							BSTR2C(navigator->dvi_Name, volInfoOut[cont++].name) ;	
							newObj=malloc(sizeof(struct VolumeInfo));
							BSTR2C(navigator->dvi_Name,newObj->name);
							newObj->next=NULL;
							if (volinfoHead==NULL) volinfoHead=newObj;
							else
							{
								ptrHead=volinfoHead;
								while(ptrHead->next)
									ptrHead=ptrHead->next;
								ptrHead->next=newObj;
							}
						}
					}
				}
				else
					printf("devinfoptr null");
			}
		}
	}
	else 
	{
   		printf("Failed to open DOS library.\n");
   		return NULL;
	}
	CloseLibrary( (struct Library *)DOSBase);
	return volinfoHead;
}

void BSTR2C(BSTR string, UBYTE* s)
{
  UBYTE i = 0 ;
  UBYTE *ptr = (UBYTE *)BADDR(string) ;

  for (i=0; i < ptr[0]; i++)
  {
    s[i] = ptr[i+1] ;
  }
  s[i] = 0 ;
}