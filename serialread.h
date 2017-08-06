#define SERIAL_BUFFER_SIZE 256

static struct IOTArray TERMINATORS_CHARACTERS =
{
		0x04040403,   /*  etx eot */
		0x03030303    /* fill to end with lowest value */
};

void SerialRead(struct IOExtSer*,const char*,const char*,char*);
void SendSerialMessage(struct IOExtSer*,const char*,const char*);
void SendSerialEndOfData(struct IOExtSer*);
void SendSerialNewLine(struct IOExtSer*);
void DisableTerminationMode(struct IOExtSer*);
void EnableTerminationMode(struct IOExtSer*);
void SendClear(struct IOExtSer*);


static int VERBOSE=0;

