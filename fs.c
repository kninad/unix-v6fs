/* Author: Guillermo Vazquez

* UTD ID: GSV200000

* CS 5348.001 Operating Systems

* Prof. S Venkatesan

* Project - 2 Part 2*

***************

* Compilation :-$ gcc fs.c -o fs.o
	
* Run using :- $ ./fs.o

******************/
/*
 Commands supported by the program:
	Main commands required for submission:
		For part 1 (enhanced to take paths - both absolute and relative - instead of simple file names):
			1. initfs filename nBlocks nInodes: Initilizes/retrieves the file system with nBlocks number of blocks,
			and nInodes number of inodes. If retrieving an already existing filesystem, no need to provide nBlocks nor nInodes
			2. cpin extFile vFilePath: Copies external file to internal file system
			3. cpout vFilePath extFile: Copies internal file to external file
			4. q: save all work and exit the program.
		
		For part 2:
			5. mkdir pathToFolder: creates directory at the specified path
			6. rm pathToFile: removes the file specified in the path (only deletes files, not directories)
			7. cd path: set current directory to path
	
	Debugging commands: these are some extra functions not required for submission
						but I used them for debugging and I decided to keep them there
		8. ls: list files under root directory
		9. superblock: displays superblock info
		10. root: displays root inode info
		11. inode n: displays the info under inode number n
		12. pwd: prints the working directory
		
 File name is limited to 14 characters.
 ***********************************************************************/

#include<stdio.h>
#include<fcntl.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<stdlib.h>

//#pragma pack(1) //old solution to padding problem, now reorders the members of struct
#define FREE_SIZE 152  
#define I_SIZE 200
#define BLOCK_SIZE 1024    
#define ADDR_SIZE 11
#define INPUT_SIZE 256
#define MAX_NAME_LENGTH 14
typedef struct {
	//this is not the order of variables shown in the v6 handout, but solves the padding problem
	unsigned short isize; //number of blocks for inodes
	unsigned short fsize; //total number of blocks
	unsigned short ninode;
	unsigned short nfree; 
	unsigned int free[FREE_SIZE]; //free[0] up to free[nfree-1] free blocks
	unsigned short inode[I_SIZE]; //similar to nfree, free[]
	char flock;
	char ilock;
	unsigned short fmod;
	unsigned short time[2];
} superblock_type;

superblock_type superBlock;

//note the differences with the i-node structure in v6-file system handout
typedef struct {
unsigned short flags; //same
unsigned short nlinks; // 2 byte, as opposed to 1 byte in v6
unsigned short uid; // 2 byte, as opposed to 1 byte in v6
unsigned short gid;// 2 byte, as opposed to 1 byte in v6
unsigned int size; //4 byte int, as opposed to 3byte in v6
unsigned int addr[ADDR_SIZE]; // 12*4=48 bytes, as opposed to 2*8=16 bytes in v6
unsigned short actime[2]; //same
unsigned short modtime[2]; //same
} inode_type;  //32 extra bytes = 64 bytes total 

inode_type inode, rootInode, curInode, workInode;	//inode of the current working directory

typedef struct {
	unsigned short inode;
	unsigned char filename[MAX_NAME_LENGTH+1]; //+1 for the /0 end
} dir_type;

dir_type root, dir;

int fileDescriptor ;		//file descriptor 
unsigned short curInodeN; //current inodeN
const unsigned short inode_alloc_flag = 0100000;
const unsigned short dir_flag = 040000;
const unsigned short dir_large_file = 010000;
const unsigned short dir_access_rights = 000777; // User, Group, & World have all access privileges 
const unsigned short INODE_SIZE = 64; // inode has been doubled
char* buffer; //will be used for reading blocks
 
int initfs(char* path, unsigned short total_blcks,unsigned short total_inodes);
void add_block_to_free_list( int blocknumber , unsigned int *empty_buffer );
void create_root();

void retrieveSuperblock();
void retrieveRootInode();
int addFile(unsigned short, unsigned char*, unsigned char*);
void listFiles();
unsigned char* getFilenameAndPath(unsigned char**);
int cpinInit();
int cpin(int, unsigned char*, unsigned char*);
int cpoutInit();
int cpout(int, int);
int mkdirInit();
int mkdir(unsigned char*, unsigned char*);
int cdInit();
int cd(unsigned char*);
int rmInit();
int canDeleteWorkInode();
unsigned short findFreeInode();
int addressOfInode(unsigned short);
unsigned int findFreeDataBlock();
void printRootInode();
void printCurrentInode();
void printSuperblock();
void printInode(int);
void printBlock(int);
void pwd();
unsigned short inodeNOf(unsigned char* filename);


int main(){
 
	char input[INPUT_SIZE];
	char *splitter;
	unsigned int numBlocks = 0, numInodes = 0;
	char *filepath;
	buffer = malloc(BLOCK_SIZE);
	printf("Size of super block = %d , size of i-node = %d\n",sizeof(superBlock),sizeof(inode));
	
	while(1) {
		printf("Enter command: ");
		scanf(" %[^\n]s", input);
		splitter = strtok(input," ");
		
		if(strcmp(splitter, "initfs") == 0){
				preInitialization();
				splitter = NULL;
		} 
		else if (strcmp(splitter, "q") == 0) {
	 
			//save superblock
			lseek(fileDescriptor, BLOCK_SIZE, 0);
			write(fileDescriptor, &superBlock, BLOCK_SIZE);
			 
			//save root inode
			lseek(fileDescriptor, 2 * BLOCK_SIZE, 0);
			write(fileDescriptor, &rootInode, INODE_SIZE);   
			 
			return 0;
		 
		}
		//commands that need file system initialized
		else{
		
			if(!fileDescriptor){
				printf("\tFile system has to be initialized first\n");
				continue;
			}
		
			else if(strcmp(splitter, "cpin") == 0){
				cpinInit();
				splitter = NULL;
			}        
		
			else if(strcmp(splitter, "cpout") == 0){
				cpoutInit();
				splitter = NULL;
			} 
			
			else if(strcmp(splitter, "mkdir") == 0){
				mkdirInit();
				splitter = NULL;
			}  
			
			else if(strcmp(splitter, "cd") == 0){
				cdInit();
				splitter = NULL;
			}  
			
			else if(strcmp(splitter, "rm") == 0){
				rmInit();
				splitter = NULL;
			}  
		
			else if(strcmp(splitter, "ls") == 0){
				listFiles();
				splitter = NULL;
			} 
		
			else if(strcmp(splitter, "pwd") == 0){
				pwd();
				splitter = NULL;
			} 
		
			else if(strcmp(splitter, "superblock") == 0){
				printSuperblock();
				splitter = NULL;
			}   
		
			else if(strcmp(splitter, "root") == 0){
				printRootInode();
				splitter = NULL;
			}
			else if(strcmp(splitter, "current") == 0){
				printCurrentInode();
				splitter = NULL;
			}
			else if(strcmp(splitter, "inode") == 0){
				printInode(atoi(strtok(NULL, " ")));
				splitter = NULL;
			}     
		
			else if(strcmp(splitter, "block") == 0){
				printBlock(atoi(strtok(NULL, " ")));
				splitter = NULL;
			}  
			
			else{
				printf("\tUnknown command\n");
			}
		}	 
	}
}

/**
 * Preinitializacion of initfs
 */
int preInitialization(){

	char *n1, *n2;
	unsigned int numBlocks = 0, numInodes = 0;
	char *filepath;
	
	filepath = strtok(NULL, " ");
	n1 = strtok(NULL, " ");
	n2 = strtok(NULL, " ");
				 
	if(access(filepath, F_OK) != -1) {
		fileDescriptor = open(filepath, O_RDWR);
		if(!fileDescriptor){
			printf("\tFilesystem already exists but open() failed with error [%s]\n", strerror(errno));
			return 1;
		}
		printf("\tFilesystem already exists and the same will be used.\n");
		retrieveSuperblock();
		retrieveRootInode();
		
	} else {
		if (!n1 || !n2)
			printf("\tAll arguments(path, number of inodes and total number of blocks) have not been entered\n");			
		else {
			numBlocks = atoi(n1);
			numInodes = atoi(n2);
			if( initfs(filepath,numBlocks, numInodes )){
				printf("\tThe file system is initialized\n");	
			} else {
				printf("\tError initializing file system. Exiting... \n");
				return 1;
			}
		}
	}
	
	return 0;
}

/*
 * Initializes the file system at the path given by 'path'
 * , with 'blocks' number of blocks and 'inodes' number of inodes
 */
int initfs(char* path, unsigned short blocks,unsigned short inodes) {

	 unsigned int buffer[BLOCK_SIZE/4];
	 int bytes_written;
	 
	 unsigned short i = 0;
	 superBlock.fsize = blocks;
	 unsigned short inodes_per_block= BLOCK_SIZE/INODE_SIZE;
	 
	 if((inodes%inodes_per_block) == 0)
		superBlock.isize = inodes/inodes_per_block;
	 else
		superBlock.isize = (inodes/inodes_per_block) + 1;
	 
	 if((fileDescriptor = open(path,O_RDWR|O_CREAT,0700))== -1){
		printf("\n\topen() failed with the following error [%s]\n",strerror(errno));
		return 0;
	}
			 
	 for (i = 0; i < FREE_SIZE; i++)
		superBlock.free[i] =  0;			//initializing free array to 0 to remove junk data. free array will be stored with data block numbers shortly.
	 superBlock.nfree = 0;
	 
	 for (i = 0; i < I_SIZE && i < inodes - 1; i++)
		superBlock.inode[i] = i + 2;		//Initializing the inode array to inode numbers
	 superBlock.ninode = i;

	 superBlock.flock = 'a'; 					//flock,ilock and fmode are not used.
	 superBlock.ilock = 'b';					
	 superBlock.fmod = 0;
	 superBlock.time[0] = 0;
	 superBlock.time[1] = 1970;
	 
	 // writing zeroes to all inodes in ilist
	 for (i = 0; i < BLOCK_SIZE/4; i++) 
	 	buffer[i] = 0;
				
	 for (i = 0; i < superBlock.isize; i++)
	 	write(fileDescriptor, buffer, BLOCK_SIZE);
	 
	 // Create root directory
	 create_root();
	 for(i = 3 + superBlock.isize; i <= superBlock.fsize; ++i)
		add_block_to_free_list(i, buffer);

	 lseek(fileDescriptor, BLOCK_SIZE, SEEK_SET);
	 write(fileDescriptor, &superBlock, BLOCK_SIZE); // writing superblock to file system
	 return 1;
}

/*
 *  Add Data block number block_number to free list
 */
void add_block_to_free_list(int block_number,  unsigned int *empty_buffer){

	if(superBlock.nfree == FREE_SIZE ){
		int free_list_data[BLOCK_SIZE / 4], i;
		free_list_data[0] = FREE_SIZE;
		
		for(i = 0; i < BLOCK_SIZE / 4; i++ )
			if(i < FREE_SIZE )
				free_list_data[i + 1] = superBlock.free[i];
			else
				free_list_data[i + 1] = 0; // getting rid of junk data in the remaining unused bytes of header block
		
		lseek( fileDescriptor, (block_number) * BLOCK_SIZE, 0 );
		write( fileDescriptor, free_list_data, BLOCK_SIZE ); // Writing free list to header block
		superBlock.nfree = 0;
		
	} else {
		lseek( fileDescriptor, (block_number) * BLOCK_SIZE, 0 );
		write( fileDescriptor, empty_buffer, BLOCK_SIZE );  // writing 0 to remaining data blocks to get rid of junk data
	}

	superBlock.free[superBlock.nfree] = block_number;  // Assigning blocks to free array
	++superBlock.nfree;
}

/* 
 * Create root directory
 */
void create_root() {

	int i, root_data_block = 2 + superBlock.isize; // Allocating first data block to root directory
	rootInode.flags = inode_alloc_flag | dir_flag | dir_large_file | dir_access_rights;   		// flag for root directory 
	rootInode.nlinks = 0; 
	rootInode.uid = 0;
	rootInode.gid = 0;
	rootInode.size = 32;
	rootInode.addr[0] = root_data_block;
	
	for( i = 1; i < ADDR_SIZE; i++ )
		inode.addr[i] = 0;
	
	rootInode.actime[0] = 0;
	rootInode.modtime[0] = 0;
	rootInode.modtime[1] = 0;
	
	lseek(fileDescriptor, 2 * BLOCK_SIZE, 0);
	write(fileDescriptor, &rootInode, INODE_SIZE);   // 
	
	root.inode = 1;   // root directory's inode number is 1.
	root.filename[0] = '.';
	root.filename[1] = '\0';
	
	lseek(fileDescriptor, root_data_block * BLOCK_SIZE, 0);
	write(fileDescriptor, &root, 16);
	
	root.filename[0] = '.';
	root.filename[1] = '.';
	root.filename[2] = '\0';
	
	write(fileDescriptor, &root, 16);
	//make current Inode be root Inode
	memcpy(&curInode, &rootInode, INODE_SIZE);
	curInodeN = 1;
}

/*
 * prints the info of the inode of the root
 * */
void printRootInode(){
	printf("\tFlags: 0x%X\n", rootInode.flags);
	printf("\tnlinks: %d\n", rootInode.nlinks);
	printf("\tuid: %d\n", rootInode.uid);
	printf("\tgid: %d\n", rootInode.uid);
	printf("\tsize: %d bytes\n", rootInode.size);
	printf("\tData blocks: ");
	int i = 0;
	for(; i < ADDR_SIZE && rootInode.addr[i]!=0; ++i)
		printf("%d ", rootInode.addr[i]);
	printf("\n");

}

void printCurrentInode(){
	printf("\tFlags: 0x%X\n", curInode.flags);
	printf("\tnlinks: %d\n", curInode.nlinks);
	printf("\tuid: %d\n", curInode.uid);
	printf("\tgid: %d\n", curInode.uid);
	printf("\tsize: %d bytes\n", curInode.size);
	printf("\tData blocks: ");
	int i = 0;
	for(; i < ADDR_SIZE && curInode.addr[i]!=0; ++i)
		printf("%d ", curInode.addr[i]);
	printf("\n");

}

/*
 * prints the superblock info
 * */
void printSuperblock(){
	printf("\tisize: %d\n", superBlock.isize);
	printf("\tfsize: %d\n", superBlock.fsize);
	printf("\tnfree: %d\n", superBlock.nfree);
	printf("\tninode: %d\n", superBlock.ninode);
	printf("\tnext free block at %d\n", superBlock.free[superBlock.nfree-1]);	
}

/*
 * prints the information in inode number inodeN
 * gives error if invalid inodeN
 * */
void printInode(int inodeN){
	if(inodeN < 1 || inodeN>superBlock.isize*BLOCK_SIZE/INODE_SIZE){
		printf("\tInvalid inode number\n");
		return;
	}
	int address = addressOfInode(inodeN);
	lseek(fileDescriptor, address, SEEK_SET);
	read(fileDescriptor, &inode, INODE_SIZE);
	if(!(inode.flags & inode_alloc_flag)){
			printf("\tInode is unallocated\n");
			return;
	}
	printf("\tFlags: 0x%X\n", inode.flags);
	printf("\tnlinks: %d\n", inode.nlinks);
	printf("\tuid: %d\n", inode.uid);
	printf("\tgid: %d\n", inode.uid);
	printf("\tsize: %d bytes\n", inode.size);
	printf("\tData blocks: ");
	int i;
	for(i = 0; i < ADDR_SIZE && inode.addr[i]!=0; ++i)
		printf("%d ", inode.addr[i]);
	printf("\n");
}

/*
 * prints the block number given by blockN
 * */
void printBlock(int blockN){
	lseek(fileDescriptor, blockN*BLOCK_SIZE, SEEK_SET);
	read(fileDescriptor, buffer, BLOCK_SIZE);
	printf("%s\n", buffer);
}

/*
 *	separates a long path to a file into pathToFile and filename
 * 
 * e,g /home/gsv/file.txt -> filename = file.txt, path = /home/gsv
 * 
 * param: path - pointer to pointer to path, it will be modified by removing the filename 
 * returns NULL if filename is too long
 */
unsigned char* getFilenameAndPath(unsigned char** path){
	unsigned char *filename = strrchr( *path,'/');
	if(filename){ 
		if(filename != *path)
			*filename++ = '\0';
		else{
			filename++;
			*path = malloc(2);
			*path = "/\0";
		}
		
	}
	else{ // if '/' is not found, filename is purely filename
		if(strlen(*path)>MAX_NAME_LENGTH+1)
			return NULL;
		filename = malloc(MAX_NAME_LENGTH+1);
		strncpy(filename, *path, MAX_NAME_LENGTH);
		filename[MAX_NAME_LENGTH] = '\0';
		*path[0] = '\0';
	}	
	
	//printf("Filename is %s\n", filename);
	//printf("Path is %s\n",*path);
	if(strlen(filename)>MAX_NAME_LENGTH+1)
			return NULL;	
	return filename;
}

/*
 * initialization for cpin command
 * */
int cpinInit(){
	unsigned char *extFilePath, *vFilePath;
	extFilePath = strtok(NULL, " ");
	vFilePath = strtok(NULL, " ");
	if( !extFilePath || !vFilePath ){
		printf("\tNot all arguments have been entered\n");
		return 1;
	}
	
	unsigned char* filename = getFilenameAndPath(&vFilePath);
	if(!filename){
		printf("\tFile name is limited to %d characters\n", MAX_NAME_LENGTH);
		return -5;
	}

	if(access(extFilePath, F_OK) != -1) {
		int fp = open(extFilePath, O_RDWR);
		if(!fp){
			printf("\tCannot open the input file\n");
			return 4;
		}
		//if we are here, the external file exists and we opened it
		int i = cpin(fp, vFilePath, filename);
		switch (i){
				case 0: printf("\tFile copied successfully\n"); break;
				case -1: printf("\tSystem ran out of free blocks\n"); break;
				case -2: printf("\tFile too big, got truncated\n"); break;
				case -3: printf("\tDirectory is full\n"); break;
				case -4: printf("\tNot copied. Ran out of inodes\n"); break;
				case 7: case 8: printf("\tInvalid path\n"); break;
				case 10: printf("\tA file with that name already exist\n"); break;
				default: printf("\tSomething went wrong, we should not be here\n"); break;
		}
		return i; 
	}
	
	else {
		printf("\tThe external file does not exist\n");
		return 5;
	}
	
		
}

/*
 *	cpin, copies the file pointed at by fp into the internal file name given by *vFilepath 
 * */
int cpin(int fp, unsigned char* path, unsigned char* filename){
	//grabbing an inode or the file
	unsigned short inodeN = findFreeInode();
	if(inodeN == 0)
		return -4;
	int temp = addFile(inodeN, path, filename);
	if( temp != 0){
		return temp;
	}
	
	int address = addressOfInode(inodeN);
	lseek(fileDescriptor, address, SEEK_SET);
	read(fileDescriptor, &inode, INODE_SIZE);	
	inode.flags = inode_alloc_flag | dir_access_rights;
	inode.nlinks = 1;
	inode.gid = 0;
	inode.uid = 0;
	inode.addr[0] = 0;

	//adding file to current working directory 
	
	//read external and copy to internal block by block
	int i = -1,bytesRead = 0;
	while((bytesRead = read(fp, buffer, BLOCK_SIZE)) > 0 && ++i < ADDR_SIZE){
		inode.addr[i] = findFreeDataBlock();
		if(inode.addr[i] == 0){
			return -1;
		}
		inode.size += bytesRead;
		lseek(fileDescriptor, inode.addr[i]*BLOCK_SIZE, SEEK_SET);
		write(fileDescriptor, buffer, bytesRead);
	}
		
		
	if(i+1 < ADDR_SIZE){
		inode.addr[i+1] = 0;
	}
	//saving inode
	lseek(fileDescriptor, address, SEEK_SET);
	write(fileDescriptor, &inode, INODE_SIZE);
		
	//save the parent inode
	lseek(fileDescriptor, addressOfInode(curInodeN), 0);
	write(fileDescriptor, &curInode, INODE_SIZE);
		
	if(i==ADDR_SIZE && bytesRead > 0){ //truncated file, it is too big
		return -2;
	}
		
	return 0; 
}

int mkdirInit(){
	unsigned char *path;
	path = strtok(NULL, " ");
	if( !path ){
		printf("\tNot all arguments have been entered\n");
		return 1;
	}
	
	unsigned char* dirName = getFilenameAndPath(&path);
	if(!dirName){
		printf("\tFile name is limited to 14 characters\n");
		return -5;
	}
	
	int i = mkdir(path, dirName);
	switch (i){
		case 0: printf("\tFolder created successfully\n"); break;
		case -1: printf("\tSystem ran out of free blocks\n"); break;
		case -4: printf("\tNot created. Ran out of inodes\n"); break;
		case 7: case 8: printf("\tInvalid path\n"); break;
		case 10: printf("\tA file with that name already exist\n"); break;
		default: printf("\tSomething went wrong, we should not be here\n"); break;
	}
	return i; 
		
}

int mkdir(unsigned char *path, unsigned char* dirName){
	//grabbing an inode or the file
	unsigned short inodeN = findFreeInode();
	if(inodeN == 0)
		return -4;
		
	//adding file to its folder 
	int temp = addFile(inodeN, path, dirName);
	if( temp != 0){
		return temp;
	}
		
	int address = addressOfInode(inodeN);
	lseek(fileDescriptor, address, SEEK_SET);
	read(fileDescriptor, &inode, INODE_SIZE);	
	inode.flags = inode_alloc_flag | dir_access_rights | dir_flag;
	inode.nlinks = 1;
	inode.gid = 0;
	inode.uid = 0;
	inode.addr[0] = 0;

	
	//adding the two entries to the directory
	inode.addr[0] = findFreeDataBlock();
	if(inode.addr[0] == 0){
		return -1;
	}
	
	dir.inode = inodeN;   // its own inode number
	dir.filename[0] = '.';
	dir.filename[1] = '\0';
	
	lseek(fileDescriptor, inode.addr[0] * BLOCK_SIZE, 0);
	write(fileDescriptor, &dir, 16);
	
	//TODO: this is inefficient af, make it better (next 2 lines)
	dir.inode = inodeNOf(path);   // parent inode number
	lseek(fileDescriptor, inode.addr[0] * BLOCK_SIZE + 16, 0);
	dir.filename[0] = '.';
	dir.filename[1] = '.';
	dir.filename[2] = '\0';
	write(fileDescriptor, &dir, 16);
	
	//saving inode
	lseek(fileDescriptor, address, SEEK_SET);
	write(fileDescriptor, &inode, INODE_SIZE);
	
	return 0;
}

int cdInit(){
	unsigned char *path;
	path = strtok(NULL, " ");
	if( !path ){
		curInode = rootInode;
		curInodeN = 1;
		printf("/tFolder changed to root\n");
		return 0;
	}
			
	int i = cd(path);
	switch (i){
		case 0: printf("\tChanged folder successfully\n"); break;
		case -1: printf("\tInvalid path\n"); break;
		case -2: printf("\tCannot cd into a file\n"); break;
		default: printf("\tSomething went wrong, we should not be here\n"); break;
	}
	return i; 
}

int cd(unsigned char* path){
	int inodeN = inodeNOf(path);
	if(inodeN == 0)
		return -1;
	if(!(workInode.flags&dir_flag))
		return -2;
	curInode = workInode;
	curInodeN = inodeN;
	return 0;
}

/*
 * adds the file in inodeN and name filename to the directory specified in path
 * */
int addFile(unsigned short inodeN, unsigned char* path, unsigned char* filename){
	int i,j, workInodeN = inodeNOf(path), address = 0;
	//printInode(workInodeN);
	if(workInodeN == 0)
		return 7; //not found
	if(!(workInode.flags&dir_flag))
		return 8; //not a directory
		
	for(i = 0; i < ADDR_SIZE && workInode.addr[i]!=0; ++i){
		lseek(fileDescriptor, workInode.addr[i]*BLOCK_SIZE, SEEK_SET);
		read(fileDescriptor, buffer, BLOCK_SIZE);
		for( j = 0; j < BLOCK_SIZE; j+=16 ){
			dir = *(dir_type*) (buffer+j);
			dir.filename[MAX_NAME_LENGTH] = '\0';
			if(dir.inode == 0){//it is free
				address = workInode.addr[i]*BLOCK_SIZE + j;
			}
			if(dir.inode != 0 && strcmp(dir.filename, filename) == 0){ //checking if file exists
				return 10; //give error	
			}
		}
	}
	
	//if we are here, the file does not exist and we can save it at address stored in 'address'
	//adding the file
	if(address!=0){
		dir.inode = inodeN;
		strncpy(dir.filename, filename,MAX_NAME_LENGTH);
		dir.filename[MAX_NAME_LENGTH] = '\0';
		lseek(fileDescriptor, address, SEEK_SET);
		write(fileDescriptor, &dir, 16);
		workInode.size += 16; //one 16 byte dir got added
		lseek(fileDescriptor, addressOfInode(workInodeN), SEEK_SET);
		write(fileDescriptor, &workInode, INODE_SIZE);
		return 0;
	}
	
	//if address was 0,we need a new block to add it
	//dataBlock is full, grab new one
	if(i+1 >= ADDR_SIZE){
		return 9; //full directory
	}
	
	inode.addr[++i] = findFreeDataBlock();
	if(inode.addr[i] == 0){
		return -4; //no more data blocks
	}
	dir.inode=inodeN;
	strncpy(dir.filename, filename, MAX_NAME_LENGTH);
	lseek(fileDescriptor, inode.addr[i]*BLOCK_SIZE, SEEK_SET);
	write(fileDescriptor, &dir, 16);
	
	return 0;
}

/*
 * returns the inode number of the path given by path
 * and sets workInode to the inode of that file 
 * returns 0 if not found, and workInode can be anything
 * */
unsigned short inodeNOf(unsigned char* path){
	//printf("Received: %s\n",path);
	//path is empty means we are using current directory
	if(path[0] == '\0'){
		workInode = curInode;
		return curInodeN;
	}
	
	int i,j, inodeN = 0, found = 0;
	//find out whether path is relative or absolute path
	//this determines starting point
	if(path[0] == '/'){ //absolute path
		workInode = rootInode;
		inodeN = 1;
		path++; 
	}
	else{
		workInode = curInode;
		inodeN = curInodeN;
	}
	//printf("%s\n", path);
	char* splitter;
	splitter = strtok(path,"/");
	while(splitter){
		if(!(workInode.flags&dir_flag))
				return 0; //somehow we ended in not a directory -> invalid path
		found = 0;
		for(i = 0; found == 0 && i < ADDR_SIZE && workInode.addr[i]!=0; ++i){
			lseek(fileDescriptor, workInode.addr[i]*BLOCK_SIZE, SEEK_SET);
			read(fileDescriptor, buffer, BLOCK_SIZE);
			for( j = 0; j < BLOCK_SIZE; j+=16 ){
				dir = *(dir_type*) (buffer+j);
				dir.filename[MAX_NAME_LENGTH] = '\0';
				if( dir.inode != 0 && strcmp(dir.filename, splitter) == 0){
					found = 1;
					inodeN = dir.inode;
					lseek(fileDescriptor, addressOfInode(inodeN), SEEK_SET);
					read(fileDescriptor, &workInode, INODE_SIZE);
					break;
				}
			}
		}
		if(found == 0)
			return 0;
		splitter = strtok(NULL, "/");
	}
		
	return inodeN;
}

/*
 * prints the current working directory, same as unix's pwd command
 */
void pwd(){
	
	if(rootInode.addr[0]==curInode.addr[0]){
		printf("\tCurrent directory: /\n");
		return;
	}
	
	char* pathTokens[20]; //20 is too much, but well...
	short parent = 0, search = curInodeN; //unknown parent
	int count = 0,i,j, found = 0;
	lseek(fileDescriptor, curInode.addr[0]*BLOCK_SIZE+16, SEEK_SET);
	read(fileDescriptor, &parent, 2);
	lseek(fileDescriptor, addressOfInode(parent), SEEK_SET);
	read(fileDescriptor, &inode, INODE_SIZE);
	//printf("Current inode is %d and searching for %d\n", parent,search);
	while(inode.addr[0]!=rootInode.addr[0] && count < 20){
		found = 0;
		for(i = 0; found == 0 && i < ADDR_SIZE && inode.addr[i]!=0; ++i){
			lseek(fileDescriptor, inode.addr[i]*BLOCK_SIZE, SEEK_SET);
			read(fileDescriptor, buffer, BLOCK_SIZE);
			for( j = 0; j < BLOCK_SIZE; j+=16 ){
				dir = *(dir_type*) (buffer+j);
				dir.filename[MAX_NAME_LENGTH] = '\0';
				if(dir.inode==search){
					found = 1;
					pathTokens[count] = malloc(MAX_NAME_LENGTH+1);
					strncpy(pathTokens[count], dir.filename,MAX_NAME_LENGTH+1);
					break;
				}
			}
		}
		search = parent;
		lseek(fileDescriptor, inode.addr[0]*BLOCK_SIZE+16, SEEK_SET);
		read(fileDescriptor, &parent, 2);
		lseek(fileDescriptor, addressOfInode(parent), SEEK_SET);
		read(fileDescriptor, &inode, INODE_SIZE);
		++count;
		//printf("Current inode is %d and searching for %d\n", parent,search);
	}
	found = 0;
	for(i = 0; found == 0 && i < ADDR_SIZE && inode.addr[i]!=0; ++i){
		lseek(fileDescriptor, inode.addr[i]*BLOCK_SIZE, SEEK_SET);
		read(fileDescriptor, buffer, BLOCK_SIZE);
		for( j = 0; j < BLOCK_SIZE; j+=16 ){
			dir = *(dir_type*) (buffer+j);
			dir.filename[MAX_NAME_LENGTH] = '\0';
			if(dir.inode==search){
				found = 1;
				pathTokens[count] = malloc(MAX_NAME_LENGTH+1);
				strncpy(pathTokens[count], dir.filename,MAX_NAME_LENGTH+1);
				break;
			}
		}
	}
	++count;
	printf("\tCurrent Directory: ");
	for(i = count - 1; i >= 0; --i){
		printf("/%s",pathTokens[i]);
	}
	printf("\n");
}

/*
 * initialization for cpout
 * */
int cpoutInit(){
	unsigned char *extFilePath, *vFilePath;
	
	vFilePath = strtok(NULL, " ");
	extFilePath = strtok(NULL, " ");
	
	if( !extFilePath || !vFilePath ){
		printf("\tNot all arguments have been entered\n");
		return -1;
	}
	
	//finding inode of internal file
	unsigned char* filename = getFilenameAndPath(&vFilePath);
	if(!filename){
		printf("\tFile name is limited to 14 characters\n");
		return -5;
	}
		
	//finding inode of internal file
	unsigned short parentInodeN = inodeNOf(vFilePath);
	if(parentInodeN == 0){
		printf("\tInvalid path\n");
		return -2;
	}
	int inodeN = 0, i, j;
	for(i = 0; inodeN == 0 && i < ADDR_SIZE && workInode.addr[i]!=0; ++i){
		lseek(fileDescriptor, workInode.addr[i]*BLOCK_SIZE, SEEK_SET);
		read(fileDescriptor, buffer, BLOCK_SIZE);
		for( j = 0; j < BLOCK_SIZE; j+=16 ){
			dir = *(dir_type*) (buffer+j);
			dir.filename[MAX_NAME_LENGTH] = '\0';
			if( strcmp(dir.filename, filename) == 0){
				inodeN = dir.inode;
				break;
			}
		}
	}
	if(inodeN == 0){
			printf("\tCannot find the internal file\n");
			return -2;
	}
	//opening external file
	int fp = open(extFilePath,O_RDWR|O_CREAT,0700);
	if(!fp){
		printf("\tCannot access the external file\n");
		return 1;
	}	
	i = cpout(inodeN, fp);
	switch (i){
		case 0: printf("\tFile copied successfully\n"); break;
		default: printf("\tSomething went wrong, we should not be here\n"); break;
	}
	return i;
}

int canDeleteWorkInode(){
	if (!(inode.flags&dir_flag)) //if not a directory, it can be deleted
		return 1;
	int i,j;
	//if here, then it is a directory
	//iterate thought its content, if we find an entry with filename not . and not .., it is non-empty
	for(i = 0; i < ADDR_SIZE && inode.addr[i]!=0; ++i){
		lseek(fileDescriptor, inode.addr[i]*BLOCK_SIZE, SEEK_SET);
		read(fileDescriptor, buffer, BLOCK_SIZE);
		for( j = 0; j < BLOCK_SIZE; j+=16 ){
			dir = *(dir_type*) (buffer+j);
			dir.filename[MAX_NAME_LENGTH] = '\0';
			if(dir.inode != 0 
					&& strcmp(dir.filename, ".") != 0 
						&& strcmp(dir.filename, "..") != 0){ //checking if file exists
				return 0; // it is nonempty
			}
		}
	}
	return 1; //we iterated through all,found nothing, therefore empty and can be deleted
}

/*
 *	deletes the file after parsing the commands 
 */
int rmInit(){
	unsigned char *path;
	
	path = strtok(NULL, " ");
	if(!path){
		printf("\tNot all arguments have been entered\n");
		return -1;
	}
	
	unsigned char *filename = getFilenameAndPath(&path);
	if(!filename){
		printf("\tFile name is limited to %d characters\n",MAX_NAME_LENGTH);
	}
		
	//finding inode of internal file
	unsigned short parentInodeN = inodeNOf(path);
	if(parentInodeN == 0){
		printf("\tInvalid path\n");
		return -2;
	}
	int inodeN = 0, i, j;
	for(i = 0; inodeN == 0 && i < ADDR_SIZE && workInode.addr[i]!=0; ++i){
		lseek(fileDescriptor, workInode.addr[i]*BLOCK_SIZE, SEEK_SET);
		read(fileDescriptor, buffer, BLOCK_SIZE);
		for( j = 0; j < BLOCK_SIZE; j+=16 ){
			dir = *(dir_type*) (buffer+j);
			dir.filename[MAX_NAME_LENGTH] = '\0';
			if( strcmp(dir.filename, filename) == 0){
				inodeN = dir.inode;
				
				if(inodeN == 1){
					printf("\tHow dare you try to delete the root directory?\n");
					printf("\tObviously, this cannot be done\n");
					return -1;
				}
				
				//getting the inode of the erased file, we need to deallocate it;
				lseek(fileDescriptor, addressOfInode(inodeN), SEEK_SET);
				read(fileDescriptor, &inode, INODE_SIZE);
				if( canDeleteWorkInode() != 1){
					printf("\tFor this version, rm only removes files and empty directories\n");
					return -4;
				}
				
				if(inodeN == curInodeN){
					printf("\Deleting the current directory\n");
					printf("\tWorking directory defaulted to root\n");
					curInode = rootInode;
					curInodeN = 1;
				}
				
				
				
				//erasing from parent
				dir.inode = 0;
				dir.filename[0] = '\0';
				lseek(fileDescriptor, workInode.addr[i]*BLOCK_SIZE + j, SEEK_SET);
				write(fileDescriptor, &dir, 16);
				break;
				
				inode.flags = 0; //just set all to 0, this deallocates it
				//freeing the blocks in use by the file
				unsigned int buffer1[BLOCK_SIZE/4];
				for(i = 0; i < ADDR_SIZE && inode.addr[i]!=0; ++i){
					add_block_to_free_list(inode.addr[i],buffer1);
				}
				
				//saving inode
				lseek(fileDescriptor, addressOfInode(inodeN), SEEK_SET);
				write(fileDescriptor, &inode, INODE_SIZE);
			}
		}
	}
	if(inodeN==0){
		printf("File not found\n");
		return -3;
	}
	
	printf("\tFile deleted successfuly\n");
	return 0;
}

/*
 * cpout - copies internal file in inode number inodeN into external file pointed by fp
 * */
int cpout(int inodeN, int fp){
	//getting the inode
	int address = addressOfInode(inodeN);
	lseek(fileDescriptor, address, SEEK_SET);
	read(fileDescriptor, &inode, INODE_SIZE);	
		
	//read and copy
	int i, bytesRead;
	for(i = 0; i < ADDR_SIZE && inode.addr[i]!=0; ++i){
		lseek(fileDescriptor, inode.addr[i]*BLOCK_SIZE, SEEK_SET);
		bytesRead = read(fileDescriptor, buffer, BLOCK_SIZE);
		if(i == ADDR_SIZE - 1 || inode.addr[i+1]==0){
			write(fp, buffer, inode.size%BLOCK_SIZE);
		}
		else{
			write(fp, buffer, BLOCK_SIZE);
		}	
	}	
	return 0;
}

/*
 * loads superblock info from file fileDescriptor into variable superblcck
 * */
void retrieveSuperblock(){
	lseek(fileDescriptor,BLOCK_SIZE,SEEK_SET);
	unsigned int bytesRead = read(fileDescriptor, &superBlock, BLOCK_SIZE);	
	if(bytesRead!=BLOCK_SIZE)
		printf("Something went wrong");
}

/*
 * loads root inode info from the file fileDescriptor into variable rootInode
 * */
void retrieveRootInode(){
	lseek(fileDescriptor, 2*BLOCK_SIZE, SEEK_SET);
	unsigned int bytesRead = read(fileDescriptor, &rootInode, INODE_SIZE);	
	if(bytesRead!=INODE_SIZE)
		printf("Something went wrong");
	//current working directory is root
	memcpy(&curInode, &rootInode, INODE_SIZE);
	curInodeN = 1;
}

/*
 * returns the starting address of the inode given by inodeN
 * */
int addressOfInode(unsigned short inodeN){
	unsigned short inodes_per_block= BLOCK_SIZE/INODE_SIZE;
	--inodeN; //easier to use 0-based index
	return (2 + inodeN/inodes_per_block)*BLOCK_SIZE + (inodeN%inodes_per_block)*INODE_SIZE; 
}

/*
 * returns the number of a free inode, 0 if all are full
 * */
unsigned short findFreeInode(){
	if(superBlock.ninode<=0){
		int i, j;
		lseek(fileDescriptor, 2*BLOCK_SIZE, SEEK_SET);
		for(i = 0; i < superBlock.isize && superBlock.ninode < I_SIZE; i++){
			read(fileDescriptor, buffer, BLOCK_SIZE);
			for( j = 0; j < BLOCK_SIZE && superBlock.ninode < I_SIZE; j+=INODE_SIZE){
				inode = *(inode_type*) (buffer+j);
				if( !(inode.flags & inode_alloc_flag)){
					//printf("\t\tInode %d is free\n", 1+i*BLOCK_SIZE/INODE_SIZE + j/INODE_SIZE);
					superBlock.inode[superBlock.ninode++] = 1+i*BLOCK_SIZE/INODE_SIZE + j/INODE_SIZE;
				}
			}
		}
	}
	
	if(superBlock.ninode>0){
		return superBlock.inode[--superBlock.ninode];
	}
	return 0; // no free inode found
}

/*
 * returns the number of a free data block, 0 if all are full
 * */
unsigned int findFreeDataBlock(){
	if(superBlock.nfree<=1){	
		//here nfree = 1
		//go to block #free[0] and get a new free array
		lseek(fileDescriptor, superBlock.free[0]*BLOCK_SIZE, SEEK_SET);
		read(fileDescriptor, buffer, BLOCK_SIZE);
	
		//first 2 bytes are the new nfree
		superBlock.nfree = *(unsigned short*) (buffer+0);
		if(superBlock.nfree<=1){//we ran out of blocks
			printf("Wtf, I am about to return a 0 data block\n");
			return 0; //no free Data Block
		}
		//now get the next nfree ints and put them in the array
		memcpy(superBlock.free, buffer+2, superBlock.nfree*4);
	}
	return superBlock.free[--superBlock.nfree];

}

/*
 * lists the files under the curInode
 * */
void listFiles(){
	int i, j = 0;
	for(i = 0; i < ADDR_SIZE && curInode.addr[i]!=0; ++i){
		lseek(fileDescriptor, curInode.addr[i]*BLOCK_SIZE, SEEK_SET);
		read(fileDescriptor, buffer, BLOCK_SIZE);
		for( j = 0; j < BLOCK_SIZE; j+=16 ){
			dir = *(dir_type*) (buffer+j);
			dir.filename[MAX_NAME_LENGTH] = '\0';
			if(dir.inode!=0)
				printf("\t%d\t%s\n", dir.inode, dir.filename);
		}
	}
}


