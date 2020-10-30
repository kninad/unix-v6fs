/***********************************************************************
* Author: Ninad Khargonkar
* NetID: NXK180069
* Project - 2: Implementation of V6 File System
* CS 5348.001 Operating Systems: Fall 2020
* Prof. S Venkatesan
* **********************************************************************
* Compilation:- $ gcc fsaccess_nak.c -std=gnu99 -o v6fs
* Run using  :- $ ./v6fs
* You will get a prompt where you can perform the following actions:
* **********************************************************************
* This program allows user to do two things:
    1. initfs: Initilizes the file system and redesigning the Unix file system to accept large
        files of up tp 4GB, expands the free array to 152 elements, expands the i-node array to
        200 elemnts, doubles the i-node size to 64 bytes and other new features as well.
    2. Quit: save all work and exit the program.

* User Input:
    - initfs (file path) (# of total system blocks) (# of System i-nodes)
    - q
    - cpin (external file path) (v6 file)
    - cpout (v6 file) (external file path)

* File name is limited to 14 characters.
***********************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FREE_SIZE 152
#define I_SIZE 200
#define BLOCK_SIZE 1024
#define ADDR_SIZE 11
#define INPUT_SIZE 256

// Superblock Structure
typedef struct {
    char flock;
    char ilock;
    char fmod;
    unsigned short isize;  // num blocks used for the inodes
    unsigned short fsize;  // total number of blocks in the fs
    unsigned short nfree;
    unsigned short time[2];
    unsigned short ninode;
    unsigned short inode[I_SIZE];
    unsigned int free[FREE_SIZE];

} superblock_type;

// I-Node Structure
typedef struct {
    unsigned short flags;
    unsigned short nlinks;
    unsigned short uid;
    unsigned short gid;
    unsigned int size;
    unsigned int addr[ADDR_SIZE];
    unsigned short actime[2];
    unsigned short modtime[2];
} inode_type;

// Directory Structure
typedef struct {
    unsigned short inode;
    char filename[14];
} dir_type;


// Since only 1 super block for the fs, its okay to initialize it here. Used in a lot
// donwstream functions which assume its a global variable.
superblock_type superBlock;
int fileDescriptor;  //file descriptor for the v6 fs, global variable!
const unsigned short inode_alloc_flag = 0100000;
const unsigned short dir_flag = 040000;
const unsigned short dir_large_file = 010000;
const unsigned short dir_access_rights = 000777;  // User, Group, & World have all access privileges
const unsigned short INODE_SIZE = 64;             // inode has been doubled
unsigned int num_blocks, num_inodes;              // globally stored!

bool DEBUG_FLAG = false; // whether to enable print based debugging?

// Function Prototypes
int preInitialization();
int initfs(char *path, unsigned short total_blcks, unsigned short total_inodes);
void add_block_to_free_list(int blocknumber, unsigned int *empty_buffer);
void create_root();

void print_superblock();
void print_char_buffer(char buffer[], int size);
inode_type read_inode_from_num(unsigned short inode_num);
void write_inode_num(unsigned short inode_num, inode_type inode);
unsigned short get_free_inode_num();
unsigned int get_free_block();
bool filename_in_direntry(char *file_name, dir_type dir);
bool check_flag_dir(unsigned short flags);

int copy_in(char *external_file, char *v6_file);
int copy_out(char *v6_file, char *external_file);


int main(int argc, char *argv) {
    if (argc < 2) {
        printf("Debug Flag parameter not supplied.\n");
        printf("Hence print based Debugging mode OFF by default\n");
    } else {
        // Set the flag to true if (any) one argument is passed -- for simplicity not 
        // checking whether zero or one.
        printf("Since (any) one argument passed, debugging mode is set to: ON\n");
        DEBUG_FLAG = true;
    }    
    printf("Debugging Flag is set to: %d\n\n", DEBUG_FLAG);

    char input[INPUT_SIZE];
    char *splitter;
    unsigned int numBlocks = 0, numInodes = 0;
    char *filepath;
    printf("*********************************\n");
    printf("* Unix V6 File System Simulation\n");
    printf("* Size of super block = %ld , size of i-node = %ld\n", sizeof(superBlock), sizeof(inode_type));
    printf("* Available commands are: 1) initfs 2) cpin 3) cpout 4) q\n");
    printf("* The first command on startup always has to be initfs.\n");
    printf("\nEnter the required command:\n");

    while (1) {
        printf("ninad@V6FS:$ ");
        scanf(" %[^\n]s", input);
        // breaks down input string using the provided delimiter (here: " ")
        splitter = strtok(input, " ");

        if (strcmp(splitter, "initfs") == 0) {
            preInitialization();
            splitter = NULL;  // why? safety?
        } else if (strcmp(splitter, "cpin") == 0) {
            char *arg1 = strtok(NULL, " ");
            char *arg2 = strtok(NULL, " ");
            if (!arg1 || !arg2) {
                printf("Arguments (external-file and v6-file) have not been entered?\n\n");
            } else {
                int ret = copy_in(arg1, arg2);
                if (ret <= 0) {
                    printf("Copy in operation failed!\n\n");
                } else {
                    printf("Copy in operation was successful.\n\n");
                }
            }
        } else if (strcmp(splitter, "cpout") == 0) {
            char *arg1 = strtok(NULL, " ");
            char *arg2 = strtok(NULL, " ");
            if (!arg1 || !arg2) {
                printf("Arguments (external-file and v6-file) have not been entered?\n\n");
            } else {
                int ret = copy_out(arg1, arg2);
                if (ret <= 0) {
                    printf("Copy out operation failed! \n\n");
                } else {
                    printf("Copy out operation was successful\n\n");
                }
            }
        } 
    } // end while loop
} // end main


// Function Definitions begin here
// Pre-init code -- incase file system already exists: set vals for appropriate variables.
int preInitialization() {
    char *n1, *n2;
    unsigned int numBlocks = 0, numInodes = 0;
    char *filepath;

    filepath = strtok(NULL, " ");
    n1 = strtok(NULL, " ");
    n2 = strtok(NULL, " ");

    if (access(filepath, F_OK) != -1) {
        if ((fileDescriptor = open(filepath, O_RDWR, 0600)) == -1) {
            printf("Filesystem already exists but open() failed with error [%s]\n\n", strerror(errno));
            return 1;
        }
        // Still need to set the global variables! Otherwise these are never set!
        // Kind of Terrible hack, I know!
        numBlocks = atoi(n1);
        numInodes = atoi(n2);
        num_blocks = numBlocks;
        num_inodes = numInodes;
        printf("Filesystem already exists and the same will be used.\n\n");
        // But we still need to init the superblock from the disk
        lseek(fileDescriptor, BLOCK_SIZE, SEEK_SET);
        read(fileDescriptor, &superBlock, sizeof(superBlock));        
        if(DEBUG_FLAG){  //check if superblock is loaded correctly!
            print_superblock();
        }
    } else {
        if (!n1 || !n2)
            printf("All arguments(path, number of inodes and total number of blocks) have not been entered\n\n");
        else {
            numBlocks = atoi(n1);
            numInodes = atoi(n2);
            if (initfs(filepath, numBlocks, numInodes)) {
                printf("The file system is initialized\n\n");
            } else {
                printf("Error initializing file system. Exiting...\n\n");
                return 1;
            }
        }
    }
}


int initfs(char *path, unsigned short blocks, unsigned short inodes) {    
    unsigned int buffer[BLOCK_SIZE / 4];
    int bytes_written;

    // set the global variables for this run of initfs
    num_blocks = blocks;
    num_inodes = inodes;

    // unsigned short i = 0;
    superBlock.fsize = blocks;
    unsigned short inodes_per_block = BLOCK_SIZE / INODE_SIZE;

    if ((inodes % inodes_per_block) == 0)
        superBlock.isize = inodes / inodes_per_block;
    else
        superBlock.isize = (inodes / inodes_per_block) + 1;

    if ((fileDescriptor = open(path, O_RDWR | O_CREAT, 0700)) == -1) {
        printf("\nopen() failed with the following error [%s]\n", strerror(errno));
        return 0;
    }

    for (int i = 0; i < FREE_SIZE; i++)
        superBlock.free[i] = 0;  //initializing free array to 0 to remove junk data. free array will be stored with data block numbers shortly.

    superBlock.nfree = 0;
    superBlock.ninode = I_SIZE;

    // inode num = 1 is reserved for root.
    for (int i = 0; i < I_SIZE; i++)
        superBlock.inode[i] = i + 1;  //Initializing the inode array to inode numbers

    superBlock.flock = 'a';  //flock,ilock and fmode are not used.
    superBlock.ilock = 'b';
    superBlock.fmod = 'c';  // fmod is also of char type
    superBlock.time[0] = 0;
    superBlock.time[1] = 1970;

    lseek(fileDescriptor, 1 * BLOCK_SIZE, SEEK_SET);  // since first block!
    write(fileDescriptor, &superBlock, BLOCK_SIZE);   // writing superblock to file system

    // writing zeroes to all inodes in ilist
    for (int i = 0; i < BLOCK_SIZE / 4; i++)
        buffer[i] = 0;

    for (int i = 0; i < superBlock.isize; i++)
        write(fileDescriptor, buffer, BLOCK_SIZE);

    int data_blocks = blocks - 2 - superBlock.isize;
    int data_blocks_for_free_list = data_blocks - 1;

    // Create root directory
    create_root();
    // blocks are zero-addressed!
    for (int i = 2 + superBlock.isize + 1; i < blocks; i++) {
        add_block_to_free_list(i, buffer);
    }
    return 1;
}

// Add Data blocks to free list
void add_block_to_free_list(int block_number, unsigned int *empty_buffer) {
    if (superBlock.nfree == FREE_SIZE) {
        unsigned int free_list_data[BLOCK_SIZE / 4];
        free_list_data[0] = FREE_SIZE;  // curr value of nfree

        // Copy the contents (nfre, free array) to free_list_data
        for (int i = 0; i < BLOCK_SIZE / 4; i++) {
            if (i < FREE_SIZE) {
                free_list_data[i + 1] = superBlock.free[i];
            } else {
                free_list_data[i + 1] = 0;  // getting rid of junk data in the remaining unused bytes of header block
            }
        }
        // Copy nfree and free array (via free_list_data) to the given block_number
        lseek(fileDescriptor, block_number * BLOCK_SIZE, SEEK_SET);
        write(fileDescriptor, free_list_data, BLOCK_SIZE);  // Writing free list to header block
        superBlock.nfree = 0;
    } else {
        lseek(fileDescriptor, block_number * BLOCK_SIZE, SEEK_SET);
        write(fileDescriptor, empty_buffer, BLOCK_SIZE);  // writing 0 to remaining data blocks to get rid of junk data
    }
    superBlock.free[superBlock.nfree] = block_number;  // Assigning blocks to free array
    ++superBlock.nfree;
}


// Create root directory
void create_root() {
    dir_type root;                               // data block for the root directory
    inode_type inode;                            // inode for the root
    int root_data_block = 2 + superBlock.isize;  // Allocating 1st data block to root dir
    int i;

    root.inode = 1;  // root directory's inode number is 1.
    root.filename[0] = '.';
    root.filename[1] = '\0';

    inode.flags = inode_alloc_flag | dir_flag | dir_large_file | dir_access_rights;  // flag for root directory
    inode.nlinks = 0;
    inode.uid = 0;
    inode.gid = 0;
    inode.size = INODE_SIZE;
    inode.addr[0] = root_data_block;
    // Set rest of addresses in the inode to zero.
    for (i = 1; i < ADDR_SIZE; i++) {
        inode.addr[i] = 0;
    }

    inode.actime[0] = 0;
    inode.modtime[0] = 0;
    inode.modtime[1] = 0;

    // 2 * BLK_SIZE since the inode for root is the next block after the super block
    // technically the second block overall in the filesystem
    lseek(fileDescriptor, 2 * BLOCK_SIZE, SEEK_SET);
    write(fileDescriptor, &inode, INODE_SIZE);  //
    
    lseek(fileDescriptor, root_data_block * BLOCK_SIZE, SEEK_SET);  // this is correct?    
    write(fileDescriptor, &root, sizeof(dir_type));

    // Write out the entry for ".."
    root.filename[0] = '.';
    root.filename[1] = '.';
    root.filename[2] = '\0';
    write(fileDescriptor, &root, sizeof(dir_type));
}


// Print some of the superblock params: isize, fsize, nfree, ninode. Used for debugging.
void print_superblock() {
    superblock_type sb;
    lseek(fileDescriptor, 1 * BLOCK_SIZE, SEEK_SET);
    read(fileDescriptor, &sb, sizeof(superBlock));
    printf("\nPrinting Superblock params:\n");
    printf("sb read from disk: isize, fsize, nfree, ninode: %d, %d, %d, %d\n", sb.isize, sb.fsize, sb.nfree, sb.ninode);
    printf("sB global var: isize, fsize, nfree, ninode: %d, %d, %d, %d\n", superBlock.isize, superBlock.fsize, superBlock.nfree, superBlock.ninode);
}


// Print out the buffer array for a block: Used for debugging.
void print_char_buffer(char buffer[], int size) {
    printf("\n\tPrinting buffer!\n");
    for (int i = 0; i < size; ++i) {
        printf("%c ", buffer[i]);
    }
    printf("\n");
}


// Get the inode_type struct corresponding to the given inode number
inode_type read_inode_from_num(unsigned short inode_num) {
    unsigned int inodes_per_block = BLOCK_SIZE / INODE_SIZE;
    unsigned int block_num = (inode_num / inodes_per_block) + 2;
    unsigned int offset = (inode_num % inodes_per_block) - 1;  // after how many inodes
    // eg. if inum = 1, no offset! first inode on the block
    inode_type tmp_inode;
    lseek(fileDescriptor, block_num * BLOCK_SIZE + offset * INODE_SIZE, SEEK_SET);
    read(fileDescriptor, &tmp_inode, INODE_SIZE);
    return tmp_inode;
}

// Write given inode struct into the inode table i.e as a inode with number = inode_num
void write_inode_num(unsigned short inode_num, inode_type inode) {
    unsigned int inodes_per_block = BLOCK_SIZE / INODE_SIZE;
    unsigned int block_num = (inode_num / inodes_per_block) + 2;
    unsigned int offset = (inode_num % inodes_per_block) - 1;  // after how many inodes
    lseek(fileDescriptor, block_num * BLOCK_SIZE + offset * INODE_SIZE, SEEK_SET);
    write(fileDescriptor, &inode, INODE_SIZE);
}

// Get a free inode number
unsigned short get_free_inode_num() {
    if(DEBUG_FLAG){
        printf(" superblock.ninode: %d\n", superBlock.ninode);
    }
    if (superBlock.ninode == 0) {
        // Iterate through all the inodes and add free ones to the inode[] array.
        // Start from i = 2 since inode_num = 1 is for root.
        for (int i = 2; i <= num_inodes && superBlock.ninode <= I_SIZE; ++i) {
            inode_type tmp_inode = read_inode_from_num(i);
            if(DEBUG_FLAG){
                printf(" found an inode: %d ; ", i);
            }
            // flags >> 15 is the M.S.B and taking its AND with 1: free or not?
            int bits_flags = sizeof(short) * 8; // number of bits for flags type
            int mask = 1 << (bits_flags - 1); // bitwise: 1 followed by 15 zeros.
            if ((tmp_inode.flags & mask) == 0) {  // Free inode!
                if(DEBUG_FLAG){
                    printf(" found a free inode: %d\n", i);
                }
                superBlock.inode[superBlock.ninode] = i;
                ++superBlock.ninode;
            }
        }
    }
    // Now, sb.ninode > 0. Can safely do the following.
    --superBlock.ninode;
    return superBlock.inode[superBlock.ninode];
}


// Get a free block number.
unsigned int get_free_block() {
    --superBlock.nfree;
    if (superBlock.nfree == 0) {
        unsigned int head_block = superBlock.free[0];
        lseek(fileDescriptor, head_block * BLOCK_SIZE, SEEK_SET);
        read(fileDescriptor, superBlock.free, sizeof(superBlock.free));
        superBlock.nfree = FREE_SIZE;
        return head_block;
    }
    return superBlock.free[superBlock.nfree];
}


// Check whether file_name is the file associated with given  dir_entry dir
bool filename_in_direntry(char *file_name, dir_type dir) {
    return (strcmp(file_name, dir.filename) == 0);
}


// Check whether the flags satisfy those for: allocated and a directory
bool check_flag_dir(unsigned short flags) {
    // should be allocated and a directory!
    return (((flags & dir_flag) != 0) && ((flags & inode_alloc_flag) != 0));
}


// Copy in external file into a v6 file
int copy_in(char *external_file, char *v6_filename) {
    printf("This will over-write any existing v6 file!\n");
    int extf_descriptor;
    if ((extf_descriptor = open(external_file, O_RDONLY, 0700)) == -1) {
        printf("\n open() failed with the following error [%s]\n", strerror(errno));
        return -1;
    }
    if (DEBUG_FLAG) {
        printf("Opened the external file.\n");
    }
    
    unsigned short inode_num = get_free_inode_num();
    if (inode_num < 2){
        printf("Could not allocate inode for file. Error!\n");
        return -1;
    }
    
    inode_type inode;
    inode.flags = inode_alloc_flag | dir_access_rights;  // set flags
    inode.nlinks = 0;
    inode.uid = 0;
    inode.gid = 0;
    inode.size = 0;  // init the file size to zero. will be set later.
    inode.actime[0] = 0;
    inode.modtime[0] = 0;
    inode.modtime[1] = 0;

    if (DEBUG_FLAG) {
        printf("Allocated an I-Node for the file: %d\n", inode_num);
    }

    // Now for the data blocks!
    char buffer[BLOCK_SIZE] = {0};  // init with zero to avoid garbage value.
    int read_flag, block_num;
    unsigned int fsize = 0, idx = 0;  // idx in the addr[] array for the v6 file's inode.
    read_flag = read(extf_descriptor, buffer, BLOCK_SIZE);
    while (read_flag > 0) {  // 0 -> EOF, -1 -> Error.
        if (DEBUG_FLAG) {
            printf("params: %d, %d, %d", read_flag, idx, fsize);
            print_char_buffer(buffer, BLOCK_SIZE);
        }
        block_num = get_free_block();
        lseek(fileDescriptor, block_num * BLOCK_SIZE, SEEK_SET);
        write(fileDescriptor, buffer, BLOCK_SIZE);
        inode.addr[idx] = block_num;
        idx += 1;
        fsize += read_flag;  // increment number of bytes read
        read_flag = read(extf_descriptor, buffer, BLOCK_SIZE);
    }

    if (DEBUG_FLAG) {
        printf("Wrote the data to data-blocks for the assigned inode.\n");
    }

    inode.size = fsize;
    write_inode_num(inode_num, inode);

    if (DEBUG_FLAG) {
        printf("Wrote the i-node to the inode-table in num: %d\n", inode_num);
    }

    // Update Root Directory Data Block with v6file info (All files are stored in root)
    dir_type v6file;
    v6file.inode = inode_num;
    strcpy(v6file.filename, v6_filename);  // v6file.filename := v6_filename;
    const int ROOT_INODE = 1;
    inode_type root_inode = read_inode_from_num(ROOT_INODE);

    // Move to appropriate position in root dir's last data block and write out info
    unsigned int bidx_addr = root_inode.size / BLOCK_SIZE;
    unsigned int offset = root_inode.size % BLOCK_SIZE;
    if (offset == 0) {  // If perfectly divides, move to the next block (next idx in addr[])
        ++bidx_addr;
    }
    if (bidx_addr >= ADDR_SIZE) {
        printf("\nNo more space in Root Directory for additional files!, Error! \n");
        return -1;
    }

    unsigned int root_last_block_num = root_inode.addr[bidx_addr];  // < 11?
    lseek(fileDescriptor, root_last_block_num * BLOCK_SIZE + offset, SEEK_SET);
    write(fileDescriptor, &v6file, sizeof(dir_type));
    // update root's inode size and write out inode
    root_inode.size += sizeof(dir_type);  // since one more child addded to root!
    write_inode_num(ROOT_INODE, root_inode);
    close(extf_descriptor);
    return 1;  // success!
}


// Copy out from a v6 file into an external file
int copy_out(char *v6_file, char *external_file) {
    // Assumes external file does not exist. Otherwise will over-write.
    int extf_descriptor;
    if ((extf_descriptor = open(external_file, O_RDWR | O_CREAT | O_APPEND, 0700)) == -1) {
        printf(" open() failed with the following error [%s]\n", strerror(errno));
        return -1;
    }

    if (DEBUG_FLAG) {
        printf("Opened the external file.\n");
    }

    // Check for v6file in root dir! Read in 16 byte segments of dir_type and compare
    // given v6 filename
    unsigned int root_inode_num = 1;
    inode_type root_inode = read_inode_from_num(root_inode_num);
    if (!(check_flag_dir(root_inode.flags))) {
        printf("Root directory not allocated or not init as a directory! Error!\n");
        return -1;
    } else {
        printf("Root allocation check passed.\n");
    }
    // init to zero. If zero at end, it means file not found in root dir!
    unsigned short inode_num_v6file = 0;
    dir_type tmp_buffer;
    bool flag_found = false;

    for (int i = 0; i < ADDR_SIZE; ++i) {
        if (flag_found || root_inode.addr[i] == 0) {
            // printf("debug: %d, %d\n", flag_found, root_inode.addr[i]);
            break;
        }
        unsigned int block_num = root_inode.addr[i];
        lseek(fileDescriptor, block_num * BLOCK_SIZE, SEEK_SET);
        unsigned int read_bytes = read(fileDescriptor, &tmp_buffer, sizeof(dir_type));
        unsigned int count = read_bytes;
        while (read_bytes > 0 && count <= BLOCK_SIZE) {
            if (filename_in_direntry(v6_file, tmp_buffer)) {
                inode_num_v6file = tmp_buffer.inode;  // inode number saved!
                flag_found = true;                    // to break out of outer loop
                break;
            }
            read_bytes = read(fileDescriptor, &tmp_buffer, sizeof(dir_type));
            count += read_bytes;
            // printf("debug: %d, %d, %d, %d\n", i, block_num, read_bytes, count);
        }
    }
    if (!flag_found) {
        printf("Given V6 file not found in root directory! Error!\n");
        return -1;
    }

    // Now using the inode, write out contents to external file
    inode_type inode_v6file = read_inode_from_num(inode_num_v6file);
    lseek(extf_descriptor, 0, SEEK_SET);  // move to very beginning of external file
    char buffer[BLOCK_SIZE] = {0};
    for (int i = 0; i < ADDR_SIZE; ++i) {
        unsigned int curr_block_num = inode_v6file.addr[i];
        if (curr_block_num == 0) {  // go no further!
            break;
        }
        lseek(fileDescriptor, curr_block_num * BLOCK_SIZE, SEEK_SET);
        // print_char_buffer(buffer, BLOCK_SIZE);
        read(fileDescriptor, buffer, BLOCK_SIZE);
        write(extf_descriptor, buffer, BLOCK_SIZE);
    }
    close(extf_descriptor);
    return 1;
}


