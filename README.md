Project Name: FAT32 File System
(Use the main branch for the project submission)

## Group Members
  William Kilduff: wrk24@fsu.edu
  Kayla Callis: kmc23f@fsu.edu 
  Joshua Kim: jjk22g@fsu.edu 
## Division of Labor

### Part 1: Mounting the Image File
- **Responsibilities**: The user will need to mount the image file through command line arguments: ./filesys [FAT32 ISO]. Along with the info commmand and the exit command. Info: Parses the boot sector. Prints the field name and corresponding value for each entry, one per line. Exit: Safely closes the program and frees up any allocated resources.
- **Assigned to**: William Kilduff

### Part 2: Navigation
- **Responsibilities**: Implementing the cd and ls command. CD: Changes the current working directory to DIRNAME. Your code will need to maintain the current working directory state. Print an error if DIRNAME does not exist or is not a directory. Ls: Print the name filed for the directories and files within the current working directory including the “.” and “..” directories.
- **Assigned to**: Joshua Kim

### Part 3:  Create
- **Responsibilities**: Implementing the mkdir and creat command. Mkdir: Creates a new directory in the current working directory with the name DIRNAME. Creat: Creates a file in the current working directory with a size of 0 bytes and with a name of FILENAME.
- **Assigned to**: Kayla Callis

### Part 4: Read
- **Responsibilities**: Implementation of the open, close, lsof, lseek, and read command. Open: Opens a file named FILENAME in the current working directory. A file can only be read from or written to if it is opened first. You will need to maintain some data structure of opened files and add FILENAME to it when open is called. Close: Closes a file FILENAME in current working directory. Lsof: Lists all opened files. Lseek: Set the offset (in bytes) of file FILENAME for further reading or writing. Read: Read the data from a file in the current working directory with the name FILENAME.
- **Assigned to**: William Kilduff and Joshua Kim

### Part 5: External Update
- **Responsibilities**: Implementaion of the write and mv command. Write: Writes to a file in the current working directory with the name FILENAME. Mv: Move a file or directory from FILENAME to NEW_FILENAME/DIRECTORY.
- **Assigned to**: William Kilduff and Joshua Kim

### Part 6: Delete
- **Responsibilities**: Impelementation of the rm and rmdir command. Rm: Deletes the file named FILENAME from the current working directory. Rmdir: Remove a directory by the name of DIRNAME from the current working directory.
- **Assigned to**: Kayla Callis
## File Listing
```sh
project-3-group-10
│
├── src/
│ ├── main.c
│ ├── filesystem.c
│ ├── lexer.c
│ ├── delete.c
│
├── include/
│ ├── filesystem.h
│ ├── lexer.h
│ ├── delete.h
│
├── README.md
└── Makefile
```
## How to Compile & Execute

### Requirements
- **Compiler**: GCC
- **Dependencies**:
 <stdio.h>
 <errno.h>
 <stdint.h>
 <string.h>
 <stdlib.h>
 <stdbool.h>

### Compilation
For a C/C++ example:
```
type "make"
```
This will build the executable called "shell"
### Execution
```
type "shell fat32.img"
```
This will run the program ...


## Bugs
- **Bug 1**: The error message for End of file reached prints out in some specific cases when it should not.
  
 
