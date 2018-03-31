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
			sprintf(newObj->statInfo,"%ld %ld %d %ld %ld %ld",FIB->fib_Size,FIB->fib_NumBlocks,FIB->fib_DirEntryType>0?1:0,FIB->fib_Date.ds_Days,FIB->fib_Date.ds_Minute,FIB->fib_Date.ds_Tick);
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
	struct VolumeInfo* newObj;
	struct VolumeInfo* ptrHead;
	struct VolumeInfo* volinfoHead=NULL;

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
						if (navigator->dvi_Type==flag)
						{	
							newObj=(struct VolumeInfo*)malloc(sizeof(struct VolumeInfo));
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

// Read file or directory attribute
struct Amiga_Stat* Amiga_Get_Stat(char* path)
{
	struct Amiga_Stat* out=NULL;
	struct FileInfoBlock * FIB;
	BPTR lock;
	lock = Lock(path, ACCESS_READ);
	if (!lock)
	{
		if (VERBOSE) printf("File %s not readable\n",path);
		return NULL;
	}
	FIB = AllocVec(sizeof(struct FileInfoBlock), MEMF_CLEAR);
	if (FIB)
	{
		Examine(lock, FIB);
		out = malloc (sizeof(struct Amiga_Stat));
		out->st_size=FIB->fib_Size;
		out->st_blksize=FIB->fib_NumBlocks;
		out->directory=FIB->fib_DirEntryType>0?1:0;
		out->days=FIB->fib_Date.ds_Days;
		out->minutes=FIB->fib_Date.ds_Minute;
		out->seconds=FIB->fib_Date.ds_Tick;
	}
	FreeVec(FIB);
	UnLock(lock);
	return out;
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

void getVolumeName(const char* fullPath,const int size,char* out)
{
	int cont;
	for (cont=0;cont<size;cont++)
	{
		if (fullPath[cont]==':'||fullPath[cont]==0)
		{
			out[cont]=0;
			break;
		}
		else out[cont]=fullPath[cont];
	}
	return ;
}

// Read stat for a location
struct Amiga_Statfs* getStatFs(char* path)
{
	struct InfoData * FIB;
	BPTR lock;
	BOOL success;
	struct Amiga_Statfs* out=NULL;

	lock = Lock(path, ACCESS_READ);
	if (lock==0) return NULL;
	FIB = AllocVec(sizeof(struct FileInfoBlock), MEMF_CLEAR);
	if (FIB)
	{
		success = Info(lock, FIB);
		out = malloc (sizeof(struct Amiga_Statfs));
		out->st_blksize=FIB->id_BytesPerBlock;
		out->st_numblocks=FIB->id_NumBlocks;
		out->st_numblocksused=FIB->id_NumBlocksUsed;
		FreeVec(FIB);
	}
	UnLock(lock);
	return out;	
}
void Amiga_Simulate_Keypress(const int keycode,const int time,const int updown)
{
	volatile UBYTE* boh = (volatile UBYTE*) 0xbfe401;
	volatile UBYTE* boh2 = (volatile UBYTE*) 0xbfe501;
	volatile UBYTE* boh3 = (volatile UBYTE*) 0xbfee01;
	volatile UBYTE* boh4 = (volatile UBYTE*) 0xbfec01;

	Disable();
	*boh=(UBYTE)time;
	*boh2=0;
	*boh3=65;
	if (updown==0) *boh4=(UBYTE)~((UBYTE)keycode<<1);
	else *boh4=~(((UBYTE)keycode<<1)+1);
	Delay(time);
	Enable();

	
}
