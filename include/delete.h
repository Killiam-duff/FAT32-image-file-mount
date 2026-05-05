#pragma once
#include <stdio.h>
#include <stdint.h>

int rm_cmd(FILE* image, uint32_t currentCluster, char *filename, uint16_t bytesPerSector, uint8_t sectorsPerCluster, uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize);
int rmdir_cmd(FILE* image, uint32_t currentCluster, const char *dirname, uint16_t bytesPerSector, uint8_t sectorsPerCluster, uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize);
//helper to clear out cluster
void freeCluster(FILE* image, uint32_t start, uint16_t bytesPerSector, uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize);
//helper to check if the dir is empty
int checkDir(FILE* image, uint32_t currentCluster, uint16_t bytesPerSector, uint8_t sectorsPerCluster, uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize);
