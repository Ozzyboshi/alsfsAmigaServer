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


#define READ_BUFFER_SIZE 256
//#define CHUNK_DATA_READ 256 // Chunk of bytes read from serial interface while file transfer
#define CHUNK_DATA_READ 256

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
struct VolumeInfo* getVolumes();
struct ContentInfo* getContentList(char*);
struct Amiga_Stat* Amiga_Get_Stat(char*);

void sendSerialMessage(struct IOExtSer*,char*,char*);
void sendSerialEndOfData(struct IOExtSer*);
void sendSerialNewLine(struct IOExtSer*);

// Serial functions
void Amiga_Store_Data(struct IOExtSer*);
void Amiga_Create_Empty_File(struct IOExtSer*);
void Amiga_Send_vols(struct IOExtSer*);
void Amiga_Send_List(struct IOExtSer*,char*);
void Serial_Amiga_Send_Stat(struct IOExtSer*,char*);

int VERBOSE=0;

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
				SerialIO->IOSer.io_Command  = SDCMD_SETPARAMS;
				if (DoIO((struct IORequest *)SerialIO))
                 			printf("Set Params failed ");   /* Inform user of error */
				else
				{
					while (terminate==0)
					{
						for (cont=0;cont<READ_BUFFER_SIZE;cont++)
                					SerialReadBuffer[cont]=0;
						SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
		   				SerialIO->IOSer.io_Data     = (APTR)&SerialReadBuffer[0];
		   				SerialIO->IOSer.io_Command  = CMD_READ;
						if (VERBOSE) printf("Waiting for a %d characters command\n",READ_BUFFER_SIZE);
		   				DoIO((struct IORequest *)SerialIO);
						if (VERBOSE) printf("Recv : ##%s##\n",SerialReadBuffer);

						// Start of vols command to list all volumes of the Amiga
						if (SerialReadBuffer[0]==118 && SerialReadBuffer[1]==111 && SerialReadBuffer[2]==108 && SerialReadBuffer[3]==115 && SerialReadBuffer[4]==4)
						{
							Amiga_Send_vols(SerialIO);
						}
						// Start stat file or directory
						else if (SerialReadBuffer[0]=='s' && SerialReadBuffer[1]=='t' && SerialReadBuffer[2]=='a' && SerialReadBuffer[3]=='t' && SerialReadBuffer[4]==4)
						{
							for (cont=0;cont<READ_BUFFER_SIZE;cont++)
							{
								FilenameReadBuffer[cont]=0;
							}
							sendSerialMessage(SerialIO,"1","Getting filename");
							sendSerialEndOfData(SerialIO);
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
							sendSerialMessage(SerialIO,"1","Getting filename");
							sendSerialEndOfData(SerialIO);
							SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
		   					SerialIO->IOSer.io_Command  = CMD_READ;
							SerialIO->IOSer.io_Data     = (APTR)&FilenameReadBuffer[0];
							DoIO((struct IORequest *)SerialIO);
							for (cont=0;cont<READ_BUFFER_SIZE;cont++)
							{
								if (FilenameReadBuffer[cont]==4) {FilenameReadBuffer[cont]=0;printf("corretto il nome del file");}
								
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
							sendSerialMessage(SerialIO,"1","Getting filename");
							sendSerialEndOfData(SerialIO);

							// Read filename from serial port
							SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
		   					SerialIO->IOSer.io_Command  = CMD_READ;
							SerialIO->IOSer.io_Data     = (APTR)&FilenameReadBuffer[0];
							//printf("Nome del file completo di path\n");
		   					DoIO((struct IORequest *)SerialIO);
							printf("Received : ##%s##\n",FilenameReadBuffer);

							// Read filesize from serial port
							sendSerialMessage(SerialIO,"2","Getting filesize");
							sendSerialEndOfData(SerialIO);

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
							sendSerialMessage(SerialIO,"3","Getting binary data");
							sendSerialEndOfData(SerialIO);

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
								if (FilenameReadBuffer[cont]==4) {FilenameReadBuffer[cont]=0;printf("corretto il nome del file\n");}
								
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
						else if (SerialReadBuffer[0]=='e' && SerialReadBuffer[1]=='x' && SerialReadBuffer[2]=='i' && SerialReadBuffer[3]=='t' && SerialReadBuffer[4]==4)
							terminate = 1;
						else
						{
							printf("Cmd %s not recognized\n",SerialReadBuffer);
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
			printf("Error: Could create IORequest\n");

    		/* Delete the message port */
    		DeleteMsgPort(SerialMP);
    	}
	else
    		printf("Error: Could not create message port\n");

	return 0;
}

// Sends a string via serial interfaces
void sendSerialMessage(struct IOExtSer* SerialIO,char* msg,char* debugMsg)
{
	char chunk[41];
	int i=0;
	
	//SerialIO->IOSer.io_Data     = (APTR)msg;
	if (debugMsg && VERBOSE) printf("%s\n",debugMsg);
	for (i=0;i<=(int)((strlen(msg)-1)/40);i++)
	{
		SerialIO->IOSer.io_Length   = -1;
		SerialIO->IOSer.io_Command  = CMD_WRITE;
		snprintf(chunk,40,"%s",&msg[i*40]);
		SerialIO->IOSer.io_Data     = (APTR)chunk;
		if (DoIO((struct IORequest *)SerialIO))     /* execute write */
			printf("Write failed.  Error - %d\n",SerialIO->IOSer.io_Error);

	}
	return ;
}
void sendSerialEndOfData(struct IOExtSer* SerialIO)
{
	char buffer[2];
	buffer[0]=3;
	buffer[1]=0;
	SerialIO->IOSer.io_Length   = 1;
	SerialIO->IOSer.io_Command  = CMD_WRITE;
	SerialIO->IOSer.io_Data     = (APTR)&buffer[0];
	if (VERBOSE) printf("ETX (end of text) sent");
	if (DoIO((struct IORequest *)SerialIO)) printf("Write failed.  Error - %d\n",SerialIO->IOSer.io_Error);
	return ;
}
void sendSerialNewLine(struct IOExtSer* SerialIO)
{
	SerialIO->IOSer.io_Length   = 1;
	SerialIO->IOSer.io_Command  = CMD_WRITE;
	SerialIO->IOSer.io_Data     = (APTR)"\n";
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
struct VolumeInfo* getVolumes()
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
						if (navigator->dvi_Type==DLT_VOLUME)
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


void Amiga_Send_vols(struct IOExtSer* SerialIO)
{
	struct VolumeInfo* ptr;
	struct VolumeInfo* freePtr;
	struct VolumeInfo* volinfoHead=NULL;

	volinfoHead=getVolumes();
	ptr=volinfoHead;
	while (ptr)
	{
		//printf("%s\n",ptr->name);
		sendSerialMessage(SerialIO,ptr->name,ptr->name);
		sendSerialNewLine(SerialIO);
		ptr=ptr->next;
	}
	sendSerialEndOfData(SerialIO);

	ptr=volinfoHead;
	while (ptr!=NULL)
	{
		freePtr=ptr->next;
		free(ptr);
		ptr=freePtr;
	}

	return ;
}

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
	sendSerialMessage(SerialIO,"1","Getting filename");
	sendSerialEndOfData(SerialIO);

	// Read filename from serial port
	SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
	SerialIO->IOSer.io_Command  = CMD_READ;
	SerialIO->IOSer.io_Data     = (APTR)&FilenameReadBuffer[0];
	DoIO((struct IORequest *)SerialIO);
	printf("Received : ##%s##\n",FilenameReadBuffer);
	
	for (cont=0;cont<READ_BUFFER_SIZE;cont++)
	{
		if (FilenameReadBuffer[cont]==4) {FilenameReadBuffer[cont]=0;printf("corretto il nome del file\n");}
	}

	fh = fopen(FilenameReadBuffer, "wb");
	if (!fh)
	{
		printf("Error in writing %s\n",FilenameReadBuffer);
		sendSerialMessage(SerialIO,"KO","KO");
	}
	else
	{							
		fclose(fh);
		sendSerialMessage(SerialIO,"OK","OK");
	}
	sendSerialEndOfData(SerialIO);
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
	sendSerialMessage(SerialIO,"1","Getting filename");
	sendSerialEndOfData(SerialIO);

	// Read filename from serial port
	SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
	SerialIO->IOSer.io_Command  = CMD_READ;
	SerialIO->IOSer.io_Data     = (APTR)&FilenameReadBuffer[0];
	DoIO((struct IORequest *)SerialIO);
	if (VERBOSE) printf("Received : ##%s##\n",FilenameReadBuffer);

	// Read filesize from serial port
	sendSerialMessage(SerialIO,"2","Getting filesize");
	sendSerialEndOfData(SerialIO);

	SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
	SerialIO->IOSer.io_Command  = CMD_READ;
	SerialIO->IOSer.io_Data     = (APTR)&FilesizeReadBuffer[0];
	DoIO((struct IORequest *)SerialIO);
	if (VERBOSE) printf("Received : ##%s##\n",FilesizeReadBuffer);

	// Read append flag from serial port
	sendSerialMessage(SerialIO,"3","Getting append flag");
	sendSerialEndOfData(SerialIO);

	SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
	SerialIO->IOSer.io_Command  = CMD_READ;
	SerialIO->IOSer.io_Data     = (APTR)&AppendReadBuffer[0];
	DoIO((struct IORequest *)SerialIO);
	if (VERBOSE) printf("Received : ##%s##\n",AppendReadBuffer);
	

	// From now i am listening for incoming data
	sendSerialMessage(SerialIO,"4","Getting binary data");
	sendSerialEndOfData(SerialIO);

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
		if (FilenameReadBuffer[cont]==4) {FilenameReadBuffer[cont]=0;if (VERBOSE) printf("corretto il nome del file\n");}
	}

	if (AppendReadBuffer[0]=='1')
		fh = fopen(FilenameReadBuffer, "ab");
	else
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

			if (VERBOSE) printf("contBytes : %d, fileSize : %d, bytesToRead : %d\n",contBytes,fileSize,bytesToRead);
			SerialIO->IOSer.io_Length   = bytesToRead;
			SerialIO->IOSer.io_Data     = (APTR)&DataReadBuffer[0];
			SerialIO->IOSer.io_Command  = CMD_READ;
			DoIO((struct IORequest *)SerialIO);
								
			if (fh)
			{
				fwrite(DataReadBuffer,bytesToRead,1,fh);
			}
								
			contBytes+=bytesToRead;
		}
		fclose(fh);
	}
	if (VERBOSE) printf("Rimetto a posto il terminator mode\n");

	// Restore termination mode
	SerialIO->io_SerFlags |= SERF_EOFMODE;
	SerialIO->io_TermArray = Terminators;
	SerialIO->IOSer.io_Command  = SDCMD_SETPARAMS;
	if (DoIO((struct IORequest *)SerialIO))
		printf("Set Params failed ");   /* Inform user of error */
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
		sendSerialMessage(SerialIO,ptr->fileName,ptr->fileName);
		sendSerialNewLine(SerialIO);
		ptr=ptr->next;
	}
	if (VERBOSE) printf("End of sending content data...\n");
	sendSerialEndOfData(SerialIO);

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
		if (VERBOSE) printf("Spedisco %s come size\n",app);
		sendSerialMessage(SerialIO,app,app);
		sendSerialNewLine(SerialIO);
		for (cont=0;cont<100;cont++) app[cont]=0;
		sprintf(app,"%ld",stat->st_blksize);
		if (VERBOSE) printf("Spedisco %s come block size\n",app);
		sendSerialMessage(SerialIO,app,app);
		sendSerialNewLine(SerialIO);
		
		sprintf(app,"%d",stat->directory);
		if (VERBOSE) printf("Spedisco %s come directory\n",app);
		sendSerialMessage(SerialIO,app,app);
		sendSerialNewLine(SerialIO);
		
		sprintf(app,"%ld",stat->days);
		if (VERBOSE) printf("Spedisco %s come days\n",app);
		sendSerialMessage(SerialIO,app,app);
		sendSerialNewLine(SerialIO);
		
		sprintf(app,"%ld",stat->seconds);
		if (VERBOSE) printf("Spedisco %s come seconds\n",app);
		sendSerialMessage(SerialIO,app,app);
		sendSerialNewLine(SerialIO);
		
		free(stat);
	}
	sendSerialEndOfData(SerialIO);

	return ;
}
