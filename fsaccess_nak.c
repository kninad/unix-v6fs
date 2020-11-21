/***********************************************************************
* Author: Ninad Khargonkar
* NetID: NXK180069
* Project - 2: Implementation of V6 File System
* CS 5348.001 Operating Systems: Fall 2020
* Prof. S Venkatesan
* **********************************************************************
* Compilation:- $ gcc fsaccess_nak.c -std=gnu99 -o v6fs
* Run using  :- $ ./v6fs
* You will get a prompt where you can perform the following actions. The first command
* on startup of the program always has to be initfs. 
* **********************************************************************
* This program allows user to do two things:
    1. initfs: Initilizes the file system and redesigning the Unix file system to accept large
        files of up tp 4GB, expands the free array to 152 elements, expands the i-node array to
        200 elemnts, doubles the i-node size to 64 bytes and other new features as well.
    
    2. Quit: save all work and exit the program.
    
    3. cpin: Copy in an external file into a v6 file.

    4. cpout: Copy a v6 file into an external file.

    5. rm: Remove a v6 file/empty directory.

    6. mkdir: Construct a new directory.

    7. cd: Change the current working directory (pwd or cwd).

    8. pwd: Print the current working directory.

    9. ls: Print the contents (file names) of current working directory.

* User Input:
    - initfs (file path) (# of total system blocks) (# of System i-nodes)
    - q
    - cpin (external file path) (v6 file)
    - cpout (v6 file) (external file path)
    - rm (v6 file)
    - mkdir (v6 dir)
    - cd (v6 path)
    - ls
    - pwd

* File name is limited to 14 characters.
***********************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>   // for dirname() and basename() calls
#include <stdbool.h>  // for boolean data types
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FREE_SIZE 152
#define I_SIZE 200
#define BLOCK_SIZE 1024
#define ADDR_SIZE 11
#define INPUT_SIZE 256
#define MAX_F_LEN 80  // 80 = 14 * 5 + 10 (extra space). Assuming max 5 levels deep.

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

bool DEBUG_FLAG = false;  // whether to enable print based debugging?
// Present working directory (pwd) variables
char pwd_path[MAX_F_LEN];
unsigned short pwd_inode_num;  // current inode number

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
int valid_path(char *path);
int get_inode_from_base(char *dirname, int basedir_inode_num);
bool is_plain_file_vInode(unsigned int inode_num);

bool double_slash_in_path(char *path);
int get_parent_dir(unsigned int dir_inode_num);
int special_path(char *path);
void print_absolute_path(unsigned int cur_inode_num);
int traverse_path(char *path);
void list_pwd();
bool is_empty_dir(int dir_inode_num);

int copy_in_2(char *external_file, char *v6_filename);
int copy_out_2(char *v6_file, char *external_file);

int remove_file(char *path);
int make_dir(char *path);
int change_dir(char *path);



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
    printf("* Available commands are: 1) initfs 2) cpin 3) cpout 4) cd 5) mkdir 6) rm 7) pwd 8) ls 9) q\n");
    printf("* The first command on startup always has to be initfs.\n");
    printf("\nEnter the required command:\n");

    while (1) {
        // printf("ninad@V6FS:%s $ ", pwd_path);
        printf("ninad@V6FS:$ ");
        scanf(" %[^\n]s", input);
        // breaks down input string using the provided delimiter (here: " ")
        splitter = strtok(input, " ");

        if (strcmp(splitter, "initfs") == 0) {
            preInitialization();
            splitter = NULL;  // why? safety?
        } else if (strcmp(splitter, "ls") == 0) {
            list_pwd();
            printf("\n\n");
        } else if (strcmp(splitter, "pwd") == 0) {
            print_absolute_path(pwd_inode_num);
            printf("\n");
        } else if (strcmp(splitter, "cpin") == 0) {
            char *arg1 = strtok(NULL, " ");
            char *arg2 = strtok(NULL, " ");
            if (!arg1 || !arg2) {
                printf("Arguments (external-file and v6-file) have not been entered?\n\n");
            } else {
                int ret = copy_in_2(arg1, arg2);
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
                int ret = copy_out_2(arg1, arg2);
                if (ret <= 0) {
                    printf("Copy out operation failed! \n\n");
                } else {
                    printf("Copy out operation was successful.\n\n");
                }
            }
        } else if (strcmp(splitter, "cd") == 0) {
            char *arg1 = strtok(NULL, " ");
            // char *arg2 = strtok(NULL, " ");
            if (!arg1) {
                printf("Arguments (path) have not been entered?\n\n");
            } else {
                int ret = change_dir(arg1);
                if (ret < 0) {
                    printf(" cd operation failed! \n\n");
                } else {
                    printf(" cd operation was successful.\n\n");
                }
            }
        } else if (strcmp(splitter, "mkdir") == 0) {
            char *arg1 = strtok(NULL, " ");
            // char *arg2 = strtok(NULL, " ");
            if (!arg1) {
                printf("Arguments (path) have not been entered?\n\n");
            } else {
                int ret = make_dir(arg1);
                // printf("arg: %s\n\n", arg1);
                if (ret < 0) {
                    printf(" mkdir operation failed! \n\n");
                } else {
                    printf(" mkdir operation was successful.\n\n");
                }
            }
        } else if (strcmp(splitter, "rm") == 0) {
            char *arg1 = strtok(NULL, " ");
            // char *arg2 = strtok(NULL, " ");
            if (!arg1) {
                printf("Arguments (path) have not been entered?\n\n");
            } else {
                int ret = remove_file(arg1);
                // printf("arg: %s\n\n", arg1);
                if (ret < 0) {
                    printf(" rm operation failed! \n\n");
                } else {
                    printf(" rm operation was successful.\n\n");
                }
            }
        } else if (strcmp(splitter, "q") == 0) {
            lseek(fileDescriptor, BLOCK_SIZE, SEEK_SET);
            write(fileDescriptor, &superBlock, BLOCK_SIZE);
            // if(DEBUG_FLAG){
            //     print_superblock();
            // }
            close(fileDescriptor);
            printf("Quitting!\n");
            printf("***************************\n");
            return 0;
        } else {
            printf("Please provide a valid command!\n\n");
        }
    }  // end while loop
}  // end main



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
        if (DEBUG_FLAG) {  //check if superblock is loaded correctly!
            print_superblock();
        }
        strcpy(pwd_path, "/");  // pwd is root at the beginning
        pwd_inode_num = 1;
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
    // pwd is root at the beginning
    create_root();
    // pwd_path[0] = "/";
    strcpy(pwd_path, "/");
    pwd_inode_num = 1;
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
    // inode.size = INODE_SIZE;
    inode.size = 2 * sizeof(dir_type);
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
    if (DEBUG_FLAG) {
        printf(" superblock.ninode: %d\n", superBlock.ninode);
    }
    if (superBlock.ninode == 0) {
        // Iterate through all the inodes and add free ones to the inode[] array.
        // Start from i = 2 since inode_num = 1 is for root.
        for (int i = 2; i <= num_inodes && superBlock.ninode <= I_SIZE; ++i) {
            inode_type tmp_inode = read_inode_from_num(i);
            if (DEBUG_FLAG) {
                printf(" found an inode: %d ; ", i);
            }
            // flags >> 15 is the M.S.B and taking its AND with 1: free or not?
            int bits_flags = sizeof(short) * 8;   // number of bits for flags type
            int mask = 1 << (bits_flags - 1);     // bitwise: 1 followed by 15 zeros.
            if ((tmp_inode.flags & mask) == 0) {  // Free inode!
                if (DEBUG_FLAG) {
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


// Check whether the provided inode flags correspond to a plain file.
bool is_plain_file(unsigned short flags) {
    bool is_alloc_file = ((flags & inode_alloc_flag) != 0);
    bool is_dir = ((flags & dir_flag) != 0);
    bool ans = is_alloc_file && !(is_dir);  // allocated as file and not dir => plain file!
    // printf(" FlagCheck: is_alloc: %d, is_dir: %d\n", is_alloc_file, is_dir);
    return ans;
}


// v2 Check whether the provided inode flags correspond to a plain file.
bool is_plain_file_vInode(unsigned int inode_num) {
    inode_type tmp = read_inode_from_num(inode_num);
    unsigned short flags = tmp.flags;
    bool is_alloc_file = ((flags & inode_alloc_flag) != 0);
    bool is_dir = ((flags & dir_flag) != 0);
    bool ans = is_alloc_file && !(is_dir);  // allocated as file and not dir => plain file!
    // printf(" FlagCheck: is_alloc: %d, is_dir: %d\n", is_alloc_file, is_dir);
    return ans;
}


// Should return the inode number if dirname found in the base dir, else 0.
// Negative number if Error.
int get_inode_from_base(char *dirname, int basedir_inode_num) {
    // Check for v6file in base. Read in 16 byte segments of dir_type and compare given v6 dirname
    inode_type base_inode = read_inode_from_num(basedir_inode_num);

    if (!(check_flag_dir(base_inode.flags))) {
        printf(" Error: Base directory not allocated or not init as a directory!\n");
        return -1;
    } else if (DEBUG_FLAG) {
        printf("Base Dir allocation check passed.\n");
    }

    unsigned int dir_inode_num = 0;
    dir_type tmp_buffer;
    bool flag_found = false;
    // Read in the data: directory entries for this base dir
    for (int i = 0; i < ADDR_SIZE; ++i) {
        if (flag_found || base_inode.addr[i] == 0) {
            // printf("debug: %d, %d\n", flag_found, root_inode.addr[i]);
            break;
        }
        unsigned int block_num = base_inode.addr[i];
        lseek(fileDescriptor, block_num * BLOCK_SIZE, SEEK_SET);
        unsigned int read_bytes = read(fileDescriptor, &tmp_buffer, sizeof(dir_type));
        unsigned int count = read_bytes;
        while (read_bytes > 0 && count <= BLOCK_SIZE) {
            if ((filename_in_direntry(dirname, tmp_buffer)) && (tmp_buffer.inode > 0)) {
                dir_inode_num = tmp_buffer.inode;  // inode number saved!
                flag_found = true;                 // to break out of outer loop
                break;
            }
            read_bytes = read(fileDescriptor, &tmp_buffer, sizeof(dir_type));
            count += read_bytes;
        }
    }
    return dir_inode_num;
}


// Will fill in the string buffer with the dirname corresponding to the child inode num if it
// is found in base_dir
void get_dirname_from_base(char *string_buffer, int child_inode_num, int basedir_inode_num) {
    // Check for v6file in base. Read in 16 byte segments of dir_type and compare given v6 dirname
    inode_type base_inode = read_inode_from_num(basedir_inode_num);
    if (!(check_flag_dir(base_inode.flags))) {
        printf(" Error: Base directory not allocated or not init as a directory!\n");
        // return -1;
    } else if (DEBUG_FLAG) {
        printf("Base Dir allocation check passed.\n");
    }

    dir_type tmp_buffer;
    bool flag_found = false;

    for (int i = 0; i < ADDR_SIZE; ++i) {
        if (flag_found || base_inode.addr[i] == 0) {
            break;
        }
        unsigned int block_num = base_inode.addr[i];
        lseek(fileDescriptor, block_num * BLOCK_SIZE, SEEK_SET);
        unsigned int read_bytes = read(fileDescriptor, &tmp_buffer, sizeof(dir_type));
        unsigned int count = read_bytes;
        while (read_bytes > 0 && count <= BLOCK_SIZE) {
            if (tmp_buffer.inode == child_inode_num) {
                strcpy(string_buffer, tmp_buffer.filename);  // Copy the filename here!
                flag_found = true;                           // to break out of outer loop
                break;                                       // Now break out of the while loop
            }
            read_bytes = read(fileDescriptor, &tmp_buffer, sizeof(dir_type));
            count += read_bytes;
        }
    }
}


// Given a directory entry and parent inode number, writes to appropriate location
// in parent's data block and updates its size!
int write_to_parent(dir_type dir, int parent_inode_num) {
    // Make this into a general function.
    // Update Root Directory Data Block with v6file info (All files are stored in root)
    inode_type root_inode = read_inode_from_num(parent_inode_num);

    dir_type tmp_buffer;
    for (int i = 0; i < ADDR_SIZE; ++i) {
        if (root_inode.addr[i] == 0) {
            break;
        }
        unsigned int blk_num = root_inode.addr[i];
        lseek(fileDescriptor, blk_num * BLOCK_SIZE, SEEK_SET);
        int read_bytes = read(fileDescriptor, &tmp_buffer, sizeof(dir_type));
        int count = read_bytes;
        while (read_bytes > 0 && count <= BLOCK_SIZE && tmp_buffer.inode > 0) {
            // printf("%d:%s, ", tmp_buffer.inode, tmp_buffer.filename);
            read_bytes = read(fileDescriptor, &tmp_buffer, sizeof(dir_type));
            count += read_bytes;
        }
        // Free Space for a Dir Entry!
        if(tmp_buffer.inode == 0){
            // Reset file lseek
            count -= read_bytes;
            lseek(fileDescriptor, blk_num * BLOCK_SIZE + count, SEEK_SET);
            write(fileDescriptor, &dir, sizeof(dir_type));
            break;
        }
    }

    // update root's inode size and write out inode
    root_inode.size += sizeof(dir_type);  // since one more child addded to root!
    write_inode_num(parent_inode_num, root_inode);
    return 1;  // success!
}


// Given a directory entry and parent inode number, writes to appropriate location
// in parent's data block and updates its size!
int write_to_parent_old(dir_type dir, int parent_inode_num) {
    // Make this into a general function.
    // Update Root Directory Data Block with v6file info (All files are stored in root)
    inode_type root_inode = read_inode_from_num(parent_inode_num);
    // Move to appropriate position in root dir's last data block and write out info
    unsigned int bidx_addr = root_inode.size / BLOCK_SIZE;
    unsigned int offset = root_inode.size % BLOCK_SIZE;
    if (offset == 0) {  // If perfectly divides, move to the next block (next idx in addr[])
        ++bidx_addr;
    }
    if (bidx_addr >= ADDR_SIZE) {
        printf("\nNo more space in Parent Directory for additional files!, Error! \n");
        return -1;
    }
    unsigned int root_last_block_num = root_inode.addr[bidx_addr];  // < 11?
    lseek(fileDescriptor, root_last_block_num * BLOCK_SIZE + offset, SEEK_SET);
    write(fileDescriptor, &dir, sizeof(dir_type));
    // update root's inode size and write out inode
    root_inode.size += sizeof(dir_type);  // since one more child addded to root!
    write_inode_num(parent_inode_num, root_inode);
    return 1;  // success!
}


// Check whether are consecutive "//" chars in the path. Show it as invalid.
bool double_slash_in_path(char *path) {
    char prev = path[0];
    for (int i = 1; i < strlen(path); ++i) {
        if (path[i] == '/' && prev == '/') {
            printf(" Please dont have a consecutive '/' char.\n");
            return true;
        }  //else if (path[i] == '.' && prev == '/') {
        //     printf(" Please dont include a '.' immediately after a '/' \n");
        //     return true;
        // }
        prev = path[i];
    }
    return false;
}


// Returns the inode number for the parent directory
int get_parent_dir(unsigned int dir_inode_num) {
    char parent[] = "..";  // every dir_entry should have this!
    return get_inode_from_base(parent, dir_inode_num);
}


// Return 0 if not, 1 if ".", 2 if "..", 3 if "/"
int special_path(char *path) {
    size_t len = strlen(path);
    if (len == 0 || len > 2) {  // Not a special path by any chance!
        return -1;
    } else if (len == 1 && (strcmp(path, ".") == 0)) {
        return 1;
    } else if (len == 1 && (strcmp(path, "/") == 0)) {
        return 3;
    } else if (len == 2 && (strcmp(path, "..") == 0)) {
        return 2;
    } else {
        return -1;
    }
}

// Print out the absolute path from root for a given dir by going one level up until
// we reach the root dir.
void print_absolute_path(unsigned int cur_inode_num) {
    char tmp_string[100];
    printf(" Current Path: ");
    int par_inode_num = get_parent_dir(cur_inode_num);
    get_dirname_from_base(tmp_string, cur_inode_num, par_inode_num);
    printf("%s <- ", tmp_string);
    // printf("%s (cur: %d, par: %d) <- ", tmp_string, cur_inode_num, par_inode_num);
    while (par_inode_num > 1) {
        cur_inode_num = par_inode_num;
        par_inode_num = get_parent_dir(cur_inode_num);
        get_dirname_from_base(tmp_string, cur_inode_num, par_inode_num);
        printf("%s <- ", tmp_string);
        // printf("%s (cur: %d, par: %d) <- ", tmp_string, cur_inode_num, par_inode_num);
    }
    printf("/ (root) \n");
}


// List the contents of PWD
void list_pwd() {
    inode_type pwd_inode = read_inode_from_num(pwd_inode_num);  // pwd_inode

    printf("Listing format -- fname (inode). Free space in between entries will also show up!\n");
    printf("Contents: ");
    // Move to appropriate position in root dir's last data block and write out info
    dir_type tmp_buffer;
    for (int i = 0; i < ADDR_SIZE; ++i) {
        if (pwd_inode.addr[i] == 0) {
            break;
        }
        unsigned int blk_num = pwd_inode.addr[i];
        lseek(fileDescriptor, blk_num * BLOCK_SIZE, SEEK_SET);
        int read_bytes = read(fileDescriptor, &tmp_buffer, sizeof(dir_type));
        int count = read_bytes;
        while (read_bytes > 0 && count <= BLOCK_SIZE) {
            if(tmp_buffer.inode == 0){
                if (strcmp(tmp_buffer.filename, "FREE") != 0){
                    break;
                }
            }
            printf("%s (%d), ", tmp_buffer.filename, tmp_buffer.inode);
            read_bytes = read(fileDescriptor, &tmp_buffer, sizeof(dir_type));
            count += read_bytes;
        }
    }
}


// Check whether a given dir is empty or not. Input is the dir's inode number
bool is_empty_dir(int dir_inode_num) {
    inode_type dir_inode = read_inode_from_num(dir_inode_num);
    printf("  debug: dir size: %d\n", dir_inode.size);
    if(dir_inode.size > 2 * sizeof(dir_type)){        
        return false;
    }    
    return true;
}


// Return final file/dir's inode nume if valid. Else return negative numbers for errors.
int traverse_path(char *path) {
    if (strlen(path) > MAX_F_LEN) {
        printf(" Error: File path too long! Should be under: %d chars.\n", MAX_F_LEN);
        return -1;
    }

    if (double_slash_in_path(path)) {
        printf(" Error: File path invalid! Should not contain consecutive \\ char.\n");
        return -2;
    }

    if (special_path(path) == 1) {  // "."
        // printf("\n   Current dir! \n\n");
        return pwd_inode_num;
    } else if (special_path(path) == 2) {  // ".."
        return get_parent_dir(pwd_inode_num);
    } else if (special_path(path) == 3) {  // "/"
        return 1;                          // ROOT INODE NUM
    }

    // Starting base inode number. Default value is the pwd.
    int base_inode_num = pwd_inode_num;

    //  ("Absolute path!\n");
    if (path[0] == '/') {
        base_inode_num = 1;  // init with ROOT INODE (num = 1)
    }

    // String Tokenizer: https://stackoverflow.com/a/27860945/9579260
    char *rest = NULL;
    char *token = strtok_r(path, "/", &rest);
    while (token) {
        // printf("   debug: %s \n", token);
        if (special_path(token) == 2) {
            // base_inode_num = get_parent_dir(pwd_inode_num); // base is parent dir of pwd
            base_inode_num = get_parent_dir(base_inode_num);
            token = strtok_r(NULL, "/", &rest);
            continue;
        } else if (special_path(token) == 1) {
            // base_inode_num = pwd_inode_num;
            // base_inode nume is unchanged!
            token = strtok_r(NULL, "/", &rest);
            continue;
        }
        int found_inode = get_inode_from_base(token, base_inode_num);
        if (found_inode <= 0) {
            if(DEBUG_FLAG){
                printf(" Error: %s Dir/File not found!\n", token);
            }            
            return -3;
        }
        // ELSE
        // printf(": %s\n", token);
        base_inode_num = found_inode;
        token = strtok_r(NULL, "/", &rest);
    }
    return base_inode_num;
}


// Copy in v2 external file into a v6 file PATH
int copy_in_2(char *external_file, char *v6_filename) {
    // printf("This will over-write any existing v6 file!\n");
    int extf_descriptor;
    if ((extf_descriptor = open(external_file, O_RDONLY, 0700)) == -1) {
        printf("\n[Error] open() for External File failed with the following error [%s]\n", strerror(errno));
        return -1;
    }
    if (DEBUG_FLAG) {
        printf("Opened the external file.\n");
    }

    if (strlen(v6_filename) > MAX_F_LEN) {
        printf(" Error: Path too long! Should be under: %d chars.\n", MAX_F_LEN);
        return -1;
    }

    if (double_slash_in_path(v6_filename)) {
        printf(" Error: Path is invalid!\n");
        return -2;
    }
    if (special_path(v6_filename) > 0) {
        printf(" Error: Path is invalid! Is a directory!\n");
        return -3;
    }
    char copy_str[MAX_F_LEN];  // copy is tmp string for various modifications on the passed path.
    strcpy(copy_str, v6_filename);
    if (traverse_path(copy_str) >= 1) {
        printf(" Error: File already exists!\n");
        return -4;
    } 
    

    // Snippet to get the final dir name!
    // https://stackoverflow.com/a/27860945/9579260
    // Make tmp copy !

    strcpy(copy_str, v6_filename);
    char *rest = NULL;
    char *token = strtok_r(copy_str, "/", &rest);
    char final_file[80];
    while (token) {
        // printf(": %s\n", token);
        if (token) {
            strcpy(final_file, token);
        }
        token = strtok_r(NULL, "/", &rest);
    }    
    // printf(" Final File/Dir Name: %s, path: %s\n", final_file, v6_filename);    
    if (strlen(final_file) > 14) {
        printf(" Error: Too long of a dir name! Should be under 14!\n");
        return -4;
    }
    // USE linux system calls! Defined in <libgen.h>
    strcpy(copy_str, v6_filename);
    char *parent_dir_path = dirname(copy_str);
    // Note that traverse_path modifies the string passed to it!
    // printf(" Parent path: %s, ", parent_dir_path);
    int par_inode_num = traverse_path(parent_dir_path);
    // printf(" Parent inode: %d\n", par_inode_num);
    if (par_inode_num < 1) {
        printf(" Error in Path! Parent Dir not found!\n");
        return -1;
    }
    unsigned short inode_num = get_free_inode_num();
    if (inode_num < 2) {
        printf("Could not allocate inode for file. Error!\n");
        return -1;
    } else {
        printf(" Allocated inode: %d for file\n", inode_num);
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
    // if (!is_plain_file(inode.flags)) {
    //     printf(" Plain file Flag Setting Failed!\n");
    //     // return -5;
    // } else {
    //     printf(" Plain file Flag Setting Passed!\n");
    // }

    // Now for the data blocks!
    char buffer[BLOCK_SIZE] = {0};  // init with zero to avoid garbage value.
    int read_flag, block_num;
    unsigned int fsize = 0, idx = 0;  // idx is for the addr[] array for the v6 file's inode.
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
        // over-write the buffer!
        read_flag = read(extf_descriptor, buffer, BLOCK_SIZE);
    }

    if (DEBUG_FLAG) {
        printf("Wrote the data to data-blocks for the assigned inode.\n");
        printf("The file size is: %d\n", fsize);
    }
    // Set the internal file size, even though its written one block at a time!
    inode.size = fsize;
    write_inode_num(inode_num, inode);

    if (DEBUG_FLAG) {
        printf("Wrote the i-node to the inode-table in num: %d\n", inode_num);
    }

    // Make this into a general function.
    // Update Root Directory Data Block with v6file info (All files are stored in root)
    dir_type v6file;
    v6file.inode = inode_num;
    // strcpy(v6file.filename, v6_filename);  // v6file.filename := v6_filename;
    strcpy(v6file.filename, final_file);
    write_to_parent(v6file, par_inode_num);

    close(extf_descriptor);
    return 1;  // success!
}


// Copy out v2 from a v6 file PATH into an external file
int copy_out_2(char *v6_file, char *external_file) {
    if (strlen(v6_file) > MAX_F_LEN) {
        printf(" Error: Path too long! Should be under: %d chars.\n", MAX_F_LEN);
        return -1;
    }

    if (double_slash_in_path(v6_file)) {
        printf(" Error: Path is invalid!\n");
        return -2;
    }

    if (special_path(v6_file) > 0) {
        printf(" Error: Path is invalid! Is a directory!\n");
        return -3;
    }
    char copy_str[MAX_F_LEN];
    strcpy(copy_str, v6_file);
    int trav_inode_num = traverse_path(copy_str);
    if (trav_inode_num < 2) {
        printf(" Error: File not found!!\n");
        return -4;
    }
    inode_type trav_inode = read_inode_from_num(trav_inode_num);
    printf(" FileInode: %d\n", trav_inode_num);
    if (!is_plain_file(trav_inode.flags)) {
        printf(" Error: Not a plain file, is a directory!\n");
        return -5;
    }

    // Assumes external file does not exist. Otherwise will over-write.
    int extf_descriptor;
    if ((extf_descriptor = open(external_file, O_RDWR | O_CREAT, 0700)) == -1) {
        printf(" open() failed with the following error [%s]\n", strerror(errno));
        return -1;
    }
    if (DEBUG_FLAG) {
        printf("Opened the external file.\n");
    }

    // Snippet to get the final dir name!
    // https://stackoverflow.com/a/27860945/9579260
    // Make tmp copy !

    strcpy(copy_str, v6_file);
    char *rest = NULL;
    char *token = strtok_r(copy_str, "/", &rest);
    char final_file[80];
    while (token) {
        // printf(": %s\n", token);
        if (token) {
            strcpy(final_file, token);
        }
        token = strtok_r(NULL, "/", &rest);
    }
    // printf(" Final File/Dir Name: %s\n", final_file);
    if (strlen(final_file) > 14) {
        printf(" Error: Too long of a dir name! Should be under 14!\n");
        return -4;
    }
    // USE linux system calls! Defined in <libgen.h>
    strcpy(copy_str, v6_file);
    char *parent_dir_path = dirname(copy_str);
    // Note that traverse_path modifies the string passed to it!
    // printf(" Parent path: %s, ", parent_dir_path);
    int par_inode_num = traverse_path(parent_dir_path);
    // printf(" Parent inode: %d\n", par_inode_num);

    if (par_inode_num < 1) {
        printf(" Error in Path! Parent Dir not found!\n");
        return -1;
    }
    // unsigned int root_inode_num = 1;
    // check for existence of the v6 file in parent dir
    // printf("debug: Path before inodev6file: %s\n", v6_file);
    unsigned int inode_num_v6file = trav_inode_num;

    // Now using the inode, write out contents to external file
    inode_type inode_v6file = read_inode_from_num(inode_num_v6file);
    int rem_fsize = inode_v6file.size;  // It will decrement with each write operation.
    char buffer[BLOCK_SIZE] = {0};
    lseek(extf_descriptor, 0, SEEK_SET);  // move to very beginning of external file
    for (int i = 0; i < ADDR_SIZE; ++i) {
        unsigned int curr_block_num = inode_v6file.addr[i];
        if (curr_block_num == 0 || rem_fsize <= 0) {  // go no further!
            break;
        }
        int bytes_to_write = 0;
        // If remaining file size is greater than a block size
        // we write out the entire block, Else we just write the remaining fsize.
        if (rem_fsize > BLOCK_SIZE) {
            bytes_to_write = BLOCK_SIZE;
        } else {
            bytes_to_write = rem_fsize;
        }
        lseek(fileDescriptor, curr_block_num * BLOCK_SIZE, SEEK_SET);
        // print_char_buffer(buffer, BLOCK_SIZE);
        read(fileDescriptor, buffer, BLOCK_SIZE);
        write(extf_descriptor, buffer, bytes_to_write);
        rem_fsize -= bytes_to_write;
    }
    close(extf_descriptor);
    return 1;
}


// Clear the contents of a given block number by over-writing with 0.
// Add the data block to the free list
void clear_and_free_block(unsigned int block_num) {
    int buffer[BLOCK_SIZE / 4] = {0}; // init with zero to avoid garbage value.
    lseek(fileDescriptor, block_num * BLOCK_SIZE, SEEK_SET);
    write(fileDescriptor, buffer, BLOCK_SIZE);
    add_block_to_free_list(block_num, buffer);
    // printf(" debug: Data block cleared!\n");
}


// Clear the contents of an inode by setting flags, vars and addr[] array to 0.
// Make sure that data in the blocks is also cleared. 
// Set the inode to be free.
void clear_inode_data(unsigned int inode_num) {
    inode_type file_inode = read_inode_from_num(inode_num);
    for(int i=0; i<ADDR_SIZE; ++i){        
        if( file_inode.addr[i] != 0){
            clear_and_free_block(file_inode.addr[i]);            
        }
        file_inode.addr[i] = 0;
    }
    file_inode.flags = 0; // Setting all flags to zero! Unallocated!
    write_inode_num(inode_num, file_inode);
    // printf(" debug: Inode cleared!\n");
}


// Remove the dir_entry for a file from the parent(base) dir.
int remove_dir_entry(char* fname, int finode_num, int basedir_inode_num) {
    inode_type base_inode = read_inode_from_num(basedir_inode_num);
    if (!(check_flag_dir(base_inode.flags))) {
        printf(" Error: Base directory not allocated or not init as a directory!\n");
        return -1;
    }
    bool flag_found = false;
    // Read in the data: directory entries for this base dir
    for (int i = 0; i < ADDR_SIZE; ++i) {
        if (flag_found || base_inode.addr[i] == 0) {
            // printf("debug: %d, %d\n", flag_found, root_inode.addr[i]);
            break;
        }
        dir_type tmp_buffer; // For reading in the data.
        dir_type overwrite_buffer;
        overwrite_buffer.inode = 0; // Signifies that it is a free dir entry space.  
        strcpy(overwrite_buffer.filename, "FREE"); // For help in lspwd command.

        unsigned int block_num = base_inode.addr[i];
        lseek(fileDescriptor, block_num * BLOCK_SIZE, SEEK_SET);
        unsigned int read_bytes = read(fileDescriptor, &tmp_buffer, sizeof(dir_type));
        unsigned int count = read_bytes;
        while (read_bytes > 0 && count <= BLOCK_SIZE) {
            if ((filename_in_direntry(fname, tmp_buffer)) && (tmp_buffer.inode == finode_num)) {
                // Found the File to Delete!
                // Seek the position to just before the currently read buffer using the 
                // amount in count.
                // printf(" \ndeb: fname, inode: %s, %d", tmp_buffer.filename, tmp_buffer.inode);
                // printf(" deb: over-writing\n");
                int offset = count - read_bytes;
                lseek(fileDescriptor, block_num * BLOCK_SIZE + offset, SEEK_SET);
                write(fileDescriptor, &overwrite_buffer, sizeof(dir_type));                
                flag_found = true;                 // to break out of outer loop
                break;
            }
            read_bytes = read(fileDescriptor, &tmp_buffer, sizeof(dir_type));
            count += read_bytes;
        }
    }
    // Now reduce the size of the base directory and write out the changes!
    base_inode.size -= sizeof(dir_type);
    write_inode_num(basedir_inode_num, base_inode);
    return 1;   
}


// Remove given v6 file or empty directory from the file system.
int remove_file(char *v6_file) {
    if (strlen(v6_file) > MAX_F_LEN) {
        printf(" Error: Path too long! Should be under: %d chars.\n", MAX_F_LEN);
        return -1;
    }

    if (double_slash_in_path(v6_file)) {
        printf(" Error: Path is invalid!\n");
        return -2;
    }

    if (special_path(v6_file) > 0) {
        printf(" Error: Path is invalid! Is a special directory (root, current, or parent)!\n");
        return -3;
    }
    char copy_str[MAX_F_LEN];
    strcpy(copy_str, v6_file);
    int file_inode_num = traverse_path(copy_str);  
    
    if (file_inode_num < 2) {
        printf(" Error: File not found!!\n");
        return -4;
    } else if (!is_plain_file_vInode(file_inode_num)) {
        if(!is_empty_dir(file_inode_num)){
            printf(" Error: Not a plain file or an empty directory!\n");
            return -5;
        } else {
            printf(" Given path is an empty directory. It will be now deleted!\n");
        } 
    }

    inode_type file_inode = read_inode_from_num(file_inode_num);
    clear_inode_data(file_inode_num);

    // int parent_inode_num = get_parent_dir(trav_inode_num);
    strcpy(copy_str, v6_file);
    char *parent_dir_path = dirname(copy_str);    
    // printf(" debug: Parent path: %s\n", parent_dir_path);
    int par_inode_num = traverse_path(parent_dir_path);
    if (par_inode_num < 0) {
        printf(" Error: Parent Directory %s does not exist!\n", parent_dir_path);
        return -4;
    }
    // printf(" debug: Parent Inode Num: %d\n", par_inode_num);
    strcpy(copy_str, v6_file);
    char *rest = NULL;
    char *token = strtok_r(copy_str, "/", &rest);
    char final_file[80];
    while (token) {
        // printf(": %s\n", token);
        if (token) {
            strcpy(final_file, token);
        }
        token = strtok_r(NULL, "/", &rest);
    }
    // printf(" Final File/Dir Name: %s\n", final_file);
    int res = remove_dir_entry(final_file, file_inode_num, par_inode_num);
    if(res < 0){
        printf(" Error: In removing the dir entry!\n");
        return -6;
    }
    printf(" debug: Deleted file inode: %d\n", file_inode_num);
    return 1;
}


// Change the current directory (pwd) to the given path
int change_dir(char *path) {
    char copy_str[MAX_F_LEN];
    strcpy(copy_str, path);
    int base_inode_num = traverse_path(copy_str);
    if (base_inode_num < 1) {
        printf(" Error: Traversal failed and exited !\n");
        return -1;
    }
    // Finally just need to check if it is a directory!
    inode_type base_inode = read_inode_from_num(base_inode_num);
    if (!(check_flag_dir(base_inode.flags))) {
        // printf(" Error: Terminating file in path is not allocated as a directory!\n");
        printf(" Error: Not a directory!\n");
        return -4;
    }
    // All checks passed! Now update the global pwd variables.
    pwd_inode_num = base_inode_num;
    strcpy(pwd_path, path);
    printf(" PWD inode: %d\n", pwd_inode_num);
    print_absolute_path(pwd_inode_num);
    return 1;
}


// Make a new directory at the given path.
int make_dir(char *path) {
    if (strlen(path) > MAX_F_LEN) {
        printf(" Error: Path too long! Should be under: %d chars.\n", MAX_F_LEN);
        return -1;
    }

    if (double_slash_in_path(path)) {
        printf(" Error: Path is invalid!\n");
        return -2;
    }

    if (special_path(path) > 0) {
        printf(" Error: Path is invalid! Should not be '.', '..' or '/' chars.\n");
        return -3;
    }

    // Snippet to get the final dir name!
    // https://stackoverflow.com/a/27860945/9579260
    // Make tmp copy !
    char copy[100];  // copy is tmp string for various modifications on the passed path.
    strcpy(copy, path);
    char *rest = NULL;
    char *token = strtok_r(copy, "/", &rest);
    char final_file[80];
    while (token) {
        // printf(": %s\n", token);
        if (token) {
            strcpy(final_file, token);
        }
        token = strtok_r(NULL, "/", &rest);
    }
    // printf(" Final File/Dir Name: %s\n", final_file);
    if (strlen(final_file) > 14) {
        printf(" Error: Too long of a dir name! Should be under 14!\n");
        return -4;
    }
    // USE linux system calls! Defined in <libgen.h>
    strcpy(copy, path);
    char *parent_dir_path = dirname(copy);
    // char* parent_dir_name = basename(copy);
    // printf(" Parent path: %s\n", parent_dir_path);
    int par_inode_num = traverse_path(parent_dir_path);
    if (par_inode_num < 0) {
        printf(" Error: Parent Directory %s does not exist!\n", parent_dir_path);
        return -4;
    }
    if (get_inode_from_base(final_file, par_inode_num) > 0) {
        printf(" Error: Directory/File already exists!\n");
        return -5;
    }
    // Now passed the checks for the parent dir and have its inode number.
    inode_type par_inode = read_inode_from_num(par_inode_num);

    // Inode information for the new directory entry.
    int dir_inode_num = get_free_inode_num();
    int dir_block_num = get_free_block();
    inode_type dir_inode;
    dir_type directory;
    dir_inode.flags = inode_alloc_flag | dir_access_rights | dir_flag;  // set flags
    dir_inode.nlinks = 0;
    dir_inode.uid = 0;
    dir_inode.gid = 0;
    dir_inode.size = 0;  // init the file size to zero. will be set later.
    dir_inode.actime[0] = 0;
    dir_inode.modtime[0] = 0;
    dir_inode.modtime[1] = 0;
    // Setting the first data block!
    // Later on can add more if needed!
    dir_inode.addr[0] = dir_block_num;
    // Set rest of addresses in the inode to zero.
    for (int i = 1; i < ADDR_SIZE; i++) {
        dir_inode.addr[i] = 0;
    }

    // Write out to data blocks of the directory: info about . and ..
    // First about "."
    directory.inode = dir_inode_num;
    directory.filename[0] = '.';
    directory.filename[1] = '\0';
    lseek(fileDescriptor, dir_block_num * BLOCK_SIZE, SEEK_SET);
    write(fileDescriptor, &directory, sizeof(dir_type));
    dir_inode.size += sizeof(dir_type);
    // Then write about ".." -- i.e the parent dir
    // Just over-write since directory is a temporary dir_type variable.
    directory.inode = par_inode_num;
    directory.filename[0] = '.';
    directory.filename[1] = '.';
    directory.filename[2] = '\0';
    write(fileDescriptor, &directory, sizeof(dir_type));
    dir_inode.size += sizeof(dir_type);

    // Write the inode corresponding to the new dir entry!
    // Note that size has been updated! Now we can write out the inode.
    write_inode_num(dir_inode_num, dir_inode);

    // Now update the parent dir details with a directory entry
    directory.inode = dir_inode_num;
    strcpy(directory.filename, final_file);

    write_to_parent(directory, par_inode_num);
    printf(" Created new dir, inode: %d\n", dir_inode_num);
    return 1;
}
