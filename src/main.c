#include "lexer.h"
#include "filesystem.h"
#include "create.h"
#include "delete.h"
#include <stdio.h>
#include <errno.h>
#include <stdint.h> //fixed width integers
#include <string.h>
#include <ctype.h>

int main(int argc, char* argv[])
{
    //checking if given correct number of arguments
    if (argc != 2)
    {
        printf("incorrect number of command line arguments\n");
        return 1;
    }

    //get the filename as a null terminated string
    const char* disk_image = argv[1];

    //open the filename
    FILE* image = fopen(disk_image, "rb+"); //using rb to read in binary mode

    if (image == NULL)
    {
        perror("Error: Incorrect file\n");
        return 1;
    }

    // -- INFO of the image File --

    //for bytes for the root cluster
    uint32_t rootCluster;
    //moves the file position to the specific offset, 44 offset
    fseek(image, 44, SEEK_SET);
    //reads in the root cluster at each position
    fread(&rootCluster, sizeof(uint32_t), 1, image);

    uint16_t bytesPerSector;
    //11 byte offset for secotrs
    fseek(image, 11, SEEK_SET);
    fread(&bytesPerSector, sizeof(uint16_t), 1, image);

    uint8_t sectorsPerCluster;
    //11 byte offset for secotrs
    fseek(image, 13, SEEK_SET);
    fread(&sectorsPerCluster, sizeof(uint8_t), 1, image);

    //getting the total number of clusters in the data region
    //have to get other values first to calculate 
    uint32_t totalSectors;
    fseek(image, 32, SEEK_SET);
    fread(&totalSectors, sizeof(uint32_t), 1, image);

    uint16_t reservedSectorCount;
    fseek(image, 14, SEEK_SET);
    fread(&reservedSectorCount, sizeof(uint16_t), 1, image);

    uint8_t numFat;
    fseek(image, 16, SEEK_SET);
    fread(&numFat, sizeof(uint8_t), 1, image);

    uint32_t fatSize;
    fseek(image, 36, SEEK_SET);
    fread(&fatSize, sizeof(uint32_t), 1, image);

    uint32_t dataSectors = totalSectors - reservedSectorCount - (numFat * fatSize);
    uint32_t totalClusters = dataSectors / sectorsPerCluster;
    uint32_t fatEntries = (fatSize * bytesPerSector) / 4;
    uint32_t imageSize = totalSectors * bytesPerSector;


    uint32_t currentCluster = rootCluster;



    // -- END of INFO -- 
    char currentPath[256] = "/";

    //processing input
    char input[100];

      while (1)
      {
        //simulate command line by printing path 
        printf("%s%s> ", disk_image, currentPath);

        fgets(input, sizeof(input), stdin);
    
        //get input
        input[strcspn(input, "\n")] = 0;
    
        //if its info print info
        if (strcmp(input, "info") == 0)
        {
            printf("Root cluster: %u\n", rootCluster);
            printf("Bytes per sector: %u\n", bytesPerSector);
            printf("Sectors per cluster: %u\n", sectorsPerCluster);
            printf("Total number of clusters in data region: %u\n", totalClusters);
            printf("Number of entries in one FAT: %u\n", fatEntries);
            printf("Size of image (bytes): %llu\n", imageSize); 
        }
        //if its exit close the file
        else if (strcmp(input, "exit") == 0)
        {
            fclose(image);
            return 0;
        }
        else if (strcmp(input, "ls") == 0)
        {
            ls(image, currentCluster, bytesPerSector, sectorsPerCluster, reservedSectorCount, numFat, fatSize);
        }
        else if (strncmp(input, "cd ", 3) == 0)
        {
            char* dirName = input + 3;
            //stores new cluster variable
            uint32_t newCluster = cd(image, currentCluster, dirName,
                             bytesPerSector, sectorsPerCluster,
                             reservedSectorCount, numFat, fatSize);
            //only changes current cluster if it differs from old
            if (newCluster != currentCluster)
            {
                currentCluster = newCluster;
                //checks if the user typed .. after d to fix the path name
                if (strcmp(dirName, "..") == 0)
                {
                    char* lastSlash = strrchr(currentPath, '/');
                    //find the last slash and see if it is correct
                    if (lastSlash != NULL && lastSlash != currentPath)
                    {   //if not deliminate the last slash
                        *lastSlash = '\0';
                    }
                    else
                    {
                        strcpy(currentPath, "/");
                    }
                }
                else
                {
                    if (strcmp(currentPath, "/") != 0)
                    {
                       strcat(currentPath, "/");
                    }

                    strcat(currentPath, dirName);
                }
            }
        } 
        else if(strncmp(input, "mkdir ", 6) == 0){
            char* dirname = input + 6;

            if(strlen(dirname) == 0){   //if no dirname
                fprintf(stderr, "Usage: mkdir [DIRNAME]\n");
                continue;
            }

            mkdir_cmd(image, currentCluster, dirname, bytesPerSector, sectorsPerCluster, reservedSectorCount, numFat, fatSize);
        }
        else if(strncmp(input, "creat ", 6) == 0){
            char* filename = input + 6;

            if(strlen(filename) == 0){
                fprintf(stderr, "Usage: creat [FILENAME]\n");
                continue;
            }

            creat_cmd(image, currentCluster, filename, bytesPerSector, sectorsPerCluster, reservedSectorCount, numFat, fatSize);
        }
        else if(strncmp(input, "rm ", 3) == 0){
            char* filename = input + 3;

            if(strlen(filename) == 0){
                fprintf(stderr, "Usage: rm [FILENAME]\n");
                continue;
            }

            rm_cmd(image, currentCluster, filename, bytesPerSector, sectorsPerCluster, reservedSectorCount, numFat, fatSize);
        }
        else if(strncmp(input, "rmdir ", 6) == 0){
            char* dirname = input + 6;

            if(strlen(dirname) == 0){
                fprintf(stderr, "Usage: rmdir [DIRNAME]\n");
                continue;
            }

            rmdir_cmd(image, currentCluster, dirname, bytesPerSector, sectorsPerCluster, reservedSectorCount, numFat, fatSize);
        }
        else if (strncmp(input, "open ", 5) == 0)
        {
			    char* rest = input + 5; //points to the rest of the string after "open "

			    char* filename = strtok(rest, " "); //tokenizes the string to get the filename 
			    char* flags = strtok(NULL, " ");//tokenizes the string to get the flags

			    if (filename == NULL || flags == NULL) //checks if the user inputted the correct number of arguments
          {
				    fprintf(stderr, "incorrect format: open [FILENAME] [FLAGS]\n");//error with the correct usage of the open command
            continue;
          }

            openFile(image, currentCluster, filename, flags,currentPath,bytesPerSector, sectorsPerCluster, reservedSectorCount, numFat, fatSize);
        }
        else if (strncmp(input, "close ", 6) == 0) //6 bc close is 6 include the space char
        {
            char* filename = input + 6;//the file name is the input starting at the 6th place

            if (strlen(filename) == 0)//no file name
            {
                fprintf(stderr, "incorrect format: close [FILENAME]\n");
                continue;
            }

            closeFile(filename);//call func
        }
        else if (strcmp(input, "lsof") == 0)
        {
			printf("%-6s %-15s %-6s %-10s %-10s \n", "INDEX", "NAME", "MODE", "OFFSET", "PATH");
            lsof();
        }
        else if (strncmp(input, "lseek ", 6) == 0) //take in offset
        {
          char* rest = input + 6;
          //splits the filename
          char* filename = strtok(rest, " ");
          //gets the rest of it (current offset)
          char* offsetStr = strtok(NULL, " ");

          if (filename == NULL || offsetStr == NULL)
          {
            fprintf(stderr, "incorrect format: filename offset\n");
            continue;
          }
          //conver string to int
          int offset = atoi(offsetStr);

          lseek(filename, offset);
        }
        else if (strncmp(input, "read ", 5) == 0)
        {
          char* rest = input + 5;

          // need to acquire parameters for read
          char* filename = strtok(rest, " ");
          char* sizeStr = strtok(NULL, " ");

          if (filename == NULL || sizeStr == NULL)
          {
            fprintf(stderr, "incorrect format: read [FILENAME] [SIZE]\n");
            continue;
          }

          int size = atoi(sizeStr);

          read(image, filename, size, bytesPerSector, sectorsPerCluster,
               reservedSectorCount, numFat, fatSize);
        }
        else if (strncmp(input,"mv ",3)==0)
        {
            char* rest = input + 3;
            char* src = strtok(rest, " ");
            char* dest = strtok(NULL, " ");
            if (src == NULL || dest == NULL)
            {
                fprintf(stderr, "Usage: mv [SRC] [DEST]\n");
                continue;
            }
            mv(image, currentCluster, src,dest, bytesPerSector, sectorsPerCluster, reservedSectorCount, numFat, fatSize);
        }
        else if (strncmp(input, "write ", 6) == 0)
        {
          char* rest = input + 6;

          char* space = strchr(rest, ' ');
          if (space == NULL)
          {
              fprintf(stderr, "incorrect format: write [FILENAME] \"STRING\"\n");
              continue;
          }

          // isolate filename
          *space = '\0';
          char* filename = rest;

          // get the start and end of the string
          char* start = strchr(space + 1, '"');
          char* end = strrchr(space + 1, '"');

          if (start == NULL || end == NULL || start == end)
          {
              fprintf(stderr, "Error: invalid string\n");
              continue;
          }

          *end = '\0';
          char* data = start + 1;

          int size = strlen(data);

          write(image, filename, size, data, bytesPerSector, sectorsPerCluster,
                    reservedSectorCount, numFat, fatSize);
        } 
        else
        {
          printf("Unknown command\n");
        }
      }
      printf("success");
  
      fclose(image);  
      return 0;
    }


