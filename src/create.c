#include "create.h"
#include "filesystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


int mkdir_cmd(FILE* image, uint32_t currentCluster, const char *dirname, uint16_t bytesPerSector, uint8_t sectorsPerCluster, uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize){
    //check dirname isnt empty somehow
    if(dirname == NULL || strlen(dirname) == 0){
        fprintf(stderr, "Directory name cannot be blank\n");
        return -1;
    }
    
    //check for exisiting dir
    if(findFile(image, currentCluster, dirname, bytesPerSector, sectorsPerCluster, reservedSectorCount, numFat, fatSize, NULL, NULL)){//check if dir already exists
        fprintf(stderr, "Directory already exists\n");
        return -1;
    }
    //allocate free cluster (should i error check for there being no free cluster....)
    uint32_t newCluster = findCluster(image, reservedSectorCount, bytesPerSector, fatSize);
    //error checking for no free cluster (bc fine)
    if(newCluster == 0){
        fprintf(stderr, "No free cluster\n");
        return -1;
    }

    //find new area for dir
    //if there is no space for dir
    uint32_t offset;
    if(!findDirSlot(image, currentCluster, bytesPerSector, sectorsPerCluster, reservedSectorCount, numFat, fatSize, &offset)){
        fprintf(stderr, "No space for directory\n");
        return -1;
    }
    
    //have to update FAT w/ EOC label so need to mark the cluster as en of chain
    uint32_t eoc = 0x0FFFFFFF;  //mark cluster as end of chain
    //write to each cpy
    for(int i = 0; i < numFat; i++){
        uint32_t fOffset = (reservedSectorCount + (i * fatSize)) * bytesPerSector;
        fseek(image, fOffset + (newCluster * 4), SEEK_SET); //move to new cluster
        fwrite(&eoc, sizeof(uint32_t), 1, image);   //write eoc
    }

    uint32_t dataSector = reservedSectorCount + (numFat * fatSize); //find first data sec
    uint32_t firstClustSec = ((newCluster - 2) * sectorsPerCluster) + dataSector;
    uint32_t clusterOffset = firstClustSec * bytesPerSector;
    uint32_t clusterSize = bytesPerSector * sectorsPerCluster;

    DirEntry newDir = {0};    //for holdin new dir
    for(int i = 0; i < 11; i++){
        newDir.name[i] = ' ';      //set to 11 so no trash can fill
    }
    for(int i = 0; i < 11 && dirname[i] != '\0'; i++){
        newDir.name[i] = toupper((unsigned char)dirname[i]);
    }
    newDir.attr = ATTR_DIRECTORY;   //set new dir to be a directory
    //this is to store starting cluyster
    newDir.highCluster = (newCluster >> 16) &0xFFFF;
    newDir.lowCluster = newCluster &0xFFFF;
    fseek(image, offset, SEEK_SET); //move to empty slot
    fwrite(&newDir, sizeof(DirEntry), 1, image);    //writes dir entry at empty slot

    //move to start of cluster and zero it out
    fseek(image, clusterOffset, SEEK_SET);
    char zero = 0x00;
    for(uint32_t i = 0; i < clusterSize; i++){
        fwrite(&zero, 1, 1, image);     //write 0x00 to cluster
    }

    //assign cparts of dir cluster to parent (..) and cwd(.)
    DirEntry nextDir = {0};
    for(int i = 0; i < 11; i++){
        nextDir.name[i] = ' ';      //set to 11 so no trash can fill
    }
    //name .
    nextDir.name[0] = '.';
    nextDir.attr = ATTR_DIRECTORY;
    //points . to new entry so itself (aka next dir from cwd)
    nextDir.highCluster = (newCluster >> 16) &0xFFFF;
    nextDir.lowCluster = newCluster &0xFFFF;

    DirEntry parentDir = {0};
    memset(&parentDir, 0, sizeof(DirEntry));
    for(int i = 0; i < 11; i++){
        parentDir.name[i] = ' ';      //set to 11 so no trash can fill
    }
    //give it its name ..
    parentDir.name[0] = '.';
    parentDir.name[1] = '.';
    parentDir.attr = ATTR_DIRECTORY;
    //points back to parent 
    parentDir.highCluster = (currentCluster >> 16) &0xFFFF;
    parentDir.lowCluster = currentCluster &0xFFFF; 

    fseek(image, clusterOffset, SEEK_SET);
    fwrite(&nextDir, sizeof(DirEntry), 1, image);
    fwrite(&parentDir, sizeof(DirEntry), 1, image);

    fflush(image);
    return 0;
}

int creat_cmd(FILE* image, uint32_t currentCluster, char *filename, uint16_t bytesPerSector, uint8_t sectorsPerCluster, uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize){
    if(filename == NULL || strlen(filename) == 0){
        fprintf(stderr, "Filename name cannot be blank\n");
        return -1;
    }
    if(findFile(image, currentCluster, filename, bytesPerSector, sectorsPerCluster, reservedSectorCount, numFat, fatSize, NULL, NULL)){//check if dir already exists
        fprintf(stderr, "File already exists\n");
        return -1;
    }
    uint32_t offset;
    if(!findDirSlot(image, currentCluster, bytesPerSector, sectorsPerCluster, reservedSectorCount, numFat, fatSize, &offset)){
        fprintf(stderr, "No space for file\n");
        return -1;
    }

    DirEntry newFile = {0}; //makre sure it dont fill
    //fill name
    for(int i = 0; i < 11; i++){
        newFile.name[i] = ' ';      //set to 11 so no trash can fill
    }
    for(int i = 0; i < 11 && filename[i] != '\0'; i++){
        newFile.name[i] = toupper((unsigned char)filename[i]);
    }
    newFile.attr = ATTR_ARCHIVE;
    newFile.highCluster = 0;
    newFile.lowCluster = 0;

    fseek(image, offset, SEEK_SET); //move to empty slot
    fwrite(&newFile, sizeof(DirEntry), 1, image);    //writes dir entry at empty slot

    fflush(image);
    return 0;
}

uint32_t findCluster(FILE *image, uint16_t reservedSectorCount, uint16_t bytesPerSector, uint32_t fatSize){
    uint32_t fOffset = reservedSectorCount * bytesPerSector;  //find offset
    uint32_t entries = (fatSize * bytesPerSector) / 4; //each entry is 4 bytes so div by 4

    uint32_t fatVal;    //value from fat entry

    //searcgh thru clusters
    for(uint32_t i = 2; i < entries; i++){

        fseek(image, fOffset + (i * 4), SEEK_SET); //offset is i*4 bc 4 bytes per entry
        fread(&fatVal, sizeof(uint32_t), 1, image); //read fat entry into fatVal

        fatVal &= 0x0FFFFFFF;   //mask off top 4 bits

        //if entry 0, cluster free
        if(fatVal == 0){
            return i;
        }
    }

    return 0; //no free clusetr
}

//find if file or dir already exist
int findFile(FILE *image, uint32_t currentCluster, const char *name, uint16_t bytesPerSector, uint8_t sectorsPerCluster, uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize, DirEntry *findEntry, uint32_t *entryOffset){
    uint32_t dataSector = reservedSectorCount + (numFat * fatSize); //find first data sec
    uint32_t firstClustSec = ((currentCluster - 2) * sectorsPerCluster) + dataSector;
    //sec num to offset
    uint32_t offset = firstClustSec * bytesPerSector;

    //num of entries in 1 cluster
    int entries = (bytesPerSector * sectorsPerCluster) / sizeof(DirEntry);
    DirEntry entry;
    
    //format the entry :D
    char targ[12] = {0};   //11 + delimiter
    for(int i = 0; i < 11 && name[i] != '\0'; i++){
        targ[i] = toupper((unsigned char)name[i]);
    }
    
    fseek(image, offset, SEEK_SET);

    for(int i = 0; i < entries; i++){
        //offet of entry
        uint32_t currentOffset = offset + (i * sizeof(DirEntry));
        //move to it
        fseek(image, currentOffset, SEEK_SET);

        fread(&entry, sizeof(DirEntry), 1, image);

        //no more entries
        if(entry.name[0] == 0x00){
            break;
        }
        //if its a deleted slot ignore and skip
        if(entry.name[0] == 0xE5){
            continue;
        }
        //skip LONG NAME bro
        if(entry.attr == 0x0F){
            continue;
        }
        
        char entryName[12];
        memcpy(entryName, entry.name, 11);
        entryName[11] = '\0'; //delimiter

        //move backwards to get rid of any trailing spaces
        for(int j = 10; j >= 0; j--){
            if(entryName[j] == ' '){
                entryName[j] = '\0';
            }
            else{
                break;
            }
        }

        //if the entry name == target name the file already is in existence
        if(strcmp(entryName, targ) == 0){
            //added these so findFile can also work for delete
            if(findEntry != NULL){
                *findEntry = entry;
            }
            if(entryOffset != NULL){
                *entryOffset = currentOffset;
            }
            return 1;
        }
    }

    return 0;
}

int findDirSlot(FILE *image, uint32_t currentCluster, uint16_t bytesPerSector, uint8_t sectorsPerCluster, uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize, uint32_t *sOffset){
    uint32_t dataSector = reservedSectorCount + (numFat * fatSize); //find first data sec
    uint32_t firstClustSec = ((currentCluster - 2) * sectorsPerCluster) + dataSector;
    uint32_t clusterOffset = firstClustSec * bytesPerSector;

    int entries = (bytesPerSector * sectorsPerCluster) / sizeof(DirEntry);
    DirEntry entry;

    for(int i = 0; i < entries; i++){
        uint32_t offset = clusterOffset + (i * sizeof(DirEntry));
        fseek(image, offset, SEEK_SET);

        //read, if read bad
        if(fread(&entry, sizeof(DirEntry), 1, image) != 1){
            break;
        }
        //if there is a empty entry
        //okay also added if there is a deleted slot bc that can be used too
        if(entry.name[0] == 0x00 || entry.name[0] == 0xE5){
            *sOffset = offset;
            return 1;
        }
    }

    return 0;
}
