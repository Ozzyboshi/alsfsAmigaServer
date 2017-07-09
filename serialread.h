#define SERIAL_BUFFER_SIZE 256


void SerialRead(struct IOExtSer*,const char*,const char*,char*);
void SendSerialMessage(struct IOExtSer*,const char*,const char*);
void SendSerialEndOfData(struct IOExtSer*);
void SendSerialNewLine(struct IOExtSer*);

static int VERBOSE=0;

