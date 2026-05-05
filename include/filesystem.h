#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdio.h>
#include <stdint.h>

#define MAX_OPEN_FILES 10

#pragma pack(push,1)
typedef struct
{
    uint8_t name[11];        // 0–10
    uint8_t attr;            // 11
    uint8_t ntRes;           // 12
    uint8_t crtTimeTenth;    // 13
    uint16_t crtTime;        // 14–15
    uint16_t crtDate;        // 16–17
    uint16_t lstAccDate;     // 18–19
    uint16_t highCluster;    // 20–21
    uint16_t wrtTime;        // 22–23
    uint16_t wrtDate;        // 24–25
    uint16_t lowCluster;     // 26–27
    uint32_t fileSize;       // 28–31
} DirEntry;
#pragma pack(pop)

typedef struct
{
    uint32_t cwdCluster;
    char imageName[256];
}shellData;



typedef struct {
    char name[12];          // 8.3 filename
    uint32_t firstCluster;  // starting cluster of file
    uint32_t size;          
	char path[256];          // full path to the file 
    uint32_t offset;        // current offset (starts at 0)
    int mode;               // 0 = read, 1 = write, 2 = read/write
    int inUse;              // 1 = occupied, 0 = free
} OpenFile;

int parseMode(const char* flags);
int findFreeFD();
int isFileOpen(const char* name);
void ls(FILE* image,uint32_t currentCluster,uint16_t bytesPerSector,uint8_t sectorsPerCluster,uint16_t reservedSectorCount,uint8_t numFat,uint32_t fatSize);
uint32_t cd(FILE* image,uint32_t currentCluster, const char* dirName,uint16_t bytesPerSector, uint8_t sectorsPerCluster,uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize);
uint32_t getNextCluster(FILE* image, uint32_t cluster, uint16_t reservedSectorCount, uint32_t fatSize, uint16_t bytesPerSector);
int openFile(FILE* image, uint32_t currentCluster, const char* filename, const char* flags, const char* curpath, uint16_t bytesPerSector, uint8_t sectorsPerCluster, uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize);
int closeFile(const char* filename);
void lsof();
void lseek(const char* filename, int newOffset);
void read(FILE* image, const char* filename, int size, uint16_t bytesPerSector, uint8_t sectorsPerCluster,uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize);
void mv(FILE* image, uint32_t currentCluster, const char* src, const char* dest, uint16_t bytesPerSector, uint8_t sectorsPerCluster, uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize);
void write(FILE* image, const char* filename, int size, const char* data,uint16_t bytesPerSector, uint8_t sectorsPerCluster,
               uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize);

#endif
