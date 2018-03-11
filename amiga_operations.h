// Data structures

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

// Adf file management
void Amiga_Write_Adf_Track(int ,UBYTE ** ,int );
int Amiga_Read_Adf_Data(int devicenum,int length,int offset,UBYTE **);

// Checks
int Amiga_Check_FloppyDisk_Presence(int );

// Fs operations
struct ContentInfo* getContentList(char*);
struct VolumeInfo* getVolumes(const int);

//Helpers
void BSTR2C(BSTR,UBYTE*);
void getVolumeName(const char*,const int, char*);


