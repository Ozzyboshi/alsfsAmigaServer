
#include <exec/io.h>
#include <devices/serial.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <stdio.h>
#include <string.h>

#include "serialread.h"
#include "debug.h"

//int VERBOSE;
 
void SerialRead(struct IOExtSer* SerialIO,const char* serialMsg,const char* debugMsg,char* Buffer)
{
	int cont=0;
	memset(Buffer,0,SERIAL_BUFFER_SIZE);
	
	SendSerialMessage(SerialIO,serialMsg,debugMsg);
	SendSerialEndOfData(SerialIO);
	
	// Read filename from serial port
	SerialIO->IOSer.io_Length   = SERIAL_BUFFER_SIZE-1;
	SerialIO->IOSer.io_Command  = CMD_READ;
	SerialIO->IOSer.io_Data     = (APTR)&Buffer[0];
	DoIO((struct IORequest *)SerialIO);
	if (VERBOSE) printf("Received : ##%s##\n",Buffer);
		
	for (cont=0;cont<SERIAL_BUFFER_SIZE;cont++)
		if (Buffer[cont]==4)
			Buffer[cont]=0;
			
	return ;
}

// Sends a string via serial interfaces
void SendSerialMessage(struct IOExtSer* SerialIO,const char* msg,const char* debugMsg)
{
	char chunk[41];
	int i=0;
	
	if (debugMsg && VERBOSE) printf("%s\n",debugMsg);
	for (i=0;i<=(int)((strlen(msg)-1)/40);i++)
	{
		SerialIO->IOSer.io_Length   = -1;
		SerialIO->IOSer.io_Command  = CMD_WRITE;
		snprintf(chunk,40,"%s",&msg[i*40]);
		SerialIO->IOSer.io_Data     = (APTR)chunk;
		if (DoIO((struct IORequest *)SerialIO))     /* execute write */
			fprintf(stderr,"Write failed.  Error - %d\n",SerialIO->IOSer.io_Error);

	}
	return ;
}
// As SendSerialMessage but terminated with EOF
void SendSerialMessageAndEOD(struct IOExtSer* SerialIO,const char* msg,const char* debugMsg)
{
	SendSerialMessage(SerialIO,msg,debugMsg);
	SendSerialEndOfData(SerialIO);
}


void SendSerialEndOfData(struct IOExtSer* SerialIO)
{
	char buffer[2];
	buffer[0]=3;
	buffer[1]=0;
	SerialIO->IOSer.io_Length   = 1;
	SerialIO->IOSer.io_Command  = CMD_WRITE;
	SerialIO->IOSer.io_Data     = (APTR)&buffer[0];
	if (VERBOSE) printf("ETX (end of text) sent\n");
	if (DoIO((struct IORequest *)SerialIO)) fprintf(stderr,"Write failed.  Error - %d\n",SerialIO->IOSer.io_Error);
	return ;
}
void SendSerialNewLine(struct IOExtSer* SerialIO)
{
	SerialIO->IOSer.io_Length   = 1;
	SerialIO->IOSer.io_Command  = CMD_WRITE;
	SerialIO->IOSer.io_Data     = (APTR)"\n";
	if (DoIO((struct IORequest *)SerialIO)) fprintf(stderr,"Write failed.  Error - %d\n",SerialIO->IOSer.io_Error);
	return ;
}
void DisableTerminationMode(struct IOExtSer* SerialIO)
{
	if (VERBOSE) printf("Disabling termination mode\n");
	SerialIO->io_SerFlags &= ~ SERF_EOFMODE;
	SerialIO->IOSer.io_Command  = SDCMD_SETPARAMS;
							
	if (DoIO((struct IORequest *)SerialIO))
    	fprintf(stderr,"Set Params failed ");   /* Inform user of error */

    return ;
}
void EnableTerminationMode(struct IOExtSer* SerialIO)
{
	if (VERBOSE) printf("Enabling termination mode\n");
	SerialIO->io_SerFlags |= SERF_EOFMODE;
	SerialIO->io_TermArray = TERMINATORS_CHARACTERS;
	SerialIO->IOSer.io_Command  = SDCMD_SETPARAMS;

	if (DoIO((struct IORequest *)SerialIO))
		fprintf(stderr,"Set Params failed ");   /* Inform user of error */

	return ;
}
void SendClear(struct IOExtSer* SerialIO)
{
	SerialIO->IOSer.io_Flags=0;
	SerialIO->IOSer.io_Command  = CMD_CLEAR;
	if (DoIO((struct IORequest *)SerialIO))
		fprintf(stderr,"Set clear failed ");

	return ;
}
void SendFlush(struct IOExtSer* SerialIO)
{
	SerialIO->IOSer.io_Flags=0;
	SerialIO->IOSer.io_Command  = CMD_FLUSH;
	if (DoIO((struct IORequest *)SerialIO))
		fprintf(stderr,"Set flush failed ");

	return ;
}
void SendUpdate(struct IOExtSer* SerialIO)
{
	SerialIO->IOSer.io_Flags=0;
	SerialIO->IOSer.io_Command  = CMD_UPDATE;
	if (DoIO((struct IORequest *)SerialIO))
		fprintf(stderr,"Update failed ");
}
int QuerySerialDeviceCharsLeft(struct IOExtSer* SerialIO)
{
	SerialIO->IOSer.io_Flags=0;
	SerialIO->IOSer.io_Command  = SDCMD_QUERY;
	if (DoIO((struct IORequest *)SerialIO))
		fprintf(stderr,"Set flush failed ");
	//printf("Stats %u\n",SerialIO->io_Status);
	//printf("Bytes left : %lu\n",SerialIO->IOSer.io_Actual);
	return SerialIO->IOSer.io_Actual;
}