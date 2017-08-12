/*
 * Simple_Serial.c
 *
 * This is an example of using the serial device.  First, we will attempt
 * to create a message port with CreateMsgPort().  Next, we will attempt
 * to create the IORequest with CreateExtIO().  Then, we will attempt to
 * open the serial device with OpenDevice().  If successful, we will write
 * a NULL-terminated string to it and reverse our steps.  If we encounter
 * an error at any time, we will gracefully exit.
 *
 * Compile with SAS C 5.10  lc -b1 -cfistq -v -y -L
 *
 * Run from CLI only
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <devices/serial.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <proto/dos.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <devices/trackdisk.h>


#include "serialread.h"
#include "bde64.h"
#include "amiga_operations.h"
#include "debug.h"


#define READ_BUFFER_SIZE 256
#define CHUNK_DATA_READ  512 // Chunk of bytes read from serial interface while file transfer


#define MINTRACKDEVICES 0 
#define MAXTRACKDEVICES 4 // Max number of trackdevices recognized by AmigaDOS

struct VolumeInfo
{
	UBYTE name[256];
	struct VolumeInfo* next;
};

struct ContentInfo
{
	char fileName[108];
	int type;
	struct ContentInfo* next;
};

// Data structure to hold information about a file or directory
struct Amiga_Stat
{
	long st_size;
	long st_blksize;
	int directory;
	long days;
	long minutes;
	long seconds;
};

void BSTR2C(BSTR,UBYTE*);

// Amiga functions
struct VolumeInfo* getVolumes(const int);
struct ContentInfo* getContentList(char*);
struct Amiga_Stat* Amiga_Get_Stat(char*);

void sendSerialEndOfData(struct IOExtSer*);

// Serial functions
void Amiga_Store_Data(struct IOExtSer*);
void Amiga_Create_Empty_File(struct IOExtSer*);
void Amiga_Delete(struct IOExtSer*);
void Amiga_Create_Empty_Drawer(struct IOExtSer*);
void Amiga_Rename_File_Drawer(struct IOExtSer*);
void Amiga_Send_vols(struct IOExtSer*,const int);
void Amiga_Send_List(struct IOExtSer*,char*);
void Serial_Amiga_Send_Stat(struct IOExtSer*,char*);
void Amiga_Read_File(struct IOExtSer*);
void Amiga_Delay(struct IOExtSer*);
void Amiga_Write_Adf(struct IOExtSer*);

void Serial_Check_Floppy_Drive(struct IOExtSer*);
void Serial_Amiga_Read_Adf(struct IOExtSer*);

int main(int argc,char** argv)
{
	UBYTE tmp[256] ;
	int cont=0;
	struct MsgPort *SerialMP;       /* pointer to our message port */
	struct IOExtSer* SerialIO;      /* pointer to our IORequest */
	struct VolumeInfo* vols;
	int terminate = 0;
	int contBytes = 0;
	int fileSize = 0;
	int bytesToRead = 0;
	VERBOSE=0;

	struct IOTArray Terminators =
	{
		0x04040403,   /*  etx eot */
		0x03030303    /* fill to end with lowest value */
	};

   	char SerialReadBuffer[READ_BUFFER_SIZE]; /* Reserve SIZE bytes */
	char FilenameReadBuffer[READ_BUFFER_SIZE];
	char FilesizeReadBuffer[READ_BUFFER_SIZE];
	char DataReadBuffer[CHUNK_DATA_READ];

	FILE* fh;

	// Set verbosity
	if (argc>1 && (!strcmp(argv[1],"-verbose")||!strcmp(argv[1],"-v")))
	{
		VERBOSE=1;
		printf("Verbosity set to %d\n",VERBOSE);
	}

	/* Create the message port */
	if (SerialMP=CreateMsgPort())
	{
    		/* Create the IORequest */
    		if (SerialIO = (struct IOExtSer *) CreateExtIO(SerialMP,sizeof(struct IOExtSer)))
        	{
        		/* Open the serial device */
        		if (OpenDevice(SERIALNAME,0,(struct IORequest *)SerialIO,0L))
            			printf("Error: %s did not open\n",SERIALNAME);
        		else
            		{
				// Start of cmd reading
				SerialIO->io_SerFlags |= SERF_EOFMODE;
				SerialIO->io_TermArray = Terminators;
				
				SerialIO->io_SerFlags      |= SERF_XDISABLED;
				SerialIO->io_Baud           = 19200;
				
				SerialIO->IOSer.io_Command  = SDCMD_SETPARAMS;
				if (DoIO((struct IORequest *)SerialIO))
                 			printf("Set Params failed ");   /* Inform user of error */
				else
				{
					while (terminate==0)
					{
						for (cont=0;cont<READ_BUFFER_SIZE;cont++)
                					SerialReadBuffer[cont]=0;
                		SerialIO->io_SerFlags |= SERF_EOFMODE;
						SerialIO->io_TermArray = Terminators;
						SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
		   				SerialIO->IOSer.io_Data     = (APTR)&SerialReadBuffer[0];
		   				SerialIO->IOSer.io_Command  = CMD_READ;
						if (VERBOSE) printf("Waiting for a %d characters command\n",READ_BUFFER_SIZE);
		   				DoIO((struct IORequest *)SerialIO);
						if (VERBOSE) printf("Recv : ##%s##\n",SerialReadBuffer);

						// Start of vols command to list all volumes of the Amiga
						if (SerialReadBuffer[0]==118 && SerialReadBuffer[1]==111 && SerialReadBuffer[2]==108 && SerialReadBuffer[3]==115 && SerialReadBuffer[4]==4)
						{
							Amiga_Send_vols(SerialIO,DLT_VOLUME);
						}
						// Start of 'device' command to list all  devices of the Amiga
						else if (SerialReadBuffer[0]=='d' && SerialReadBuffer[1]=='e' && SerialReadBuffer[2]=='v' && SerialReadBuffer[3]=='i' && SerialReadBuffer[4]=='c' && SerialReadBuffer[5]=='e' && SerialReadBuffer[6]==4)
						{
							Amiga_Send_vols(SerialIO,DLT_DEVICE);
						}
						// Start stat file or directory
						else if (SerialReadBuffer[0]=='s' && SerialReadBuffer[1]=='t' && SerialReadBuffer[2]=='a' && SerialReadBuffer[3]=='t' && SerialReadBuffer[4]==4)
						{
							for (cont=0;cont<READ_BUFFER_SIZE;cont++)
							{
								FilenameReadBuffer[cont]=0;
							}
							SendSerialMessage(SerialIO,"1","Getting filename");
							SendSerialEndOfData(SerialIO);
							SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
		   					SerialIO->IOSer.io_Command  = CMD_READ;
							SerialIO->IOSer.io_Data     = (APTR)&FilenameReadBuffer[0];
							DoIO((struct IORequest *)SerialIO);
							for (cont=0;cont<READ_BUFFER_SIZE;cont++)
							{
								if (FilenameReadBuffer[cont]==4) {FilenameReadBuffer[cont]=0;}
								
							}
							Serial_Amiga_Send_Stat(SerialIO,FilenameReadBuffer);
						}
						// Start content read of volume or directory (list cmd)
						else if (SerialReadBuffer[0]=='l' && SerialReadBuffer[1]=='i' && SerialReadBuffer[2]=='s' && SerialReadBuffer[3]=='t' && SerialReadBuffer[4]==4)
						{
							for (cont=0;cont<READ_BUFFER_SIZE;cont++)
							{
								FilenameReadBuffer[cont]=0;
							}
							SendSerialMessage(SerialIO,"1","Getting filename");
							SendSerialEndOfData(SerialIO);
							SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
		   					SerialIO->IOSer.io_Command  = CMD_READ;
							SerialIO->IOSer.io_Data     = (APTR)&FilenameReadBuffer[0];
							DoIO((struct IORequest *)SerialIO);
							for (cont=0;cont<READ_BUFFER_SIZE;cont++)
							{
								if (FilenameReadBuffer[cont]==4) {FilenameReadBuffer[cont]=0;}
								
							}
							Amiga_Send_List(SerialIO,FilenameReadBuffer);
						}

						// Start of store data procedure
						else if (SerialReadBuffer[0]=='s' && SerialReadBuffer[1]=='t' && SerialReadBuffer[2]=='o' && SerialReadBuffer[3]=='r' && SerialReadBuffer[4]=='e' && SerialReadBuffer[5]==4)
						{
							/* Init buffer with zeroes */
							for (cont=0;cont<READ_BUFFER_SIZE;cont++)
							{
								FilenameReadBuffer[cont]=0;
								FilesizeReadBuffer[cont]=0;
							}
							SendSerialMessage(SerialIO,"1","Getting filename");
							SendSerialEndOfData(SerialIO);

							// Read filename from serial port
							SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
		   					SerialIO->IOSer.io_Command  = CMD_READ;
							SerialIO->IOSer.io_Data     = (APTR)&FilenameReadBuffer[0];
							//printf("Nome del file completo di path\n");
		   					DoIO((struct IORequest *)SerialIO);
							printf("Received : ##%s##\n",FilenameReadBuffer);

							// Read filesize from serial port
							SendSerialMessage(SerialIO,"2","Getting filesize");
							SendSerialEndOfData(SerialIO);

							SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
		   					SerialIO->IOSer.io_Command  = CMD_READ;
							SerialIO->IOSer.io_Data     = (APTR)&FilesizeReadBuffer[0];
		   					DoIO((struct IORequest *)SerialIO);
							printf("Received : ##%s##\n",FilesizeReadBuffer);

							/*SerialIO->IOSer.io_Length   = -1;
				    			SerialIO->IOSer.io_Command  = CMD_WRITE;
							SerialIO->IOSer.io_Data     = (APTR)"OK";*/

							/*if (DoIO((struct IORequest *)SerialIO))     
									printf("Write failed.  Error - %d\n",SerialIO->IOSer.io_Error);*/
							// From now i am listening for incoming data
							// Disable terminators
							SendSerialMessage(SerialIO,"3","Getting binary data");
							SendSerialEndOfData(SerialIO);

							// Disable termination mode
							SerialIO->io_SerFlags &= ~ SERF_EOFMODE;
							//SerialIO->io_TermArray = NULL;
							SerialIO->IOSer.io_Command  = SDCMD_SETPARAMS;
							
							if (DoIO((struct IORequest *)SerialIO))
                 						printf("Set Params failed ");   /* Inform user of error */

							SerialIO->IOSer.io_Data     = (APTR)&DataReadBuffer[0];
							SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE;
							SerialIO->IOSer.io_Command  = CMD_READ;

							
							for (cont=0;cont<READ_BUFFER_SIZE;cont++)
							{
								if (FilenameReadBuffer[cont]==4) {FilenameReadBuffer[cont]=0;}
								
							}

							fh = fopen(FilenameReadBuffer, "wb");
							if (!fh)
							{							
								printf("Error in writing %s\n",FilenameReadBuffer);

							}
							else
							{							
								fileSize=atoi(FilesizeReadBuffer);
								contBytes=0;
							
								while (contBytes < fileSize )
								{
									if (contBytes+CHUNK_DATA_READ<fileSize) bytesToRead=CHUNK_DATA_READ;
									else bytesToRead=fileSize-contBytes;

									printf("contBytes : %d, fileSize : %d, bytesToRead : %d\n",contBytes,fileSize,bytesToRead);
									SerialIO->IOSer.io_Length   = bytesToRead;
									DoIO((struct IORequest *)SerialIO);
								
									if (fh)
									{
										fwrite(DataReadBuffer,bytesToRead,1,fh);
									}
								
									contBytes+=bytesToRead;
								}
								fclose(fh);
							}
							printf("Rimetto a posto il terminator mode\n");

							// Restore termination mode
							SerialIO->io_SerFlags |= SERF_EOFMODE;
							SerialIO->io_TermArray = Terminators;
							SerialIO->IOSer.io_Command  = SDCMD_SETPARAMS;
							if (DoIO((struct IORequest *)SerialIO))
					 			printf("Set Params failed ");   /* Inform user of error */


						}
						else if (SerialReadBuffer[0]=='s' && SerialReadBuffer[1]=='t' && SerialReadBuffer[2]=='o' && SerialReadBuffer[3]=='r' && SerialReadBuffer[4]=='e' && SerialReadBuffer[5]=='r' && SerialReadBuffer[6]=='a' && SerialReadBuffer[7]=='w' && SerialReadBuffer[8]==4)
						{
							Amiga_Store_Data(SerialIO);	
						}
						// Create empty file
						else if (SerialReadBuffer[0]=='c' && SerialReadBuffer[1]=='r' && SerialReadBuffer[2]=='e' && SerialReadBuffer[3]=='a' && SerialReadBuffer[4]=='t' && SerialReadBuffer[5]=='e' && SerialReadBuffer[6]=='f' && SerialReadBuffer[7]=='i' && SerialReadBuffer[8]=='l' && SerialReadBuffer[9]=='e' && SerialReadBuffer[10]==4)
						{
							Amiga_Create_Empty_File(SerialIO);
						}
						else if (SerialReadBuffer[0]=='m' && SerialReadBuffer[1]=='k' && SerialReadBuffer[2]=='d' && SerialReadBuffer[3]=='r' && SerialReadBuffer[4]=='a' && SerialReadBuffer[5]=='w' && SerialReadBuffer[6]=='e' && SerialReadBuffer[7]=='r' && SerialReadBuffer[8]==4)
						{
							Amiga_Create_Empty_Drawer(SerialIO);
						}
						else if (SerialReadBuffer[0]=='r' && SerialReadBuffer[1]=='e' && SerialReadBuffer[2]=='n' && SerialReadBuffer[3]=='a' && SerialReadBuffer[4]=='m' && SerialReadBuffer[5]=='e' && SerialReadBuffer[6]==4)
						{
							Amiga_Rename_File_Drawer(SerialIO);
						}
						else if (SerialReadBuffer[0]=='r' && SerialReadBuffer[1]=='e' && SerialReadBuffer[2]=='a' && SerialReadBuffer[3]=='d' && SerialReadBuffer[4]=='f' && SerialReadBuffer[5]=='i' && SerialReadBuffer[6]=='l' && SerialReadBuffer[7]=='e' && SerialReadBuffer[8]==4)
						{
							Amiga_Read_File(SerialIO);
						}
						else if (SerialReadBuffer[0]=='d' && SerialReadBuffer[1]=='e' && SerialReadBuffer[2]=='l' && SerialReadBuffer[3]=='a' && SerialReadBuffer[4]=='y' && SerialReadBuffer[5]==4)
						{
							Amiga_Delay(SerialIO);
						}
						else if (SerialReadBuffer[0]=='d' && SerialReadBuffer[1]=='e' && SerialReadBuffer[2]=='l' && SerialReadBuffer[3]=='e' && SerialReadBuffer[4]=='t' && SerialReadBuffer[5]=='e' && SerialReadBuffer[6]==4)
						{
							Amiga_Delete(SerialIO);
						}
						else if (SerialReadBuffer[0]=='w' && SerialReadBuffer[1]=='r' && SerialReadBuffer[2]=='i' && SerialReadBuffer[3]=='t' && SerialReadBuffer[4]=='e' && SerialReadBuffer[5]=='a' && SerialReadBuffer[6]=='d' && SerialReadBuffer[7]=='f' && SerialReadBuffer[8]==4)
						{
							Amiga_Write_Adf(SerialIO);
						}
						else if (SerialReadBuffer[0]=='r' && SerialReadBuffer[1]=='e' && SerialReadBuffer[2]=='a' && SerialReadBuffer[3]=='d' && SerialReadBuffer[4]=='a' && SerialReadBuffer[5]=='d' && SerialReadBuffer[6]=='f' && SerialReadBuffer[7]==4)
						{
							Serial_Amiga_Read_Adf(SerialIO);
						}
						// Check floppy drive presence 'chkfloppy command'
						else if (SerialReadBuffer[0]=='c' && SerialReadBuffer[1]=='h' && SerialReadBuffer[2]=='k' && SerialReadBuffer[3]=='f' && SerialReadBuffer[4]=='l' && SerialReadBuffer[5]=='o' && SerialReadBuffer[6]=='p' && SerialReadBuffer[7]=='p'&& SerialReadBuffer[8]=='y' && SerialReadBuffer[9]==4)
						{
							Serial_Check_Floppy_Drive(SerialIO);
						}
						else if (SerialReadBuffer[0]=='e' && SerialReadBuffer[1]=='x' && SerialReadBuffer[2]=='i' && SerialReadBuffer[3]=='t' && SerialReadBuffer[4]==4)
							terminate = 1;
						else
						{
							printf("Cmd %s not recognized\n",SerialReadBuffer);
							terminate = 1;
						}
					}
				}

			    /* Close the serial device */
			    CloseDevice((struct IORequest *)SerialIO);
            }
			/* Delete the IORequest */
			DeleteExtIO((struct IORequest *)SerialIO);
		}
		else
			fprintf(stderr,"Error: Could create IORequest\n");

    	/* Delete the message port */
    	DeleteMsgPort(SerialMP);
    }
	else
    	fprintf(stderr,"Error: Could not create message port\n");

	return 0;
}

void sendSerialEndOfData(struct IOExtSer* SerialIO)
{
	char buffer[2];
	buffer[0]=3;
	buffer[1]=0;
	SerialIO->IOSer.io_Length   = 1;
	SerialIO->IOSer.io_Command  = CMD_WRITE;
	SerialIO->IOSer.io_Data     = (APTR)&buffer[0];
	if (VERBOSE) printf("ETX (end of text) sent\n");
	if (DoIO((struct IORequest *)SerialIO)) printf("Write failed.  Error - %d\n",SerialIO->IOSer.io_Error);
	return ;
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

// Read all files and directory from a location
struct ContentInfo* getContentList(char* path)
{
	struct FileInfoBlock * FIB;
	BPTR lock;
	BOOL success;
	struct ContentInfo* newObj;
	struct ContentInfo* ptrHead;
	struct ContentInfo* volinfoHead=NULL;
	lock = Lock(path, ACCESS_READ);
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
			if (volinfoHead==NULL) volinfoHead=newObj;
			else
			{
				ptrHead=volinfoHead;
				while(ptrHead->next)
					ptrHead=ptrHead->next;
				ptrHead->next=newObj;
			}
			success = ExNext(lock, FIB);
		}
		FreeVec(FIB);
	}
	UnLock(lock);
	return volinfoHead;	
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

void Amiga_Send_vols(struct IOExtSer* SerialIO,int flag)
{
	struct VolumeInfo* ptr;
	struct VolumeInfo* freePtr;
	struct VolumeInfo* volinfoHead=NULL;

	volinfoHead=getVolumes(flag);
	ptr=volinfoHead;
	while (ptr)
	{
		//printf("%s\n",ptr->name);
		SendSerialMessage(SerialIO,ptr->name,ptr->name);
		SendSerialNewLine(SerialIO);
		ptr=ptr->next;
	}
	SendSerialEndOfData(SerialIO);

	ptr=volinfoHead;
	while (ptr!=NULL)
	{
		freePtr=ptr->next;
		free(ptr);
		ptr=freePtr;
	}

	return ;
}

// Rename file or drawer
void Amiga_Rename_File_Drawer(struct IOExtSer* SerialIO)
{
	char FilenameReadBuffer[SERIAL_BUFFER_SIZE];
	char FilenameReadBuffer2[SERIAL_BUFFER_SIZE];
	SerialRead(SerialIO,"1","Getting old filename",FilenameReadBuffer);
	SerialRead(SerialIO,"2","Getting new filename",FilenameReadBuffer2);
	
	if (!Rename(FilenameReadBuffer,FilenameReadBuffer2))
	{
		fprintf(stderr,"Error in renaming %s\n",FilenameReadBuffer);
		SendSerialMessage(SerialIO,"KO","KO");
	}
	else
	{
		SendSerialMessage(SerialIO,"OK","OK");
	}
	sendSerialEndOfData(SerialIO);	
	return ;
}


// Create an empty drawer
void Amiga_Create_Empty_Drawer(struct IOExtSer* SerialIO)
{
		BPTR lock;
		char FilenameReadBuffer[READ_BUFFER_SIZE];
		int cont;
		
		/* Init buffer with zeroes */
		for (cont=0;cont<READ_BUFFER_SIZE;cont++)
		{
			FilenameReadBuffer[cont]=0;
		}
		SendSerialMessage(SerialIO,"1","Getting filename");
		SendSerialEndOfData(SerialIO);

		// Read filename from serial port
		SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
		SerialIO->IOSer.io_Command  = CMD_READ;
		SerialIO->IOSer.io_Data     = (APTR)&FilenameReadBuffer[0];
		DoIO((struct IORequest *)SerialIO);
			printf("Received : ##%s##\n",FilenameReadBuffer);
	
		for (cont=0;cont<READ_BUFFER_SIZE;cont++)
		{
			if (FilenameReadBuffer[cont]==4) {FilenameReadBuffer[cont]=0;}
		}
		
		lock = CreateDir (FilenameReadBuffer);
		if (!lock)
		{
			printf("Error in creating %s\n",FilenameReadBuffer);
			SendSerialMessage(SerialIO,"KO","KO");
		}
		else
		{
			UnLock(lock);
			SendSerialMessage(SerialIO,"OK","OK");
		}
		sendSerialEndOfData(SerialIO);
		return ;
}

// Create an empty file
void Amiga_Create_Empty_File(struct IOExtSer* SerialIO)
{
	char FilenameReadBuffer[READ_BUFFER_SIZE];
	FILE* fh;
	int cont;
	
	/* Init buffer with zeroes */
	for (cont=0;cont<READ_BUFFER_SIZE;cont++)
	{
		FilenameReadBuffer[cont]=0;
	}
	SendSerialMessage(SerialIO,"1","Getting filename");
	SendSerialEndOfData(SerialIO);

	// Read filename from serial port
	SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
	SerialIO->IOSer.io_Command  = CMD_READ;
	SerialIO->IOSer.io_Data     = (APTR)&FilenameReadBuffer[0];
	DoIO((struct IORequest *)SerialIO);
	printf("Received : ##%s##\n",FilenameReadBuffer);
	
	for (cont=0;cont<READ_BUFFER_SIZE;cont++)
	{
		if (FilenameReadBuffer[cont]==4) {FilenameReadBuffer[cont]=0;}
	}

	fh = fopen(FilenameReadBuffer, "wb");
	if (!fh)
	{
		printf("Error in writing %s\n",FilenameReadBuffer);
		SendSerialMessage(SerialIO,"KO","KO");
	}
	else
	{							
		fclose(fh);
		SendSerialMessage(SerialIO,"OK","OK");
	}
	sendSerialEndOfData(SerialIO);
	return ;
}

// Create an empty file
void Amiga_Delete(struct IOExtSer* SerialIO)
{
	char FilenameReadBuffer[READ_BUFFER_SIZE];
	FILE* fh;
	int cont;
	BOOL outcome;
	
	/* Init buffer with zeroes */
	for (cont=0;cont<READ_BUFFER_SIZE;cont++)
	{
		FilenameReadBuffer[cont]=0;
	}
	SendSerialMessage(SerialIO,"1","Getting filename");
	SendSerialEndOfData(SerialIO);

	// Read filename from serial port
	SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
	SerialIO->IOSer.io_Command  = CMD_READ;
	SerialIO->IOSer.io_Data     = (APTR)&FilenameReadBuffer[0];
	DoIO((struct IORequest *)SerialIO);
	printf("Received : ##%s##\n",FilenameReadBuffer);
	
	for (cont=0;cont<READ_BUFFER_SIZE;cont++)
	{
		if (FilenameReadBuffer[cont]==4) {FilenameReadBuffer[cont]=0;}
	}

	outcome = DeleteFile(FilenameReadBuffer);
	if (!outcome)
	{
		printf("Error in deleting %s\n",FilenameReadBuffer);
		SendSerialMessage(SerialIO,"KO","KO");
	}
	else
	{							
		SendSerialMessage(SerialIO,"OK","OK");
	}
	sendSerialEndOfData(SerialIO);
	return ;
}

void Amiga_Delay(struct IOExtSer* SerialIO)
{
	char DelayReadBuffer[SERIAL_BUFFER_SIZE];
	SerialRead(SerialIO,"1","Delay",DelayReadBuffer);
	Delay(atoi(DelayReadBuffer));
	SendSerialMessage(SerialIO,"OK","Delay OK");
	SendSerialEndOfData(SerialIO);
	return ;
}

// Read adf file from floppy
void Serial_Amiga_Read_Adf(struct IOExtSer* SerialIO)
{
	UBYTE *buffer;
	char TrackDeviceReadBuffer[SERIAL_BUFFER_SIZE];
	char SizeReadBuffer[SERIAL_BUFFER_SIZE];
	char OffsetReadBuffer[SERIAL_BUFFER_SIZE];
	int size;
	int offset;
	int base64Size=0;
	u8* base64Data;
	int contBytes;
	int bytesToRead;

	SerialRead(SerialIO,"1","Getting trackdevice",TrackDeviceReadBuffer);
	SerialRead(SerialIO,"2","Getting size",SizeReadBuffer);
	SerialRead(SerialIO,"3","Getting offset",OffsetReadBuffer);

	size = atoi(SizeReadBuffer);
	offset = atoi(OffsetReadBuffer);

	SendSerialMessage(SerialIO,"4","Start of raw data sent");

	contBytes=0;
	while (contBytes < size )
	{

		// If i am at the last iteration i want to read only the last files and not the entire chunk
		if (contBytes+5632<size) bytesToRead=5632;
		else bytesToRead=size-contBytes;

		printf("Start reading data\n");fflush(stdout);
		buffer = AllocMem(bytesToRead, MEMF_CHIP);
		memset(buffer,0,bytesToRead);
		base64Size=Amiga_Read_Adf_Data(atoi(TrackDeviceReadBuffer),bytesToRead,offset+contBytes,&buffer);
		printf("End reading data\n");fflush(stdout);
		if (buffer==NULL)
		{
			SendSerialMessageAndEOD(SerialIO,"6","Drive not present or removed");
			return ;
		}

		base64Data = base64_encode((u8*)buffer, &base64Size);
		if (VERBOSE) printf("Base 64 data %d - %s\n",base64Size,base64Data);
		//if (VERBOSE) printf("Base 64 size %d, sending...",base64Size);

		// Send the encoded chunk via serial port followed by a newline
		for (int serialCont=0;serialCont<base64Size;serialCont++)
		{
			SerialIO->IOSer.io_Length   = 1;
			SerialIO->IOSer.io_Command  = CMD_WRITE;
			SerialIO->IOSer.io_Data     = (APTR)&base64Data[serialCont];
			if (DoIO((struct IORequest *)SerialIO))     /* execute write */
				printf("Write failed.  Error - %d\n",SerialIO->IOSer.io_Error);
		}
		if (VERBOSE) printf("Data sent!\n");

		SendSerialNewLine(SerialIO);
				
		free(base64Data);
		FreeMem(buffer,bytesToRead);
		contBytes+=bytesToRead;
	}
	SendSerialEndOfData(SerialIO);
}

// Read file from amiga fs
void Amiga_Read_File(struct IOExtSer* SerialIO)
{
	char FilenameReadBuffer[SERIAL_BUFFER_SIZE];
	char SizeReadBuffer[SERIAL_BUFFER_SIZE];
	char OffsetReadBuffer[SERIAL_BUFFER_SIZE];
	char* data;
	FILE* fh;
	int size;
	int offset;
	u8* base64Data;
	int fileSize;
	int contBytes;
	int bytesToRead;
	struct Amiga_Stat* stat;
	int fileRealLength;
	
	struct IOTArray Terminators =
	{
		0x04040403,   /*  etx eot */
		0x03030303    /* fill to end with lowest value */
	};

	SerialRead(SerialIO,"1","Getting old filename",FilenameReadBuffer);
	SerialRead(SerialIO,"2","Getting size",SizeReadBuffer);
	SerialRead(SerialIO,"3","Getting offset",OffsetReadBuffer);
	
	stat = Amiga_Get_Stat(FilenameReadBuffer);
	if (stat)
	{
		fileRealLength=(int)stat->st_size;
		free(stat);
		size = atoi(SizeReadBuffer);
		offset = atoi(OffsetReadBuffer);
		
		if (offset+size>fileRealLength)
		{
			size=fileRealLength-offset;
		}
		//printf("Size calculated in %d\n",size);
		if (offset>fileRealLength)
		{
			printf("Something went wrong, offset cant be greater than the file length\n");
			SendSerialEndOfData(SerialIO);
			return ;
		}
				
		// Disable termination mode
		SerialIO->io_SerFlags &= ~ SERF_EOFMODE;
		SerialIO->IOSer.io_Command  = SDCMD_SETPARAMS;
								
		if (DoIO((struct IORequest *)SerialIO))
			printf("Set Params failed ");   /* Inform user of error */
		
		fh = fopen(FilenameReadBuffer, "r");
		if (fh)
		{
			if  (offset>0) fseek(fh, offset, SEEK_SET);
			// size if the length in bytes of the requested data or the file size of the requested data exceedes the file length
			// from now i read data from the file in chunks, each chunk is CHUNK_DATA_READ bytes
			contBytes=0;
			while (contBytes < size )
			{
				// If i am at the last iteration i want to read only the last files and not the entire chunk
				if (contBytes+CHUNK_DATA_READ<size) bytesToRead=CHUNK_DATA_READ;
				else bytesToRead=size-contBytes;
				
				if (VERBOSE) printf("contBytes : %d, fileSize : %d, bytesToRead : %d\n",contBytes,size,bytesToRead);
				
				// point fh to skip bytes alread read
				if (contBytes>0) if (fseek(fh,offset+contBytes,SEEK_SET)==-1) {printf("Error in seeking at %d",offset+contBytes);exit(0);}
				
				// read bytesToRead bytes from file
				data=malloc(bytesToRead);
				fread(data,bytesToRead,1,fh);
				
				// encode the data read to base64 for transfer
				int base64Size=bytesToRead;
				base64Data = base64_encode((u8*)data, &base64Size);
				if (VERBOSE) printf("Base 64 data %d - %s\n",base64Size,base64Data);
				
				// Send the encoded chunk via serial port followed by a newline
				for (int serialCont=0;serialCont<base64Size;serialCont++)
				{
					SerialIO->IOSer.io_Length   = 1;
					SerialIO->IOSer.io_Command  = CMD_WRITE;
					SerialIO->IOSer.io_Data     = (APTR)&base64Data[serialCont];
					//printf("Sto per mandare %s\n",base64Data);
					if (DoIO((struct IORequest *)SerialIO))     /* execute write */
						printf("Write failed.  Error - %d\n",SerialIO->IOSer.io_Error);
					//printf("Mandato");
				}
				SendSerialNewLine(SerialIO);
				
				free(base64Data);
				free(data);
				contBytes+=bytesToRead;
			}
			
			fclose(fh);
			
			// Restore termination mode
			SerialIO->io_SerFlags |= SERF_EOFMODE;
			SerialIO->io_TermArray = Terminators;
			SerialIO->IOSer.io_Command  = SDCMD_SETPARAMS;
			if (DoIO((struct IORequest *)SerialIO))
				printf("Set Params failed ");   /* Inform user of error */
		}
	}
	else printf("File not found");
	SendSerialEndOfData(SerialIO);
	return ;
}

//Stores binary data to amiga fs
void Amiga_Store_Data(struct IOExtSer* SerialIO)
{
	char FilenameReadBuffer[READ_BUFFER_SIZE];
	char FilesizeReadBuffer[READ_BUFFER_SIZE];
	char AppendReadBuffer[READ_BUFFER_SIZE];
	char DataReadBuffer[CHUNK_DATA_READ];
	int cont=0;
	int contBytes = 0;
	int fileSize = 0;
	int bytesToRead = 0;
	FILE* fh;
	BPTR fd;

	struct IOTArray Terminators =
	{
		0x04040403,   /*  etx eot */
		0x03030303    /* fill to end with lowest value */
	};

	/* Init buffer with zeroes */
	for (cont=0;cont<READ_BUFFER_SIZE;cont++)
	{
		FilenameReadBuffer[cont]=0;
		FilesizeReadBuffer[cont]=0;
		AppendReadBuffer[cont]=0;
	}
	SendSerialMessage(SerialIO,"1","Getting filename");
	SendSerialEndOfData(SerialIO);

	// Read filename from serial port
	SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
	SerialIO->IOSer.io_Command  = CMD_READ;
	SerialIO->IOSer.io_Data     = (APTR)&FilenameReadBuffer[0];
	DoIO((struct IORequest *)SerialIO);
	if (VERBOSE) printf("Received : ##%s##\n",FilenameReadBuffer);

	// Read filesize from serial port
	SendSerialMessage(SerialIO,"2","Getting filesize");
	SendSerialEndOfData(SerialIO);

	SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
	SerialIO->IOSer.io_Command  = CMD_READ;
	SerialIO->IOSer.io_Data     = (APTR)&FilesizeReadBuffer[0];
	DoIO((struct IORequest *)SerialIO);
	if (VERBOSE) printf("Received : ##%s##\n",FilesizeReadBuffer);

	// Read append flag from serial port
	SendSerialMessage(SerialIO,"3","Getting append flag");
	SendSerialEndOfData(SerialIO);

	SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
	SerialIO->IOSer.io_Command  = CMD_READ;
	SerialIO->IOSer.io_Data     = (APTR)&AppendReadBuffer[0];
	DoIO((struct IORequest *)SerialIO);
	if (VERBOSE) printf("Received : ##%s##\n",AppendReadBuffer);

	// From now i am listening for incoming data
	SendSerialMessage(SerialIO,"4","Getting binary data");
	SendSerialEndOfData(SerialIO);

	// Disable termination mode
	SerialIO->io_SerFlags &= ~ SERF_EOFMODE;
	SerialIO->IOSer.io_Command  = SDCMD_SETPARAMS;
							
	if (DoIO((struct IORequest *)SerialIO))
    	printf("Set Params failed ");   /* Inform user of error */

	SerialIO->IOSer.io_Data     = (APTR)&DataReadBuffer[0];
	SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE;
	SerialIO->IOSer.io_Command  = CMD_READ;

	for (cont=0;cont<READ_BUFFER_SIZE;cont++)
	{
		if (FilenameReadBuffer[cont]==4) {FilenameReadBuffer[cont]=0;}
	}
	if (AppendReadBuffer[0]=='1')
	{
		fd = Open(FilenameReadBuffer, MODE_OLDFILE);
		Seek( fd, 0, OFFSET_END );
		printf("Trying to append...\n");
	}
	else
	{
		fd = Open(FilenameReadBuffer, MODE_NEWFILE);
	}
	/*if (AppendReadBuffer[0]=='1')
		fh = fopen(FilenameReadBuffer, "ab");
	else
		fh = fopen(FilenameReadBuffer, "wb");
	if (!fh)*/
	if (!fd)
	{							
		printf("Error in writing %s\n",FilenameReadBuffer);
	}
	else
	{							
		fileSize=atoi(FilesizeReadBuffer);
		contBytes=0;

		while (contBytes < fileSize )
		{
			if (contBytes+CHUNK_DATA_READ<fileSize) bytesToRead=CHUNK_DATA_READ;
			else bytesToRead=fileSize-contBytes;

			if (VERBOSE) printf("contBytes : %d, fileSize : %d, bytesToRead : %d\n",contBytes,fileSize,bytesToRead);
			for (int serialCont=0;serialCont<bytesToRead;serialCont++)
			{
				SerialIO->IOSer.io_Length   = 1;
				SerialIO->IOSer.io_Data     = (APTR)&DataReadBuffer[serialCont];
				SerialIO->IOSer.io_Command  = CMD_READ;
				while (DoIO((struct IORequest *)SerialIO))
				{
					printf("Read failed.  Error - %d\n",SerialIO->IOSer.io_Error);
					if (SerialIO->IOSer.io_Error==SerErr_BaudMismatch) printf("Baud mismatch\n");
				}
				if (SerialIO->IOSer.io_Actual!=1) printf("Error, character not read: %lu\n",SerialIO->IOSer.io_Actual);
			}

			//size_t bytesWritten=fwrite(DataReadBuffer,bytesToRead,1,fh);
			Write(fd,DataReadBuffer,bytesToRead);					
			contBytes+=bytesToRead;
		}
		//fclose(fh);
		Close(fd);
	}
	if (VERBOSE) printf("Rimetto a posto il terminator mode\n");
	
	// Restore termination mode
	SerialIO->io_SerFlags |= SERF_EOFMODE;
	SerialIO->io_TermArray = Terminators;
	SerialIO->IOSer.io_Command  = SDCMD_SETPARAMS;
	if (DoIO((struct IORequest *)SerialIO))
		printf("Set Params failed ");   /* Inform user of error */
		
	// Experimental
	SerialIO->IOSer.io_Flags=0;
	SerialIO->IOSer.io_Command  = CMD_CLEAR;
	if (DoIO((struct IORequest *)SerialIO))
		printf("Set clear failed ");
		
	SendSerialMessage(SerialIO,"5","Send OK");
        SendSerialEndOfData(SerialIO);
	return ;
}

// Send the content of a directory via serial cable
void Amiga_Send_List(struct IOExtSer* SerialIO,char* path)
{
	struct ContentInfo* ptr;
	struct ContentInfo* freePtr;
	struct ContentInfo* contentHead=NULL;

	contentHead=getContentList(path);
	ptr=contentHead;
	if (VERBOSE) printf("Start of sending content data...\n");
	while (ptr)
	{
		//printf("%s\n",ptr->name);
		SendSerialMessage(SerialIO,ptr->fileName,ptr->fileName);
		SendSerialNewLine(SerialIO);
		ptr=ptr->next;
	}
	if (VERBOSE) printf("End of sending content data...\n");
	SendSerialEndOfData(SerialIO);

	ptr=contentHead;
	while (ptr!=NULL)
	{
		freePtr=ptr->next;
		free(ptr);
		ptr=freePtr;
	}
	
	return ;
}

// Send the inode info of a file or directory via serial cable
void Serial_Amiga_Send_Stat(struct IOExtSer* SerialIO,char* path)
{
	char app[1000];
	int cont=0;

	struct Amiga_Stat* stat;
	stat = Amiga_Get_Stat(path);
	if (stat)
	{
		for (cont=0;cont<100;cont++) app[cont]=0;
		
		sprintf(app,"%ld",stat->st_size);
		if (VERBOSE) printf("Sent %s as size\n",app);
		SendSerialMessage(SerialIO,app,app);
		SendSerialNewLine(SerialIO);
		for (cont=0;cont<100;cont++) app[cont]=0;
		sprintf(app,"%ld",stat->st_blksize);
		if (VERBOSE) printf("Sent %s as block size\n",app);
		SendSerialMessage(SerialIO,app,app);
		SendSerialNewLine(SerialIO);
		
		sprintf(app,"%d",stat->directory);
		if (VERBOSE) printf("Sent %s as directory\n",app);
		SendSerialMessage(SerialIO,app,app);
		SendSerialNewLine(SerialIO);
		
		sprintf(app,"%ld",stat->days);
		if (VERBOSE) printf("Sent %s as days\n",app);
		SendSerialMessage(SerialIO,app,app);
		SendSerialNewLine(SerialIO);
		
		sprintf(app,"%ld",stat->minutes);
		if (VERBOSE) printf("Sent %s as minutes\n",app);
		SendSerialMessage(SerialIO,app,app);
		SendSerialNewLine(SerialIO);
		
		sprintf(app,"%ld",stat->seconds);
		if (VERBOSE) printf("Sent %s as seconds\n",app);
		SendSerialMessage(SerialIO,app,app);
		SendSerialNewLine(SerialIO);
		
		free(stat);
	}
	SendSerialEndOfData(SerialIO);

	return ;
}

void Serial_Check_Floppy_Drive(struct IOExtSer* SerialIO)
{
	char TrackDeviceReadBuffer[READ_BUFFER_SIZE];
	SendSerialMessageAndEOD(SerialIO,"1","Getting trackdevice");
	// Read trackdevice from serial port
	SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
	SerialIO->IOSer.io_Command  = CMD_READ;
	SerialIO->IOSer.io_Data     = (APTR)&TrackDeviceReadBuffer[0];
	DoIO((struct IORequest *)SerialIO);
	int trackDevice = atoi(TrackDeviceReadBuffer);
	if (trackDevice<MINTRACKDEVICES||trackDevice>MAXTRACKDEVICES)
	{
		SendSerialMessage(SerialIO,"6","Trakdevice number not entered correctly");
		SendSerialEndOfData(SerialIO);
		return;
	}

	int status = Amiga_Check_FloppyDisk_Presence(trackDevice);
	if (!status)
	{
		SendSerialMessageAndEOD(SerialIO,"7","Floppy disk inserted");
	}
	else if (status==1)
	{
		SendSerialMessageAndEOD(SerialIO,"8","Floppy disk not inserted");
	}
	else
	{
		SendSerialMessageAndEOD(SerialIO,"9","Drive not recognized");
	}
	return ;
}

// Writes an Amiga disk image to a trackdevice
void Amiga_Write_Adf(struct IOExtSer* SerialIO)
{
	char TrackDeviceReadBuffer[READ_BUFFER_SIZE];
	char StartTrackReadBuffer[READ_BUFFER_SIZE];
	char EndTrackReadBuffer[READ_BUFFER_SIZE];
	char DataReadBuffer[CHUNK_DATA_READ];
	int cont=0;
	int contBytes = 0;
	int fileSize = 0;
	int bytesToRead = 0;
	FILE* fh;
	BPTR fd;
	char command[100];
	UBYTE *buffer[11];
	int trackCounter = 0;
	int sectorCounter = 0;

	/* Init buffer with zeroes */
	for (cont=0;cont<READ_BUFFER_SIZE;cont++)
	{
		TrackDeviceReadBuffer[cont]=0;
		StartTrackReadBuffer[cont]=0;
		EndTrackReadBuffer[cont]=0;
	}
	SendSerialMessage(SerialIO,"1","Getting trackdevice");
	SendSerialEndOfData(SerialIO);

	// Read trackdevice from serial port
	SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
	SerialIO->IOSer.io_Command  = CMD_READ;
	SerialIO->IOSer.io_Data     = (APTR)&TrackDeviceReadBuffer[0];
	DoIO((struct IORequest *)SerialIO);
	if (VERBOSE) printf("Received : ##%s##\n",TrackDeviceReadBuffer);
	if (atoi(TrackDeviceReadBuffer)<MINTRACKDEVICES||atoi(TrackDeviceReadBuffer)>MAXTRACKDEVICES)
	{
		SendSerialMessage(SerialIO,"6","Trakdevice number not entered correctly");
		SendSerialEndOfData(SerialIO);
		return ;
	}

	// Read filesize from serial port
	SendSerialMessage(SerialIO,"2","Getting start");
	SendSerialEndOfData(SerialIO);

	SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
	SerialIO->IOSer.io_Command  = CMD_READ;
	SerialIO->IOSer.io_Data     = (APTR)&StartTrackReadBuffer[0];
	DoIO((struct IORequest *)SerialIO);
	if (VERBOSE) printf("Received : ##%s##\n",StartTrackReadBuffer);

	// Read append flag from serial port
	SendSerialMessage(SerialIO,"3","Getting end");
	SendSerialEndOfData(SerialIO);

	SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
	SerialIO->IOSer.io_Command  = CMD_READ;
	SerialIO->IOSer.io_Data     = (APTR)&EndTrackReadBuffer[0];
	DoIO((struct IORequest *)SerialIO);
	if (VERBOSE) printf("Received : ##%s##\n",EndTrackReadBuffer);

	// From now i am listening for incoming data
	sprintf(command,"4 0 %d",CHUNK_DATA_READ);
	SendSerialMessage(SerialIO,command,command);
	SendSerialEndOfData(SerialIO);

	// Disable termination mode
	DisableTerminationMode(SerialIO);

	for (cont=0;cont < 11 ; cont ++) buffer[cont]= AllocMem(512, MEMF_CHIP);


	//fileSize=901120;
	fileSize=(atoi(EndTrackReadBuffer)-atoi(StartTrackReadBuffer)+1)*(512*22);
	contBytes=0;

	while (contBytes < fileSize )
	{
		if (contBytes+CHUNK_DATA_READ<fileSize) bytesToRead=CHUNK_DATA_READ;
		else bytesToRead=fileSize-contBytes;

		if (VERBOSE) printf("contBytes : %d, fileSize : %d, bytesToRead : %d\n",contBytes,fileSize,bytesToRead);
		for (int serialCont=0;serialCont<bytesToRead;serialCont++)
		{
			SerialIO->IOSer.io_Length   = 1;
			SerialIO->IOSer.io_Data     = (APTR)&buffer[sectorCounter][serialCont%512];
			SerialIO->IOSer.io_Command  = CMD_READ;
			while (DoIO((struct IORequest *)SerialIO))
			{
				printf("Read failed.  Error - %d\n",SerialIO->IOSer.io_Error);
				if (SerialIO->IOSer.io_Error==SerErr_BaudMismatch) printf("Baud mismatch\n");
			}
			if (SerialIO->IOSer.io_Actual!=1) printf("Error, character not read: %lu\n",SerialIO->IOSer.io_Actual);
		}

		contBytes+=bytesToRead;
		sectorCounter++;
		if (contBytes%5632==0)
		{
			// flush to sector
			if (VERBOSE) printf("Start writing track %d\n",(atoi(StartTrackReadBuffer)*2)+trackCounter);
			Amiga_Write_Adf_Track((atoi(StartTrackReadBuffer)*2)+trackCounter,buffer,atoi(TrackDeviceReadBuffer));
			trackCounter++;
			sectorCounter=0;
		}

		if (contBytes < fileSize)
		{
			sprintf(command,"4 %d %d",contBytes,CHUNK_DATA_READ);
			SendSerialMessage(SerialIO,command,command);
			sendSerialEndOfData(SerialIO);
		}
	}

	for (cont=0;cont < 11 ; cont ++) FreeMem(buffer[cont], 512);
	
	// Restore termination mode
	EnableTerminationMode(SerialIO);
	
	//Clear internal buffers
	SendClear(SerialIO);

	// Send confirmation
	SendSerialMessage(SerialIO,"5","Send OK");
    sendSerialEndOfData(SerialIO);
	return ;
}