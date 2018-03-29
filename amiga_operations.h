// Data structures

struct VolumeInfo
{
	UBYTE name[256];
	struct VolumeInfo* next;
};

struct ContentInfo
{
	char fileName[108];
	char statInfo[256];
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

// Data structure to hold information about a filesystem
struct Amiga_Statfs
{
	long st_blksize;
	long st_numblocks;
	long st_numblocksused;
};


// Adf file management
void Amiga_Write_Adf_Track(int ,UBYTE ** ,int );
int Amiga_Read_Adf_Data(int devicenum,int length,int offset,UBYTE **);

// Checks
int Amiga_Check_FloppyDisk_Presence(int );

// Fs operations
struct ContentInfo* getContentList(char*);
struct VolumeInfo* getVolumes(const int);
struct Amiga_Stat* Amiga_Get_Stat(char*);
struct Amiga_Statfs* getStatFs(char* path);

//Helpers
void BSTR2C(BSTR,UBYTE*);
void getVolumeName(const char*,const int, char*);


