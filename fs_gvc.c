/***********************************************************************
 #Guillermo Vazquez
 # NET-ID GSV200000

 Commands: 
	 1. initfs filename nblocks inodes: Initilizes/retrieves the file system and redesigning the Unix file system to accept large 
			files of up tp 4GB, expands the free array to 152 elements, expands the i-node array to 
			200 elemnts, doubles the i-node size to 64 bytes and other new features as well.
	 2. cpin extFile vFile: Copies external file to internal file system
	 3. cpout vFile extFile: Copies internal file to external file
	 4. q: save all work and exit the program.

	 ***some extra functions not required for submission, but used for debugging and I decided to keep them there
	5. superblock: displays superblock info
	6. root: displays root inode info
	7. inode n: displays the info under inode number n 
	8. ls: list files under root directory
		 
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

inode_type inode, rootInode;

typedef struct {
	unsigned short inode;
	unsigned char filename[14];
} dir_type;

dir_type root, dir;

int fileDescriptor ;		//file descriptor 
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
int addFileToRoot(unsigned short, unsigned char*);
void listFilesInRoot();
int cpinInit();
int cpin(int, char*);
int cpoutInit();
int cpout(int, int);
unsigned short findFreeInode();
int addressOfInode(unsigned short);
unsigned int findFreeDataBlock();
void printRootInode();
void printSuperblock();
void printInode(int);
void printBlock(int);
unsigned short inodeNOf(unsigned char* filename);


int main() {
 
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
				printf("File system has to be initialized first\n");
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
		
			else if(strcmp(splitter, "ls") == 0){
				listFilesInRoot();
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
			else if(strcmp(splitter, "inode") == 0){
				printInode(atoi(strtok(NULL, " ")));
				splitter = NULL;
			}     
		
			else if(strcmp(splitter, "block") == 0){
				printBlock(atoi(strtok(NULL, " ")));
				splitter = NULL;
			}  
		
		}	 
	}
}

int preInitialization(){

	char *n1, *n2;
	unsigned int numBlocks = 0, numInodes = 0;
	char *filepath;
	
	filepath = strtok(NULL, " ");
	n1 = strtok(NULL, " ");
	n2 = strtok(NULL, " ");
				 
	if(access(filepath, F_OK) != -1) {
		if(fileDescriptor = open(filepath, O_RDWR, 0600) == -1){
			printf("\tFilesystem already exists but open() failed with error [%s]\n", strerror(errno));
			return 1;
		}
		printf("\tFilesystem already exists and the same will be used.\n");
		fileDescriptor = open(filepath, O_RDWR);
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
	 superBlock.ninode = I_SIZE;
	 
	 for (i = 0; i < I_SIZE; i++)
		superBlock.inode[i] = i + 1;		//Initializing the inode array to inode numbers
	 
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

// Add Data blocks to free list
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

// Create root directory
void create_root() {

	int i, root_data_block = 2 + superBlock.isize; // Allocating first data block to root directory
	rootInode.flags = inode_alloc_flag | dir_flag | dir_large_file | dir_access_rights;   		// flag for root directory 
	rootInode.nlinks = 0; 
	rootInode.uid = 0;
	rootInode.gid = 0;
	rootInode.size = INODE_SIZE;
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
 
}

void printRootInode(){
	printf("\tFlags: %X\n", rootInode.flags);
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


void printSuperblock(){
	printf("\tisize: %d\n", superBlock.isize);
	printf("\tfsize: %d\n", superBlock.fsize);
	printf("\tnfree: %d\n", superBlock.nfree);
	printf("\tninode: %d\n", superBlock.ninode);
	printf("\tnext free block at %d\n", superBlock.free[superBlock.nfree-1]);	
}

void printInode(int inodeN){
	if(inodeN < 1 || inodeN>superBlock.isize*BLOCK_SIZE/INODE_SIZE){
		printf("\tInvalid inode number\n");
		return;
	}
	int address = addressOfInode(inodeN);
	lseek(fileDescriptor, address, SEEK_SET);
	read(fileDescriptor, &inode, INODE_SIZE);
	printf("\tFlags: %X\n", inode.flags);
	printf("\tnlinks: %d\n", inode.nlinks);
	printf("\tuid: %d\n", inode.uid);
	printf("\tgid: %d\n", rootInode.uid);
	printf("\tsize: %d bytes\n", inode.size);
	printf("\tData blocks: ");
	int i;
	for(i = 0; i < ADDR_SIZE && inode.addr[i]!=0; ++i)
		printf("%d ", inode.addr[i]);
	printf("\n");
}

void printBlock(int blockN){
	lseek(fileDescriptor, blockN*BLOCK_SIZE, SEEK_SET);
	read(fileDescriptor, buffer, BLOCK_SIZE);
	printf("%s\n", buffer);
}

int cpinInit(){
	unsigned char *extFilePath, *vFilePath;
	extFilePath = strtok(NULL, " ");
	vFilePath = strtok(NULL, " ");
	if( !extFilePath || !vFilePath ){
		printf("\tNot all arguments have been entered\n");
		return 1;
	}
	
	if( *vFilePath == '/'){ //ignore the slash, we only work in root dir so absolute = /+relative
		vFilePath++;
	}
	
	
	if( inodeNOf(vFilePath) != 0){
		printf("\tA file with that name exists already\n");
		return 3;
	}
			
	if(access(extFilePath, F_OK) != -1) {
		int fp = open(extFilePath, O_RDWR);
		if(!fp){
			printf("\tCannot open the input file\n");
			return 4;
		}
		//if we are here, the external file exists and we opened it
		int i = cpin(fp, vFilePath);
		switch (i){
				case 0: printf("\tFile copied successfully\n"); break;
				case -1: printf("\tSystem ran out of free blocks\n"); break;
				case -2: printf("\tFile too big, got truncated\n"); break;
				case -3: printf("\tRoot directory is full\n"); break;
				default: printf("\tSomething went wrong, we should not be here\n"); break;
		}
		return i; 
	}
	
	else {
		printf("\tThe external file does not exist\n");
		return 5;
	}
	
		
}

int cpin(int fp, char* vFilePath){
		//grabbing an inode or the file
		unsigned short inodeN = findFreeInode();
		int address = addressOfInode(inodeN);
		lseek(fileDescriptor, address, SEEK_SET);
		read(fileDescriptor, &inode, INODE_SIZE);	
		inode.flags = inode_alloc_flag | dir_access_rights;
		inode.nlinks = 1;
		inode.gid = 0;
		inode.uid = 0;
		inode.addr[0] = 0;
		
		//adding file to root 
		if( addFileToRoot(inodeN, vFilePath) != 0){
			return -3;
		}
		//read external and copy to internal block by block
		int i = -1,bytesRead = 0;
		while((bytesRead = read(fp, buffer, BLOCK_SIZE)) > 0 && ++i < ADDR_SIZE){
			inode.addr[i] = findFreeDataBlock();
			if(inode.addr[i] == 0){
				printf("\tNo more free blocks available");
				return -1;
			}
			inode.size += bytesRead;
			lseek(fileDescriptor, inode.addr[i]*BLOCK_SIZE, SEEK_SET);
			write(fileDescriptor, buffer, bytesRead);
		}
		
		if(i==ADDR_SIZE && bytesRead > 0){
			printf("\tThis version does not handle big files. The file got truncated\n");
			return -2;
		}
		if(i+1 < ADDR_SIZE){
			inode.addr[i+1] = 0;
		}
		//saving inode
		lseek(fileDescriptor, address, SEEK_SET);
		write(fileDescriptor, &inode, INODE_SIZE);
		
		
		//save root inode
		lseek(fileDescriptor, 2 * BLOCK_SIZE, 0);
		write(fileDescriptor, &rootInode, INODE_SIZE);
		
		return 0; 
}

int cpoutInit(){
	unsigned char *extFilePath, *vFilePath;
	
	vFilePath = strtok(NULL, " ");
	extFilePath = strtok(NULL, " ");
	
	if( !extFilePath || !vFilePath ){
		printf("\tNot all arguments have been entered\n");
		return -1;
	}
	
	if(*vFilePath == '/'){
		vFilePath++;
	}
	
	//finding inode of internal file
	unsigned short inodeN = inodeNOf(vFilePath);
	if(inodeN == 0){
		printf("\tCannot find the internal file");
		return -1;
	}
			
	//opening external file
	int fp = open(extFilePath,O_RDWR|O_CREAT,0700);
	if(!fp){
		printf("\tCannot access the external file\n");
		return 1;
	}	
	int i = cpout(inodeN, fp);
	switch (i){
		case 0: printf("\tFile copied successfully\n"); break;
		default: printf("\tSomething went wrong, we should not be here\n"); break;
	}
	return i;
}

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
		write(fp, buffer, bytesRead);
	}	
	return 0;
}

void retrieveSuperblock(){
	lseek(fileDescriptor,BLOCK_SIZE,SEEK_SET);
	unsigned int bytesRead = read(fileDescriptor, &superBlock, BLOCK_SIZE);	
	if(bytesRead!=BLOCK_SIZE)
		printf("Something went wrong");
}

void retrieveRootInode(){
	lseek(fileDescriptor, 2*BLOCK_SIZE, SEEK_SET);
	unsigned int bytesRead = read(fileDescriptor, &rootInode, INODE_SIZE);	
	if(bytesRead!=INODE_SIZE)
		printf("Something went wrong");
}


int addressOfInode(unsigned short inodeN){
	unsigned short inodes_per_block= BLOCK_SIZE/INODE_SIZE;
	--inodeN; //easier to use 0-based index
	return (2 + inodeN/inodes_per_block)*BLOCK_SIZE + (inodeN%inodes_per_block)*INODE_SIZE; 
}

unsigned short findFreeInode(){
	if(superBlock.ninode<=0){
		int i, j;
		lseek(fileDescriptor, 2*BLOCK_SIZE, SEEK_SET);
		for(i = 0; i < superBlock.isize && superBlock.ninode < I_SIZE; i++){
			read(fileDescriptor, buffer, BLOCK_SIZE);
			for( j = 0; j < BLOCK_SIZE && superBlock.ninode < I_SIZE; j+=INODE_SIZE){
				inode = *(inode_type*) (buffer+j);
				if( !(inode.flags & inode_alloc_flag)){
					printf("\t\tInode %d is free\n", 1+i*BLOCK_SIZE/INODE_SIZE + j/INODE_SIZE);
					superBlock.inode[superBlock.ninode++] = 1+i*BLOCK_SIZE/INODE_SIZE + j/INODE_SIZE;
				}
			}
		}
	}
	
	if(superBlock.ninode>0){
		return superBlock.inode[--superBlock.ninode];
	}
	printf("Wtf, I am about to return a 0 inode\n");
	return 0; // no free inode found
}

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

void listFilesInRoot(){
	int i, j = 0;
	for(i = 0; i < ADDR_SIZE && rootInode.addr[i]!=0; ++i){
		lseek(fileDescriptor, rootInode.addr[i]*BLOCK_SIZE, SEEK_SET);
		read(fileDescriptor, buffer, BLOCK_SIZE);
		for( j = 0; j < BLOCK_SIZE; j+=16 ){
			dir = *(dir_type*) (buffer+j);
			if(dir.inode==0)
				break;
			printf("\t%d\t%s\n", dir.inode, dir.filename);
		}
	}
}

int addFileToRoot(unsigned short inodeN, unsigned char* filename){
	int i = 0;
	
	//we need to find a non-full data block
	// if addr[i+1] !=0, then addr[i] is full
	while(i < ADDR_SIZE-1 &&rootInode.addr[i+1]!=0){
		i++;
	}
	
	lseek(fileDescriptor, rootInode.addr[i]*BLOCK_SIZE, SEEK_SET);
	read(fileDescriptor, buffer, BLOCK_SIZE);
	//check the last inode: if it is allocated, the block is full
	dir = *(dir_type*) (buffer + (BLOCK_SIZE-16));
	if(dir.inode==0){ //there is place in this dataBlock!
		//find spot
		int j = 0;
		dir = *(dir_type*) (buffer+j);
		while(dir.inode != 0){
			j+=16;
			dir = *(dir_type*) (buffer+j);
		}
		lseek(fileDescriptor, rootInode.addr[i]*BLOCK_SIZE + j, SEEK_SET);	
	}
	
	else{ //dataBlock is full, grab new one
		rootInode.addr[++i] = findFreeDataBlock();
		if(rootInode.addr[i] == 0){
			printf("\tNo more free blocks available");
			return;
		}
		lseek(fileDescriptor, rootInode.addr[i]*BLOCK_SIZE, SEEK_SET);
	}
	dir.inode = inodeN;
	strcpy(dir.filename, filename);
	write(fileDescriptor, &dir, 16);
	rootInode.size += 16; //one 16 byte dir got added
	return 0;
	/*
	 * // the code below tries a cooler method, but does not work
	 * //why doesnt this work? notlikethis
	//check if the last used block is full
	int i = rootInode.size/BLOCK_SIZE;
	int j = rootInode.size%BLOCK_SIZE;
	if(j == 0){
		if(i==ADDR_SIZE){
			printf("\tThis version does not handle big files yet. Root directory cannot be bigger\n");
			return 1;
		}
		rootInode.addr[++i] = findFreeDataBlock();
		if(rootInode.addr[i] == 0){
			printf("\tSystem ran out of free blocks\n");
			return 2;
		}
	}
	dir.inode = inodeN;
	strcpy(dir.filename, filename);
	lseek(fileDescriptor, rootInode.addr[i]*BLOCK_SIZE + j, SEEK_SET);
	write(fileDescriptor, &dir, 16);
	rootInode.size += 16; //one 16 byte dir got added
	printf("\tJust added file in byte %d\n", rootInode.addr[i]*BLOCK_SIZE + j);
	return 0;
	*/
}

unsigned short inodeNOf(unsigned char* filename){
	int i, j;
	for(i = 0; i < ADDR_SIZE && rootInode.addr[i]!=0; ++i){
		lseek(fileDescriptor, rootInode.addr[i]*BLOCK_SIZE, SEEK_SET);
		read(fileDescriptor, buffer, BLOCK_SIZE);
		for( j = 0; j < BLOCK_SIZE; j+=16 ){
			dir = *(dir_type*) (buffer+j);
			if( strcmp(dir.filename, filename) == 0)
				return dir.inode;
		}
	}
	return 0;
}
