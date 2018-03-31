/*
 * alsfssrv.c
 *
 * This is the alsfs server meant to be run on the Amiga.
 * It stays in memory as a daemon and responds to the Nodejs http server requests
 * using the amiga serial port.
 * 
 * Tested on a real Amiga 600 unexpanded
 *
 * Compile with vbcc, to setup a compile environment quickly use docker and ozzyboshi/vbcc image.
 * for example : docker run -it -v #path where the sources are#:/data --rm  ozzyboshi/dockeramigavbcc make -f /data/Makefile
 * 
 * This program should work in any workbench release however it is tested only with Workbench 2.1 for now
 * Use the -verobose comman line flag to ativate the debug messages
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

// Serial functions
void Amiga_Store_Data(struct IOExtSer*);
void Amiga_Create_Empty_File(struct IOExtSer*);
void Amiga_Delete(struct IOExtSer*);
void Amiga_Create_Empty_Drawer(struct IOExtSer*);
void Amiga_Rename_File_Drawer(struct IOExtSer*);
void Amiga_Send_vols(struct IOExtSer*,const int);
void Serial_Amiga_Send_Stat(struct IOExtSer*,char*);
void Serial_Amiga_Send_Statfs(struct IOExtSer*,const char* const);
void Amiga_Read_File(struct IOExtSer*);
void Amiga_Delay(struct IOExtSer*);
void Amiga_Write_Adf(struct IOExtSer*);
void Amiga_Send_List(struct IOExtSer*,char*);

void Serial_Check_Floppy_Drive(struct IOExtSer*);
void Serial_Relabel_Volume(struct IOExtSer*);
void Serial_Amiga_Read_Adf(struct IOExtSer*);
void Serial_Read_Content(struct IOExtSer*);
void Amiga_Keypress(struct IOExtSer*,const char,const char,const char,const char);

int main(int argc,char** argv)
{
	UBYTE tmp[256] ;
	int cont=0;
	struct MsgPort *SerialMP;       /* pointer to our message port */
	struct IOExtSer* SerialIO;      /* pointer to our IORequest */
	int terminate = 0;

	char SerialReadBuffer[READ_BUFFER_SIZE]; /* Reserve SIZE bytes */
	char FilenameReadBuffer[READ_BUFFER_SIZE];
	char FilesizeReadBuffer[READ_BUFFER_SIZE];

	// Set verbosity
	if (argc>1 && (!strcmp(argv[1],"-verbose")||!strcmp(argv[1],"-v")))
	{
		VERBOSE=1;
		printf("Verbosity set to %d\n",VERBOSE);
	}
	else 	VERBOSE=0;

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
				SendClear(SerialIO);
			
				// Start of cmd reading
				SerialIO->io_SerFlags |= SERF_EOFMODE;
				SerialIO->io_TermArray = TERMINATORS_CHARACTERS;				
				SerialIO->io_SerFlags      |= SERF_XDISABLED;
				SerialIO->io_Baud           = 19200;
			
				SerialIO->IOSer.io_Command  = SDCMD_SETPARAMS;
				if (DoIO((struct IORequest *)SerialIO))
			   			printf("Set Params failed ");   /* Inform user of error */
				else
				{
					while (terminate==0)
					{
						for (cont=0;cont<READ_BUFFER_SIZE;cont++) SerialReadBuffer[cont]=0;
			    			SerialIO->io_SerFlags |= SERF_EOFMODE;

						SerialIO->io_TermArray = TERMINATORS_CHARACTERS;
						SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE-1;
			   			SerialIO->IOSer.io_Data     = (APTR)&SerialReadBuffer[0];
			   			SerialIO->IOSer.io_Command  = CMD_READ;

						if (VERBOSE) printf("Waiting for a %d characters command - debug %d\n",READ_BUFFER_SIZE,VERBOSE);
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
							SerialRead(SerialIO,"1","Getting filename",FilenameReadBuffer);
							Serial_Amiga_Send_Stat(SerialIO,FilenameReadBuffer);
						}
						// Start content read of volume or directory (list cmd)
						else if (SerialReadBuffer[0]=='l' && SerialReadBuffer[1]=='i' && SerialReadBuffer[2]=='s' && SerialReadBuffer[3]=='t' && SerialReadBuffer[4]==4)
						{
							Serial_Read_Content(SerialIO);
						}
						// Store raw data
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
							// Relabel volume
						else if (SerialReadBuffer[0]=='r' && SerialReadBuffer[1]=='e' && SerialReadBuffer[2]=='l' && SerialReadBuffer[3]=='a' && SerialReadBuffer[4]=='b' && SerialReadBuffer[5]=='e' && SerialReadBuffer[6]=='l' && SerialReadBuffer[7]==4)
						{
							Serial_Relabel_Volume(SerialIO);
						}
						// Emulate keyboard press
						else if (SerialReadBuffer[0]=='k' && SerialReadBuffer[1]=='e' && SerialReadBuffer[2]=='y' && SerialReadBuffer[3]=='p' && SerialReadBuffer[4]=='r' && SerialReadBuffer[5]=='e' && SerialReadBuffer[6]=='s' && SerialReadBuffer[7]=='s' && SerialReadBuffer[12]==4)
						{
							Amiga_Keypress(SerialIO,SerialReadBuffer[8],SerialReadBuffer[9],SerialReadBuffer[10],SerialReadBuffer[11]);
						}
						// Start stat filesystem
						else if (SerialReadBuffer[0]=='s' && SerialReadBuffer[1]=='t' && SerialReadBuffer[2]=='a' && SerialReadBuffer[3]=='t' && SerialReadBuffer[4]=='f' && SerialReadBuffer[5]=='s' && SerialReadBuffer[6]==4)
						{
							SerialRead(SerialIO,"1","Getting filename",FilenameReadBuffer);
							Serial_Amiga_Send_Statfs(SerialIO,(const char* const)FilenameReadBuffer);
						}
						else if (SerialReadBuffer[0]=='e' && SerialReadBuffer[1]=='x' && SerialReadBuffer[2]=='i' && SerialReadBuffer[3]=='t' && SerialReadBuffer[4]==4)
							terminate = 1;
						else if (strlen(SerialReadBuffer)>0)
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
		else fprintf(stderr,"Error: Could create IORequest\n");

		/* Delete the message port */
		DeleteMsgPort(SerialMP);
	}
	else fprintf(stderr,"Error: Could not create message port\n");
	return 0;
}

void Amiga_Keypress(struct IOExtSer* SerialIO,const char x,const char y,const char a,const char b)
{
	const int time=5;
	char keycode[3];
	char keycode2[3];
	sprintf(keycode,"%c%c",x,y);
	sprintf(keycode2,"%c%c",a,b);

	int keycodeint=atoi(keycode);
	int keycode2int=atoi(keycode2);

	if (keycode2int>=0) Amiga_Simulate_Keypress(keycode2int,5,0);
	Amiga_Simulate_Keypress(keycodeint,5,0);
	Amiga_Simulate_Keypress(keycodeint,5,1);
	if (keycode2int>=0) Amiga_Simulate_Keypress(keycode2int,5,1);

	SendSerialEndOfData(SerialIO);
	return;
	
	/*Disable();
	volatile UBYTE* boh = (volatile UBYTE*) 0xbfe401;
	volatile UBYTE* boh2 = (volatile UBYTE*) 0xbfe501;
	volatile UBYTE* boh3 = (volatile UBYTE*) 0xbfee01;
	volatile UBYTE* boh4 = (volatile UBYTE*) 0xbfec01;
	*boh=10;
	*boh2=0;
	*boh3=65;*/
	//*boh4=~(0x20<<1+0);
	/**boh4=~(atoi(keycode)<<1+0);
	Delay(10);
	Enable();
	Disable();
	*boh=10;
	*boh2=0;
	*boh3=65;*/
	//*boh4=~((0x20<<1)+1);
	//*boh4=~((atoi(keycode)<<1)+1);
	/*Delay(10);
	Enable();
	SendSerialEndOfData(SerialIO);
	return;*/
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
	char VolumeBuffer[SERIAL_BUFFER_SIZE];
	char VolumeBuffer2[SERIAL_BUFFER_SIZE];
	char cmdCopy[20+SERIAL_BUFFER_SIZE*2];
	BOOL result;

	SerialRead(SerialIO,"1","Getting old filename",FilenameReadBuffer);
	SerialRead(SerialIO,"2","Getting new filename",FilenameReadBuffer2);

	getVolumeName(FilenameReadBuffer,SERIAL_BUFFER_SIZE, VolumeBuffer);
	getVolumeName(FilenameReadBuffer2,SERIAL_BUFFER_SIZE, VolumeBuffer2);

	if (strcmp(VolumeBuffer,VolumeBuffer2))
	{
		sprintf(cmdCopy,"COPY \"%s\" TO \"%s\" ALL CLONE QUIET",FilenameReadBuffer,FilenameReadBuffer2);
		if (VERBOSE) printf("%s",cmdCopy);
		result = system(cmdCopy);
		if (VERBOSE) printf(result ? "true" : "false");
		if (result)
		{
			fprintf(stderr,"Error in renaming %s\n",FilenameReadBuffer);
			SendSerialMessage(SerialIO,"KO","KO");
		}
		else
		{
			sprintf(cmdCopy,"DELETE \"%s\" FORCE QUIET ALL",FilenameReadBuffer);
			system(cmdCopy);
			SendSerialMessage(SerialIO,"OK","OK");
		}
	}
	else
	{
		if (!Rename(FilenameReadBuffer,FilenameReadBuffer2))
		{
			fprintf(stderr,"Error in renaming %s\n",FilenameReadBuffer);
			SendSerialMessage(SerialIO,"KO","KO");
		}
		else
		{
			SendSerialMessage(SerialIO,"OK","OK");
		}
	}
	SendSerialEndOfData(SerialIO);	
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
	if (VERBOSE) printf("Received : ##%s##\n",FilenameReadBuffer);
	
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
	SendSerialEndOfData(SerialIO);
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
	if (VERBOSE) printf("Received : ##%s##\n",FilenameReadBuffer);
	
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
	SendSerialEndOfData(SerialIO);
	return ;
}

// Delete a file
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
	if (VERBOSE) printf("Received : ##%s##\n",FilenameReadBuffer);
	
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
	SendSerialEndOfData(SerialIO);
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

// Read content for a drawer
void Serial_Read_Content(struct IOExtSer* SerialIO)
{
	char FilenameReadBuffer[SERIAL_BUFFER_SIZE];
	SerialRead(SerialIO,"1","Getting filename",FilenameReadBuffer);
	Amiga_Send_List(SerialIO,FilenameReadBuffer);
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
			
			EnableTerminationMode(SerialIO);
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
	int errorDetected=0;

	SerialRead(SerialIO,"1","Getting filename",FilenameReadBuffer);
	SerialRead(SerialIO,"2","Getting filesize",FilesizeReadBuffer);
	SerialRead(SerialIO,"3","Getting append flag",AppendReadBuffer);

	// From now i am listening for incoming data
	SendSerialMessageAndEOD(SerialIO,"4","Getting binary data");
	DisableTerminationMode(SerialIO);

	for (cont=0;cont<READ_BUFFER_SIZE;cont++)
	{
		if (FilenameReadBuffer[cont]==4) {FilenameReadBuffer[cont]=0;}
	}
	if (AppendReadBuffer[0]=='1')
	{
		fd = Open(FilenameReadBuffer, MODE_OLDFILE);
		Seek( fd, 0, OFFSET_END );
		if (VERBOSE) printf("Trying to append...\n");
	}
	else
	{
		fd = Open(FilenameReadBuffer, MODE_NEWFILE);
	}
	if (!fd)
	{							
		printf("Error in writing %s\n",FilenameReadBuffer);
	}
	else
	{							
		fileSize=atoi(FilesizeReadBuffer);
		contBytes=0;

		while (errorDetected==0 && (contBytes < fileSize) )
		{
			if (contBytes+CHUNK_DATA_READ<fileSize) bytesToRead=CHUNK_DATA_READ;
			else bytesToRead=fileSize-contBytes;

			if (VERBOSE) printf("contBytes : %d, fileSize : %d, bytesToRead : %d\n",contBytes,fileSize,bytesToRead);
			for (int serialCont=0;errorDetected==0&&(serialCont<bytesToRead);serialCont++)
			{
				SerialIO->IOSer.io_Length   = 1;
				SerialIO->IOSer.io_Data     = (APTR)&DataReadBuffer[serialCont];
				SerialIO->IOSer.io_Command  = CMD_READ;
				if (DoIO((struct IORequest *)SerialIO))
				{
					printf("Read failed.  Error - %d\n",SerialIO->IOSer.io_Error);
					if (SerialIO->IOSer.io_Error==SerErr_BaudMismatch) fprintf(stderr,"Baud mismatch\n");
					else if (SerialIO->IOSer.io_Error==SerErr_DevBusy) fprintf(stderr,"The Serial device internal routines were busy\n");
					else if (SerialIO->IOSer.io_Error==SerErr_InvParam) fprintf(stderr,"A task specified an invalid parameter in the IOExtSer structure\n");
					else if (SerialIO->IOSer.io_Error==SerErr_LineErr) fprintf(stderr,"Bad electrical connection\n");
					//else if (SerialIO->IOSer.io_Error==SerErr_NotOpen) printf("Serial device not open\n");
					else if (SerialIO->IOSer.io_Error==SerErr_BufOverflow) fprintf(stderr,"The task-defined read butTer has overflowed\n");
					//else if (SerialIO->IOSer.io_Error==SerErr_InvBaud) printf("The specified baud rate is invalid\n");
					else if (SerialIO->IOSer.io_Error==SerErr_NoDSR) fprintf(stderr,"Data set ready signal was sent during data transfer\n");
					//else if (SerialIO->IOSer.io_Error==SerErr_NoCTS) printf("Clear to send signal was sent during data transfer\n");
					else if (SerialIO->IOSer.io_Error==SerErr_DetectedBreak) fprintf(stderr,"The system detected a queued or immediate break signal during data transfer\n");
					errorDetected=1;
				}
				if (SerialIO->IOSer.io_Actual!=1) printf("Error, character not read: %lu\n",SerialIO->IOSer.io_Actual);
			}

			if (errorDetected==0) Write(fd,DataReadBuffer,bytesToRead);					
			contBytes+=bytesToRead;
		}
		Close(fd);
	}
	EnableTerminationMode(SerialIO);
	if (errorDetected==0)		
		SendSerialMessage(SerialIO,"5","Send OK");
	else
	{
		int tries=0;
		int continueFlushing=1;
		while (continueFlushing==1)
		{
			Delay(50);
			SendFlush(SerialIO);
			SendClear(SerialIO)	;
						
			int charsInBuffer=QuerySerialDeviceCharsLeft(SerialIO);
			if (charsInBuffer==0)
			{
				if (VERBOSE) printf("No more chars detected... tries %d\n",tries);
				tries++;
			}
			else
			{
				if (VERBOSE) printf("Resetting tries...");
				tries=0;
			}
			if (tries>10)
			{
				SendSerialMessage(SerialIO,"6","Send KO");
				continueFlushing=0;
			}
		}

	}
	SendClear(SerialIO)	;
	SendSerialEndOfData(SerialIO);

	return ;
}

// Send the inode info of a file or directory via serial cable
void Serial_Amiga_Send_Stat(struct IOExtSer* SerialIO,char* path)
{
	char app[1000];
	int cont=0;
	int found=0;
	struct VolumeInfo* ptr;
	struct VolumeInfo* freePtr;
	struct VolumeInfo* volinfoHead=NULL;

	// Check if the volume exists
	while (path[cont]!=':' && path[cont]!=0)
		cont++;
	snprintf(app,cont+1,"%s",path);
	volinfoHead=getVolumes(DLT_VOLUME);
	ptr=volinfoHead;
	while (!found&&ptr)
	{
		if (!strcmp(app,ptr->name))
			found=1;
		ptr=ptr->next;
	}
	ptr=volinfoHead;
	while (ptr!=NULL)
	{
		freePtr=ptr->next;
		free(ptr);
		ptr=freePtr;
	}
	if (!found)
	{
		SendSerialEndOfData(SerialIO);
		return ;
	}

	struct Amiga_Stat* stat;
	stat = Amiga_Get_Stat(path);
	if (stat)
	{	
		sprintf(app,"%ld %ld %d %ld %ld %ld",stat->st_size,stat->st_blksize,stat->directory,stat->days,stat->minutes,stat->seconds);
		SendSerialMessage(SerialIO,app,app);
		free(stat);
	}
	SendSerialEndOfData(SerialIO);
	return ;
}

// Send the inode info of a file or directory via serial cable
void Serial_Amiga_Send_Statfs(struct IOExtSer* SerialIO,const char* const path)
{
	char app[1000];
	int cont=0;
	int found=0;
	struct VolumeInfo* ptr;
	struct VolumeInfo* freePtr;
	struct VolumeInfo* volinfoHead=NULL;

	// Check if the volume exists
	while (path[cont]!=':' && path[cont]!=0)
		cont++;
	snprintf(app,cont+1,"%s",path);
	volinfoHead=getVolumes(DLT_VOLUME);
	ptr=volinfoHead;
	while (!found&&ptr)
	{
		if (!strcmp(app,ptr->name))
			found=1;
		ptr=ptr->next;
	}
	ptr=volinfoHead;
	while (ptr!=NULL)
	{
		freePtr=ptr->next;
		free(ptr);
		ptr=freePtr;
	}
	if (!found)
	{
		SendSerialEndOfData(SerialIO);
		return ;
	}

	struct Amiga_Statfs* stat;
	stat = getStatFs((char*)path);
	if (stat)
	{	
		sprintf(app,"%ld %ld %ld",stat->st_blksize,stat->st_numblocks,stat->st_numblocksused);
		SendSerialMessage(SerialIO,app,app);
		free(stat);
	}
	SendSerialEndOfData(SerialIO);
	return ;
}

void Serial_Check_Floppy_Drive(struct IOExtSer* SerialIO)
{
	char TrackDeviceReadBuffer[READ_BUFFER_SIZE];
	SerialRead(SerialIO,"1","Getting trackdevice",TrackDeviceReadBuffer);
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

void Serial_Relabel_Volume(struct IOExtSer* SerialIO)
{
	struct VolumeInfo* ptr;
	struct VolumeInfo* freePtr;
	struct VolumeInfo* volinfoHead=NULL;
	char VolumeReadBuffer[READ_BUFFER_SIZE];
	char NewNameReadBuffer[READ_BUFFER_SIZE];
	int found=0;
	char completeName[257];

	SerialRead(SerialIO,"1","Getting volume name",VolumeReadBuffer);
	if (VolumeReadBuffer[strlen(VolumeReadBuffer)-1]!=':') strcat(VolumeReadBuffer,":");
	SerialRead(SerialIO,"2","Getting new name",NewNameReadBuffer);

	if (VERBOSE) printf("Relabeling volume '%s' in '%s'\n",VolumeReadBuffer,NewNameReadBuffer);

	volinfoHead=getVolumes(DLT_VOLUME);
	ptr=volinfoHead;
	while (!found&&ptr)
	{
		sprintf(completeName,"%s:",ptr->name);
		if (!strcmp(VolumeReadBuffer,completeName))
			found=1;
		ptr=ptr->next;
	}
	ptr=volinfoHead;
	while (ptr!=NULL)
	{
		freePtr=ptr->next;
		free(ptr);
		ptr=freePtr;
	}
	if (!found)
	{
		SendSerialMessageAndEOD(SerialIO,"5","KO");
		return ;
	}

	if (Relabel(VolumeReadBuffer,NewNameReadBuffer))
		SendSerialMessageAndEOD(SerialIO,"3","OK");
	else
		SendSerialMessageAndEOD(SerialIO,"4","KO");
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
	char driveName[10];

	SerialRead(SerialIO,"1","Getting trackdevice",TrackDeviceReadBuffer);
	if (atoi(TrackDeviceReadBuffer)<MINTRACKDEVICES||atoi(TrackDeviceReadBuffer)>MAXTRACKDEVICES)
	{
		SendSerialMessageAndEOD(SerialIO,"6","Trakdevice number not entered correctly");
		return ;
	}

	SerialRead(SerialIO,"2","Getting start",StartTrackReadBuffer);
	if (atoi(StartTrackReadBuffer)<0||atoi(StartTrackReadBuffer)>79)
	{
		SendSerialMessageAndEOD(SerialIO,"6","Start track number not entered correctly");
		return ;
	}

	SerialRead(SerialIO,"3","Getting end",EndTrackReadBuffer);
	if (atoi(EndTrackReadBuffer)<0||atoi(EndTrackReadBuffer)>79||atoi(EndTrackReadBuffer)<atol(StartTrackReadBuffer))
	{
		SendSerialMessageAndEOD(SerialIO,"6","End track number not entered correctly");
		return ;
	}

	// Mark disk as busy
	sprintf(driveName,"DF%d:",atoi(TrackDeviceReadBuffer));
	Inhibit(driveName, TRUE);

	// From now i am listening for incoming data
	sprintf(command,"4 0 %d",CHUNK_DATA_READ);
	SendSerialMessageAndEOD(SerialIO,command,command);

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
			SendSerialMessageAndEOD(SerialIO,command,command);
		}
	}

	// Release disk
	Inhibit(driveName, FALSE);

	for (cont=0;cont < 11 ; cont ++) FreeMem(buffer[cont], 512);
	
	// Restore termination mode
	EnableTerminationMode(SerialIO);
	
	//Clear internal buffers
	SendClear(SerialIO);

	// Send confirmation
	SendSerialMessageAndEOD(SerialIO,"5","Send OK");
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
		SendSerialMessage(SerialIO,ptr->statInfo,ptr->statInfo);
		SendSerialMessage(SerialIO," ","-");
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
