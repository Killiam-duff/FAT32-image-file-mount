#pragma once
#include <stdio.h>
#include <stdint.h>
#include "filesystem.h"

//dir attributes DIR_Attr
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_NAME (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

int mkdir_cmd(FILE* image, uint32_t currentCluster, const char *dirname, uint16_t bytesPerSector, uint8_t sectorsPerCluster, uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize); //make a dir 
int creat_cmd(FILE* image, uint32_t currentCluster, char *filename, uint16_t bytesPerSector, uint8_t sectorsPerCluster, uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize); //make a file
//find a free cluster
uint32_t findCluster(FILE* image, uint16_t reservedSectorCount, uint16_t bytesPerSector, uint32_t fatSize); 
//find out if there is a file already in the directory
int findFile(FILE* image, uint32_t currentCluster, const char *name, uint16_t bytesPerSector, uint8_t sectorsPerCluster, uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize, DirEntry *findEntry, uint32_t *entryOffset);
//find an empty dir slot to put file
int findDirSlot(FILE* image, uint32_t currentCluster, uint16_t bytesPerSector, uint8_t sectorsPerCluster, uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize, uint32_t *sOffset);

