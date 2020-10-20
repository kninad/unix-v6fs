/***********************************************************************



 This program allows user to do two things:
   1. initfs: Initilizes the file system and redesigning the Unix file system to accept large
      files of up tp 4GB, expands the free array to 152 elements, expands the i-node array to
      200 elemnts, doubles the i-node size to 64 bytes and other new features as well.
   2. Quit: save all work and exit the program.

 User Input:
     - initfs (file path) (# of total system blocks) (# of System i-nodes)
     - q

 File name is limited to 14 characters.
 ***********************************************************************/

#include <errno.h>
#include <fcntl.h>
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
    unsigned short isize;  // num blocks used for the inodes
    unsigned short fsize;  // total number of blocks in the fs
    unsigned short nfree;
    unsigned int free[FREE_SIZE];
    unsigned short ninode;
    unsigned short inode[I_SIZE];
    char flock;
    char ilock;
    unsigned short fmod;
    unsigned short time[2];
} superblock_type;

// Since only 1 super block for the fs, its okay to initialize it here. Used in a lot
// donwstream functions which assume its a global variable.
superblock_type superBlock;

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

int fileDescriptor;  //file descriptor for the v6 fs, global variable!
const unsigned short inode_alloc_flag = 0100000;
const unsigned short dir_flag = 040000;
const unsigned short dir_large_file = 010000;
const unsigned short dir_access_rights = 000777;  // User, Group, & World have all access privileges
const unsigned short INODE_SIZE = 64;             // inode has been doubled
unsigned int num_blocks, num_inodes;              // global store

// Function Prototypes
int initfs(char *path, unsigned short total_blcks, unsigned short total_inodes);
void add_block_to_free_list(int blocknumber, unsigned int *empty_buffer);
void create_root();

void copy_in(char *external_file, char *v6_file);
void copy_out(char *v6_file, char *external_file);

int main() {
    char input[INPUT_SIZE];
    char *splitter;
    unsigned int numBlocks = 0, numInodes = 0;
    char *filepath;
    // printf("Size of super block = %d , size of i-node = %d\n", sizeof(superBlock), sizeof(inode));
    printf("Size of super block = %d , size of i-node = %d\n", sizeof(superblock_type), sizeof(inode_type));
    printf("Enter command:\n");

    while (1) {
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
                printf("Arguments (external-file and v6-file) have not been entered?\n");
            } else {
                copy_in(arg1, arg2);
            }
        } else if (strcmp(splitter, "cpout") == 0) {
            char *arg1 = strtok(NULL, " ");
            char *arg2 = strtok(NULL, " ");
            if (!arg1 || !arg2) {
                printf("Arguments (external-file and v6-file) have not been entered?\n");
            } else {
                copy_out(arg1, arg2);
            }
        } else if (strcmp(splitter, "q") == 0) {
            lseek(fileDescriptor, BLOCK_SIZE, 0);
            write(fileDescriptor, &superBlock, BLOCK_SIZE);
            return 0;
        }
    }
}

int preInitialization() {
    char *n1, *n2;
    unsigned int numBlocks = 0, numInodes = 0;
    char *filepath;

    filepath = strtok(NULL, " ");
    n1 = strtok(NULL, " ");
    n2 = strtok(NULL, " ");

    if (access(filepath, F_OK) != -1) {
        if (fileDescriptor = open(filepath, O_RDWR, 0600) == -1) {
            printf("\n filesystem already exists but open() failed with error [%s]\n", strerror(errno));
            return 1;
        }
        printf("filesystem already exists and the same will be used.\n");
    } else {
        if (!n1 || !n2)
            printf(" All arguments(path, number of inodes and total number of blocks) have not been entered\n");
        else {
            numBlocks = atoi(n1);
            numInodes = atoi(n2);
            if (initfs(filepath, numBlocks, numInodes)) {
                printf("The file system is initialized\n");
            } else {
                printf("Error initializing file system. Exiting... \n");
                return 1;
            }
        }
    }
}

int initfs(char *path, unsigned short blocks, unsigned short inodes) {
    // Is it buffer[BLOCK_SIZE / 4] because sizeof(int) == 4, so we can only have that large of a buffer for a block?
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
        printf("\n open() failed with the following error [%s]\n", strerror(errno));
        return 0;
    }

    for (int i = 0; i < FREE_SIZE; i++)
        superBlock.free[i] = 0;  //initializing free array to 0 to remove junk data. free array will be stored with data block numbers shortly.

    superBlock.nfree = 0;
    superBlock.ninode = I_SIZE;

    for (int i = 0; i < I_SIZE; i++)
        superBlock.inode[i] = i + 1;  //Initializing the inode array to inode numbers

    superBlock.flock = 'a';  //flock,ilock and fmode are not used.
    superBlock.ilock = 'b';
    superBlock.fmod = 0;
    superBlock.time[0] = 0;
    superBlock.time[1] = 1970;

    lseek(fileDescriptor, BLOCK_SIZE, SEEK_SET);
    write(fileDescriptor, &superBlock, BLOCK_SIZE);  // writing superblock to file system

    // writing zeroes to all inodes in ilist
    for (int i = 0; i < BLOCK_SIZE / 4; i++)
        buffer[i] = 0;

    for (int i = 0; i < superBlock.isize; i++)
        write(fileDescriptor, buffer, BLOCK_SIZE);

    int data_blocks = blocks - 2 - superBlock.isize;
    int data_blocks_for_free_list = data_blocks - 1;

    // Create root directory
    create_root();
    // an extra +1 at the end since root's data block is already initialized
    for (int i = 2 + superBlock.isize + 1; i < data_blocks_for_free_list; i++) {
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

    // lseek(fileDescriptor, root_data_block, 0);
    lseek(fileDescriptor, root_data_block, SEEK_SET);
    // write(fileDescriptor, &root, 16);
    write(fileDescriptor, &root, sizeof(dir_type));

    root.filename[0] = '.';
    root.filename[1] = '.';
    root.filename[2] = '\0';

    // write(fileDescriptor, &root, 16); // sizeof(dir_type) = 16;
    write(fileDescriptor, &root, sizeof(dir_type));
}

// Get the inode corresponding to the inode number
inode_type read_inode_from_num(int inode_num) {
    unsigned int block_num = (inode_num - 1) * INODE_SIZE / BLOCK_SIZE + 2;
    unsigned int offset = (inode_num - 1) * INODE_SIZE % BLOCK_SIZE;
    inode_type tmp_inode;
    lseek(fileDescriptor, block_num * BLOCK_SIZE + offset, SEEK_SET);
    read(fileDescriptor, &tmp_inode, INODE_SIZE);
    return tmp_inode;
}

// Write the given inode into the num position in inode table!
void write_inode_num(int inode_num, inode_type inode) {
    unsigned int block_num = (inode_num - 1) * INODE_SIZE / BLOCK_SIZE + 2;
    unsigned int offset = (inode_num - 1) * INODE_SIZE % BLOCK_SIZE;
    lseek(fileDescriptor, blk_num * BLOCK_SIZE + offset, SEEK_SET);
    write(fileDescriptor, &inode, INODE_SIZE);
}

// Get the number for "a" free inode
unsigned short get_free_inode_num() {
    if (superBlock.ninode == 0) {
        // Iterate through all the inodes and add free ones to the inode[] array.
        // Start from i = 2 since i = 1 is for root.
        for (int i = 2; i <= num_inodes, superBlock.ninode <= I_SIZE; ++i) {
            inode_type tmp_inode = read_inode_from_num(i);
            // flags >> 15 is the M.S.B and taking its AND with 1: free or not?
            if ((tmp_inode.flags >> 15) & 1 == 0) {  // Free inode!
                superBlock.inode[superBlock.ninode] = i;
                ++superBlock.ninode;
            }
        }
        --superBlock.ninode;
        return superBlock.inode[ninode];
    }
}

// Get the block number for a free block.
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

// Copy in external file into a v6 file
void copy_in(char *external_file, char *v6_filename) {
    int extf_descriptor;
    if ((extf_descriptor = open(external_file, O_RDONLY, 0700)) == -1) {
        printf("\n open() failed with the following error [%s]\n", strerror(errno));
        return 0;
    }

    int inode_num = get_free_inode_num();
    inode_type inode = read_inode_from_num(inode_num);

    inode.flags = inode_alloc_flag | dir_access_rights;  // set flags
    inode.nlinks = 0;
    inode.uid = 0;
    inode.gid = 0;
    inode.size = INODE_SIZE;
    inode.actime[0] = 0;
    inode.modtime[0] = 0;
    inode.modtime[1] = 0;
    // Now for the data blocks!
    unsigned int buffer[BLOCK_SIZE / 4];
    unsigned int read_flag = BLOCK_SIZE, fsize = 0, block_num;
    int idx = 0;  // idx in the addr[] array for the v6 file's inode.
    while (read_flag == BLOCK_SIZE) {
        read_flag = read(extf_descriptor, buffer, BLOCK_SIZE);
        block_num = get_free_block();
        lseek(fileDescriptor, block_num * BLOCK_SIZE, SEEK_SET);
        write(fileDescriptor, buffer, BLOCK_SIZE);
        inode.addr[idx] = block_num;
        idx += 1;
        fsize += read_flag;
    }
    write_inode_num(inode_num, inode);

    // Update Root Directory Data Block with v6file (All files are stored in root)
    dir_type v6file;
    v6file.inode = inode_num;
    v6file.filename = v6_filename;

    int root_inode_num = 1;
    inode_type root_inode = read_inode_from_num(root_inode_num);
    
    int bidx_addr = root_inode.size / BLOCK_SIZE; 
    int offset = root_inode.size % BLOCK_SIZE; 
    int block_num = addr[bidx_addr]; // < 11?
    lseek(fileDescriptor, block_num * BLOCK_SIZE + offset, SEEK_SET);
    write(fileDescriptor, &v6file, sizeof(dir_type));
    
    // update root's inode size!
    root_inode.size += sizeof(dir_type);
    write_inode_num(root_inode_num, root_inode);
}

// Copy out from a v6 file into an external file
void copy_out(char *v6_file, char *external_file) {
    printf("Implementing!\n");
}
