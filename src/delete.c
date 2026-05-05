#include "delete.h"
#include "create.h"
#include "filesystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

//do not need to delete actual file content
//mark first byte each dir entry in dir cluster to
    //if there are files after: DIR_NAME[0] = 0x5E
    //if there are no files after DIR_NAME[0] = 0x00

int rm_cmd(FILE *image, uint32_t currentCluster, char *filename, uint16_t bytesPerSector, uint8_t sectorsPerCluster, uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize){
    //filename blank
    if(filename == NULL || strlen(filename) == 0){
        fprintf(stderr, "Filename name cannot be blank\n");
        return -1;
    }

    //check if file is open
    if(isFileOpen(filename)){
        fprintf(stderr, "File is open\n");
        return -1;
    }

    //to be filled by find file
    DirEntry entry;
    uint32_t offset;

    //check if file exists
    if(!findFile(image, currentCluster, filename, bytesPerSector, sectorsPerCluster, reservedSectorCount, numFat, fatSize, &entry, &offset)){
        fprintf(stderr, "File doesnt exist\n");
        return -1;
    }

    //check to make sure it is not a dir
    if(entry.attr & ATTR_DIRECTORY){
        fprintf(stderr, "To remove a directory use rmdir\n");
        return -1;
    }

    //file start of entry cluster
    uint32_t start = (entry.highCluster << 16) | entry.lowCluster;
    //call free cluster to yk free the cluster
    freeCluster(image, start, reservedSectorCount, bytesPerSector, numFat, fatSize);

    //mark as deleted
    uint8_t deletedEntry = 0xE5;
    fseek(image, offset, SEEK_SET);
    fwrite(&deletedEntry, 1, 1, image);

    //flush changes
    fflush(image);

    return 0;
}

int rmdir_cmd(FILE *image, uint32_t currentCluster, const char *dirname, uint16_t bytesPerSector, uint8_t sectorsPerCluster, uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize){
    //dirname blank
    if(dirname == NULL || strlen(dirname) == 0){
        fprintf(stderr, "Directory name cannot be blank\n");
        return -1;
    }
    
    //to be filled by find file
    DirEntry entry;
    uint32_t offset;

    //check if dir exists
    if(!findFile(image, currentCluster, dirname, bytesPerSector, sectorsPerCluster, reservedSectorCount, numFat, fatSize, &entry, &offset)){
        fprintf(stderr, "Directory doesnt exist\n");
        return -1;
    }

    //make sure it IS a dir
    if(!(entry.attr & ATTR_DIRECTORY)){
        fprintf(stderr, "Not a directory\n");
        return -1;
    }

    /*if the entryName is open error
    if(isFileOpen(entryName)){
        fprintf(stderr, "There is an open file in this directory\n");
        return -1;
    }*/

    //file start of entry cluster
    uint32_t start = (entry.highCluster << 16) | entry.lowCluster;

    //make sure dir is emoty besides . and ..
    if(!checkDir(image, start, bytesPerSector, sectorsPerCluster, reservedSectorCount, numFat, fatSize)){
        fprintf(stderr, "Directory must be empty for deletion\n");
        return -1;
    }
    //remove dir from cwd and remove data dirname points to
    //error dirname dont exist, dirname not a dir, dirname is not empty or file in dir is open

    //same process of deleting a file just make sure dir is empty

    /*loop thru dir and make sure none of the files are open
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
    }*/
    //mark as deleted
    uint8_t deletedEntry = 0xE5;
    fseek(image, offset, SEEK_SET);
    fwrite(&deletedEntry, 1, 1, image);

    //call free cluster to yk free the cluster
    freeCluster(image, start, reservedSectorCount, bytesPerSector, numFat, fatSize);

    fflush(image);

    return 0;
}

void freeCluster(FILE *image, uint32_t start, uint16_t reservedSectorCount, uint16_t bytesPerSector, uint8_t numFat, uint32_t fatSize){
    //move thru chain
    for(uint32_t i = start; i < 0x0FFFFFF8; ){   //0x0FFFFFF8 is end of chain
        //set temp var to store next cluster
        uint32_t temp = getNextCluster(image, i, reservedSectorCount, fatSize, bytesPerSector);

        //loop thru each fat cp to clear
        for(int j = 0; j < numFat; j++){
            //offset of start
            uint32_t offset = (reservedSectorCount + (j * fatSize)) * bytesPerSector;
            
            //entry positions
            fseek(image, offset + (i * 4), SEEK_SET);

            //set entry to 0 to clear
            uint32_t clear = 0x00000000;
            fwrite(&clear, sizeof(uint32_t), 1, image);

        }
        //check if we are at end for safety
        if(temp >= 0x0FFFFFF8){
            break;
        }
        i = temp;   //move to next
    }

}

int checkDir(FILE *image, uint32_t currentCluster, uint16_t bytesPerSector, uint8_t sectorsPerCluster, uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize){
    //basically just editing find dir slot to check if theyre all empty instead of finding 1 empty slot
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
        //if the name is . or .. skip (might change this later but like i think this works for both)
        if(entry.name[0] == '.'){
            if(entry.name[1] == ' ' || entry.name[1] == '.'){
                continue;
            }
        }

        if(entry.name[0] == 0xE5){
            continue;   //continue bc it was deleted so its gone
        }

        if(entry.name[0] == 0x00){
            return 1;   //empty good
        }

        return 0;   //not empty, smth was found
    }

    return 1;
}
