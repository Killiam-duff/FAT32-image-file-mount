#include "filesystem.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

OpenFile openFiles[MAX_OPEN_FILES];

int parseMode(const char* flags)
{
	if (strcmp(flags, "-r") == 0)
		return 0;
	if (strcmp(flags, "-w") == 0)
		return 1;
	if (strcmp(flags, "-rw") == 0 || strcmp(flags, "-wr") == 0)
		return 2;
	return -1; // invalid
}

int findFreeFD()
{
	for (int i = 0; i < MAX_OPEN_FILES; i++)
	{
		if (!openFiles[i].inUse)//not being used, return this index as the file descriptor
			return i;
	}
	return -1; // table full
}

int isFileOpen(const char* name)
{
	for (int i = 0; i < MAX_OPEN_FILES; i++)
	{
		if (openFiles[i].inUse && strcmp(openFiles[i].name, name) == 0)
			return 1;
	}
	return 0;
}


uint32_t getNextCluster(FILE* image, uint32_t cluster, uint16_t reservedSectorCount, uint32_t fatSize, uint16_t bytesPerSector)
{
	uint32_t fatOffset = reservedSectorCount * bytesPerSector;
	uint32_t entryOffset = fatOffset + (cluster * 4);

	uint32_t nextCluster;
	fseek(image, entryOffset, SEEK_SET);
	fread(&nextCluster, sizeof(uint32_t), 1, image);

	return nextCluster & 0x0FFFFFFF; // mask upper 4 bits
}

void ls(FILE* image, uint32_t currentCluster, uint16_t bytesPerSector, uint8_t sectorsPerCluster, uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize)
{
	if (currentCluster < 2) 
	{
		fprintf(stderr, "error: Invalid Cluster %u\n", currentCluster);
		return;
	}
	uint32_t firstDatasec = reservedSectorCount + (numFat * fatSize); //find the first data sector

	uint32_t cluster = currentCluster;
	while (cluster < 0x0FFFFFF8)
	{
		uint32_t firstSec = ((cluster - 2) * sectorsPerCluster) + firstDatasec; //first sector of the cluster
		uint32_t offset = firstSec * bytesPerSector;//convert to offset
		if (fseek(image, offset, SEEK_SET) != 0)
		{
			perror("error: fseek failed");
			return;
		}
		int entries = (bytesPerSector * sectorsPerCluster) / sizeof(DirEntry);
		DirEntry entry;


		for (int i = 0; i < entries; i++)
		{
			if (fread(&entry, sizeof(DirEntry), 1, image) != 1)
				break;

			if (entry.name[0] == 0x00) //end of the directory
				break;
			if (entry.name[0] == 0xE5) //deleted
				continue;
			if (entry.attr == 0x0F)//long file
				continue;

			char name[13]; // 8 + '.' + 3 + '\0'

			char base[9];
			char ext[4];

			// copy base name (first 8 chars)
			memcpy(base, entry.name, 8);
			base[8] = '\0';

			// copy extension (last 3 chars)
			memcpy(ext, entry.name + 8, 3);
			ext[3] = '\0';

			// trim spaces in base
			for (int i = 7; i >= 0; i--)
			{
				if (base[i] == ' ')
					base[i] = '\0';
				else
					break;
			}

			// trim spaces in ext
			for (int i = 2; i >= 0; i--)
			{
				if (ext[i] == ' ')
					ext[i] = '\0';
				else
					break;
			}

			// build final name
			if (strlen(ext) > 0)
				sprintf(name, "%s.%s", base, ext);
			else
				sprintf(name, "%s", base);
			printf("%s ", name); 
		}
		cluster = getNextCluster(image, cluster, reservedSectorCount, fatSize, bytesPerSector);
	}
	printf("\n");
}

uint32_t cd(FILE* image, uint32_t currentCluster, const char* dirName,uint16_t bytesPerSector, uint8_t sectorsPerCluster,uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize)
{
	if (strcmp(dirName, ".") == 0)
		return currentCluster;

	if (currentCluster < 2)
	{
		fprintf(stderr, "error: invalid current cluster %u\n", currentCluster);
		return currentCluster;
	}

	uint32_t firstDatasec = reservedSectorCount + (numFat * fatSize); // first data sector
	uint32_t cluster = currentCluster;

	while (cluster < 0x0FFFFFF8) // iterate through all clusters of the directory
	{
		uint32_t firstSec = ((cluster - 2) * sectorsPerCluster) + firstDatasec;
		uint32_t offset = firstSec * bytesPerSector;

		if (fseek(image, offset, SEEK_SET) != 0)
		{
			perror("error: fseek failed");
			return currentCluster;
		}

		int entries = (bytesPerSector * sectorsPerCluster) / sizeof(DirEntry);
		DirEntry entry;

		for (int i = 0; i < entries; i++)
		{
			if (fread(&entry, sizeof(DirEntry), 1, image) != 1)
				break;

			if (entry.name[0] == 0x00) break; // end of directory
			if (entry.name[0] == 0xE5) continue; // deleted
			if (entry.attr == 0x0F) continue;    // long file

			char name[12];
			memcpy(name, entry.name, 11);
			name[11] = '\0';
			for (int j = 10; j >= 0; j--)
				if (name[j] == ' ') name[j] = '\0'; else break;

			// handle ".."
			if (strcmp(dirName, "..") == 0 && strcmp(name, "..") == 0)
			{
				uint32_t parent = (entry.highCluster << 16) | entry.lowCluster;
				return parent ? parent : 2;
			}

			// check directory
			if ((entry.attr & 0x10) && strcmp(name, dirName) == 0)
			{
				return (entry.highCluster << 16) | entry.lowCluster;
			}
		}

		// move to next cluster in the chain
		cluster = getNextCluster(image, cluster, reservedSectorCount, fatSize, bytesPerSector);
	}

	fprintf(stderr, "cd: %s: No such directory\n", dirName);
	return currentCluster; // not found in any cluster
}

int openFile(FILE* image, uint32_t currentCluster, const char* filename, const char* flags, const char* curpath, uint16_t bytesPerSector, uint8_t sectorsPerCluster,uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize)
{
	int mode = parseMode(flags); //call the helper function to parse the mode from the flags
	if (mode == -1) //invalid mode 
	{
		fprintf(stderr, "open: invalid mode\n");
		return -1;
	}

	if (isFileOpen(filename)) //check if the file is already open
	{
		fprintf(stderr, "open: file already open\n");
		return -1;
	}

	int fd = findFreeFD();//find a free file descriptor in the open files table
	if (fd == -1) 
	{
		fprintf(stderr, "open: too many open files\n");
		return -1;
	}

	// Search directory same as cd and ls.
	uint32_t firstDatasec = reservedSectorCount + (numFat * fatSize);
	uint32_t cluster = currentCluster;

	while (cluster < 0x0FFFFFF8) 
	{
		uint32_t firstSec = ((cluster - 2) * sectorsPerCluster) + firstDatasec;
		fseek(image, firstSec * bytesPerSector, SEEK_SET);

		int entries = (bytesPerSector * sectorsPerCluster) / sizeof(DirEntry);
		DirEntry entry;

		for (int i = 0; i < entries; i++) 
		{
			if (fread(&entry, sizeof(DirEntry), 1, image) != 1)
				break;

			if (entry.name[0] == 0x00) break;
			if (entry.name[0] == 0xE5) continue;
			if (entry.attr == 0x0F) continue;

			// skip directories
			if (entry.attr & 0x10)
				continue;

			char name[12];
			memcpy(name, entry.name, 11);
			name[11] = '\0';

			for (int j = 10; j >= 0; j--)
			{
				if (name[j] == ' ') 
					name[j] = '\0'; 
				else break;
			}

			if (strcmp(name, filename) == 0) 
			{
				// FOUND FILE then populate table
				openFiles[fd].inUse = 1;
				strcpy(openFiles[fd].name, name);

				if (strcmp(curpath, "/") == 0)//if we are at the home
				{
					sprintf(openFiles[fd].path, "%s", "fat32.img");//then the only path we need is the fat32.img. That is the path
				}
				else//if not at home (any other directory in the home)
				{
					sprintf(openFiles[fd].path, "fat32.img%s", curpath);//then add the current path to the fat32.img to get the full path to the file.
				}
				openFiles[fd].firstCluster = (entry.highCluster << 16) | entry.lowCluster;
				openFiles[fd].size = entry.fileSize;
				openFiles[fd].offset = 0;
				openFiles[fd].mode = mode;


				printf("opened %s\n", name);
				return fd;
			}
		}

		cluster = getNextCluster(image, cluster, reservedSectorCount, fatSize, bytesPerSector);
	}

	fprintf(stderr, "open: file not found\n");
	return -1;
}

int closeFile(const char* filename)
{
	for (int i = 0; i < MAX_OPEN_FILES; i++)//going through the open file list
	{
		if (openFiles[i].inUse && strcmp(openFiles[i].name, filename) == 0)//if the file is open and the name matches
		{
			// "close" file (reset entry)
			openFiles[i].inUse = 0;
			openFiles[i].name[0] = '\0';
			openFiles[i].firstCluster = 0;
			openFiles[i].size = 0;
			openFiles[i].offset = 0;
			openFiles[i].mode = 0;

			printf("closed %s\n", filename);
			return 0;
		}
	}

	fprintf(stderr, "close: file not open or does not exist\n");
	return -1;
}

void lsof()
{
  //keeps track of found or not
  int found = 0;

  //loop through the files table
  for (int i = 0; i < MAX_OPEN_FILES; i++)
  {
    //only print files that are open
    if (openFiles[i].inUse)
    {
      found = 1;

      char *modeStr;
      //for printing out permissions
      if (openFiles[i].mode == 0)
        modeStr = "r";
      else if (openFiles[i].mode == 1)
        modeStr = "w";
      else
        modeStr = "rw";

      printf("%-6d %-15s %-6s %-10u %-10s\n", i, openFiles[i].name, modeStr, openFiles[i].offset, openFiles[i].path);
    }
  }

   if (!found)
    {
        printf("No files open\n");
    }
}

void lseek(const char* filename, int newOffset)
{
  //loop through files
  for (int i = 0; i < MAX_OPEN_FILES; i++)
  {
    //if the file given matches one in the open file
    if (openFiles[i].inUse && strcmp(openFiles[i].name, filename) == 0)
    {
      //check if offset can be done (new offset cant be bigger than file size)
      if (newOffset > openFiles[i].size)
      {
        fprintf(stderr, "ERROR:Given offset is larger than file size");
        return;
      }

      //update the offset
      openFiles[i].offset = newOffset;

      printf("Offset has been updated to %d for %s\n", newOffset, filename);
      return;
    }
  }

  //error for if file not found
  fprintf(stderr, "ERROR:Given file does not exist\n");
}

//taking in image file and multiple location factors
void read(FILE* image, const char* filename, int size, uint16_t bytesPerSector, uint8_t sectorsPerCluster, uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize)
{
  //loop through open files
  for (int i = 0; i < MAX_OPEN_FILES; i++)
  {
    //check if file exist
    if (openFiles[i].inUse && strcmp(openFiles[i].name, filename) == 0)
    {
      //error for if the file is write only
      if (openFiles[i].mode == 1)
      {
        fprintf(stderr, "permissions don't allow read to this file\n");
        return;
      }
    

      //get the file offset, filesize, and cluster
      uint32_t offset = openFiles[i].offset;
      uint32_t fileSize = openFiles[i].size;
      uint32_t cluster = openFiles[i].firstCluster; 

      //readjust the size for reading past EOF
      int EOFreached;
      if (offset + size > fileSize)
      {
        size = fileSize - offset;
        EOFreached = 1;
      }

      //get current cluster size
      uint32_t clusterSize = bytesPerSector * sectorsPerCluster;

      //need to skip clusters then read, then skip again
      uint32_t clustersToSkip = offset / clusterSize;
      uint32_t offsetIncluster = offset % clusterSize; //automatically gets each offset 

      for (uint32_t j = 0; j < clustersToSkip; j++)
      {
        cluster = getNextCluster(image, cluster, reservedSectorCount, fatSize, bytesPerSector);
      }

      uint32_t bytesLeft = size;

      //the last cluster in a fat 32 is located at x0FFFFFF8 so we loop
      //till we hit that or run out of bytes
      //this function gets the exact cluster locations and reads them
      while (bytesLeft > 0 && cluster < 0x0FFFFFF8)
      {
        //these get the correct position and offse of each cluster
        uint32_t firstDataSector = reservedSectorCount + (numFat * fatSize);
        uint32_t firstSector = ((cluster - 2) * sectorsPerCluster) + firstDataSector;
        uint32_t byteOffset = firstSector * bytesPerSector;

        //this then jumps to that start location of the cluster
        fseek(image, byteOffset, SEEK_SET);
        //Then we handle the offset, but we need to reset this offsetIncluster value
        fseek(image, offsetIncluster, SEEK_CUR);
        
        //need to calculate how much we need to read for each cluster
        uint32_t bytesToRead = clusterSize - offsetIncluster;

        //ensures the max you can read from the cluster is whats left in it
        if (bytesToRead > bytesLeft)
          bytesToRead = bytesLeft;

        //now wer can read the cluster
        //since we already calculated the offset we can just read 1 for all
        char buffer[1024];
        fread(buffer, 1, bytesToRead, image);

        // print data
        for (uint32_t k = 0; k < bytesToRead; k++)
        {
          printf("%c", buffer[k]);
        }

        bytesLeft -= bytesToRead;
        offsetIncluster = 0; // only first iteration uses offset

        // move to next cluster
        cluster = getNextCluster(image, cluster, reservedSectorCount, fatSize, bytesPerSector);

      }
  
      if (EOFreached)
      {
        fprintf(stderr, "Error: reached end of file\n");
      }

      //update to the new offset
      openFiles[i].offset += size;

      return;
    }
  }

  //filenot found error
  fprintf(stderr, "file does not exist\n");
}

void mv(FILE* image, uint32_t currentCluster, const char* src, const char* dest, uint16_t bytesPerSector, uint8_t sectorsPerCluster, uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize)
{
	if (isFileOpen(src))
	{
		fprintf(stderr, "mv: source file is currently open\n");
		return;
	}
	uint32_t firstDatasec = reservedSectorCount + (numFat * fatSize); //find the first data sector
	uint32_t cluster = currentCluster;
	DirEntry srcEntry;
	long srcOffset = -1;
	int foundSrc = 0;//find the source file you want to mv

	while (cluster < 0x0FFFFFF8)
	{
		uint32_t firstSec = ((cluster - 2) * sectorsPerCluster) + firstDatasec; //first sector of the cluster
		uint32_t offset = firstSec * bytesPerSector;//convert to offset
		fseek(image, offset, SEEK_SET);
		int entries = (bytesPerSector * sectorsPerCluster) / sizeof(DirEntry);
		DirEntry entry;
		for (int i = 0; i < entries; i++)
		{
			long offset = ftell(image);//get the offset of the image

			if (fread(&entry, sizeof(DirEntry), 1, image) != 1)
				break;
			if (entry.name[0] == 0x00) //end of the directory
				break;
			if (entry.name[0] == 0xE5) //deleted
				continue;
			if (entry.attr == 0x0F)//long file
				continue;

			char name[12];
			memcpy(name, entry.name, 11);
			name[11] = '\0';
			for (int j = 10; j >= 0; j--)
			{
				if (name[j] == ' ')
					name[j] = '\0';
				else break;
			}
			if (strcmp(name, src) == 0)//if we find the match
			{
				srcEntry = entry;//make the match value into what we have.
				srcOffset = offset;
				foundSrc = 1;
				break;
			}
		}
		if (foundSrc)//found the source file end
			break;
		cluster = getNextCluster(image, cluster, reservedSectorCount, fatSize, bytesPerSector);//if not then we go on to the next cluster to check

	}

	if (!foundSrc)//doest not exist/didn't find the file
	{
		fprintf(stderr, "error: source not found\n");
		return;
	}

	cluster = currentCluster;
	int destIsDir = 0;
	uint32_t destCluster = 0;
	int destFound = 0;
	while (cluster < 0x0FFFFFF8)
	{
		uint32_t firstSec = ((cluster - 2) * sectorsPerCluster) + firstDatasec; //first sector of the cluster
		uint32_t offset = firstSec * bytesPerSector;//convert to offset
		fseek(image, offset, SEEK_SET);
		int entries = (bytesPerSector * sectorsPerCluster) / sizeof(DirEntry);
		DirEntry entry;
		for (int i = 0; i < entries; i++)
		{

			if (fread(&entry, sizeof(DirEntry), 1, image) != 1)
				break;
			if (entry.name[0] == 0x00) //end of the directory
				break;
			if (entry.name[0] == 0xE5) //deleted
				continue;
			if (entry.attr == 0x0F)//long file
				continue;

			char name[12];
			memcpy(name, entry.name, 11);
			name[11] = '\0';
			for (int j = 10; j >= 0; j--)
			{
				if (name[j] == ' ')
					name[j] = '\0';
				else
					break;
			}
			if (strcmp(name, dest) == 0)//if you find the destionation which is the directory.
			{


				if (entry.attr & 0x10) //is a directory
				{
					destIsDir = 1;
					destCluster = (entry.highCluster << 16) | entry.lowCluster; //set the cluster
					destFound = 1;//update the value to reflect you found it
					break;
				}
				else
				{
					fprintf(stderr, "error: destination is a file\n");
					return;
				}
			}

		}
		if (destIsDir)//found, end the loop
			break;
		cluster = getNextCluster(image, cluster, reservedSectorCount, fatSize, bytesPerSector);//if not found check the next cluster
	}


	if (!destFound)//the desitination is a file (renaming)
	{
		DirEntry newEntry = srcEntry;
		memset(newEntry.name, ' ', 11);//clear the name 
		char name[8] = { 0 };
		strncpy(name, dest, 7);
		for (int i = 0; i < strlen(name); i++)
		{
			newEntry.name[i] = toupper((unsigned char)name[i]);//set to upper letter
		}
		fseek(image, srcOffset, SEEK_SET);
		fwrite(&newEntry, sizeof(DirEntry), 1, image);

		printf("Moved %s to %s\n", src, dest);
		return;

	}
	//this is if we are moving it into a directory
	fseek(image, srcOffset, SEEK_SET);
	uint8_t deleted = 0xE5;
	fwrite(&deleted, 1, 1, image);//we have to delete the entry in the current direcctory and move it into the other one.
	cluster = destCluster;
	while (cluster < 0x0FFFFFF8)
	{
		uint32_t firstSec = ((cluster - 2) * sectorsPerCluster) + firstDatasec; //first sector of the cluster
		uint32_t offset = firstSec * bytesPerSector;//convert to offset
		fseek(image, offset, SEEK_SET);
		int entries = (bytesPerSector * sectorsPerCluster) / sizeof(DirEntry);
		DirEntry entry;
		for (int i = 0; i < entries; i++)
		{
			long offset = ftell(image);//get the offset of the image

			if (fread(&entry, sizeof(DirEntry), 1, image) != 1)//DNE
				break;
			if (entry.name[0] == 0x00 || entry.name[0] == 0xE5)//matches
			{
				fseek(image, offset, SEEK_SET);//find the thing
				fwrite(&srcEntry, sizeof(DirEntry), 1, image);//send to directory
				printf("Moved %s into %s\n", src, dest);
				if (srcEntry.attr & 0x10)
				{
					uint32_t srcDirCluster = (srcEntry.highCluster << 16) | srcEntry.lowCluster;
					uint32_t srcFirstSec = ((srcDirCluster - 2) * sectorsPerCluster) + firstDatasec;
					uint32_t srcDirOffset = srcFirstSec * bytesPerSector;
					fseek(image, srcDirOffset, SEEK_SET);
					DirEntry dotEntry, dotDotEntry;
					fread(&dotEntry, sizeof(DirEntry), 1, image);
					fread(&dotDotEntry, sizeof(DirEntry), 1, image);
					dotDotEntry.highCluster = (destCluster >> 16) & 0xFFFF;
					dotDotEntry.lowCluster = destCluster & 0xFFFF;
					fseek(image, srcDirOffset + sizeof(DirEntry), SEEK_SET);
					fwrite(&dotDotEntry, sizeof(DirEntry), 1, image);

				}
				fflush(image);
				return;
			}
		}
		cluster = getNextCluster(image, cluster, reservedSectorCount, fatSize, bytesPerSector);//not found go to next cluster
	}
	fprintf(stderr, "error: failed to move file\n");
}
void write(FILE* image, const char* filename, int size, const char* data, uint16_t bytesPerSector, uint8_t sectorsPerCluster, uint16_t reservedSectorCount, uint8_t numFat, uint32_t fatSize)
{
  //loop through open files 
  for (int i = 0; i < MAX_OPEN_FILES; i++)
  {
    if (openFiles[i].inUse && strcmp(openFiles[i].name, filename) == 0)
    {
      //can only go further if not in read
      if (openFiles[i].mode == 0)
      {
        fprintf(stderr, "File cannot be written to\n");
        return;
      }

      uint32_t offset = openFiles[i].offset;
      uint32_t fileSize = openFiles[i].size;
      uint32_t cluster = openFiles[i].firstCluster;

      //if we go past file size, extend file (per project spec)
      if (offset + size > fileSize)
      {
        openFiles[i].size = offset + size;
      }

      //calculating offset and cluster location (same as read)
      uint32_t clusterSize = bytesPerSector * sectorsPerCluster;
      uint32_t clustersToSkip = offset / clusterSize;
      uint32_t offsetInCluster = offset % clusterSize;
      
      for (uint32_t j = 0; j < clustersToSkip; j++)
      {
        cluster = getNextCluster(image, cluster, reservedSectorCount, fatSize, bytesPerSector);
      }

      //safety check in case cluster chain ends early
      if (cluster >= 0x0FFFFFF8)
      {
        fprintf(stderr, "Error: cluster chain ended unexpectedly\n");
        return;
      }
      
      //need to get the new size but also the new data
      uint32_t bytesLeft = size;
      const char* dataPtr = data;

      //same loop as read, except we write instead
      while (bytesLeft > 0 && cluster < 0x0FFFFFF8)
      {
        //these get the correct position and offset of each cluster
        uint32_t firstDataSector = reservedSectorCount + (numFat * fatSize);
        uint32_t firstSector = ((cluster - 2) * sectorsPerCluster) + firstDataSector;
        uint32_t byteOffset = firstSector * bytesPerSector;

        //this then jumps to that start location of the cluster
        fseek(image, byteOffset, SEEK_SET);

        //Then we handle the offset, but we need to reset this offsetIncluster value
        fseek(image, offsetInCluster, SEEK_CUR);
        
        //need to calculate how much we need to write for each cluster
        uint32_t bytesToWrite = clusterSize - offsetInCluster;

        //ensures the max you can write to the cluster is whats left in it
        if (bytesToWrite > bytesLeft)
          bytesToWrite = bytesLeft;

        fwrite(dataPtr, 1, bytesToWrite, image);
        
        //same as above but now we need to update data
        bytesLeft -= bytesToWrite;
        dataPtr += bytesToWrite;
        offsetInCluster = 0;
        
        // move to next cluster
        cluster = getNextCluster(image, cluster, reservedSectorCount, fatSize, bytesPerSector);

      }

      //update the offset 
      openFiles[i].offset += size;

      fflush(image);

      return;
    }
  }

  fprintf(stderr, "file does not exist\n");
}
