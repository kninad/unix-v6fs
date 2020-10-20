# Project Notes

Disk file convention: `v6fs.disk` -- adding a `.disk` at the end so that I can safely 
add it to `.gitignore` file for the repo.

`main()`: we go into main and use the `strtok` function on the input on each iteration of
the while loop -- working on one command per loop iteration like: `initfs,cpin,cpout..`.
Further calls to `strtok` within a loop itr give us the additional arguments passed via
the command.

`initfs()`: first opens the file using the given path! this code is useful and should be 
used later on in other functions too?
inits the superblock and also writes in the empty inode blocks and the data blocks.
Then we write out the root using the `create_root` call. Finally, we add all remaining
data blocks to the free list.
Note that all throughout initfs, the buffer array is empty. Think
of it like a placeholder to represent all zero entries.

`int buf[blk_size / 4]` -- buffer is an array representing the data for a block.
Each blk is 1024 bytes big (`blk_size = 1024`) and an int is 4 bytes, so that we will have
1024/4 as the size of the buffer.

`create_root()`: write out the inode for the root to the appropriate block (2nd block
overall, and the first inode) -- right after the superblock. write out the data block
of the root, it will be a `dir_entry` struct named `root` which is written to the 1st 
data block which is at `superblock.isize + 2` location.

In `add_block_to_free_list`, we only need to create the `free_list_data` to be of
length `FREE_SIZE` rather than the entire `BLK_SIZE / 4`. free size will presumably take
lesser space -- however it really does not matter in the grand scheme of things.

`copy_in`: open the external file and v6 file system. v6 file will always be within the 
root directory. create inode for the file. Start filling in info one block at a time
and store the blocks in inode's addr using a tmp buffer. 


`copy_out`:
