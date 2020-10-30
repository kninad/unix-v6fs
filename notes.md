# Project Notes

## Testing

- Basic test: Try the commands like initfs, cpin, cpout, q to have no bugs during the 
  course of a single run.
- Persistence: Now just initfs and quit. Then again restart, and try doing cpin, cpout. 
  This has to check whether the earlier quit makes the changes to filesystem
  persistence i.e changes are written appropriately? 
- Can always try interleaving multiple cpin, and cpout commands to different files 
  and maybe the same file. 
- Finally, just compare the contents of the two files: h1.txt and h2.txt to have a basic
  test for cpin and cpout. h1, h2 are sample external files and f1 is a v6 file.

```bash
# assume initfs already done
cpin h1.txt f1 # copy into f1
cpout f1 h2.txt # copy f1 into h2.txt
q

# Now use the diff command line tool or difffile.py to test the difference
diff h1.txt h2.txt
```


## TODO

- why there is no persistence after the `quit()` operation? -- **FIXED**
- On init and quitting, and then resuming again, trying to cpin causes a problem. 
  stuck inside the loop? -- **FIXED**
- During cpin, can we make the v6file such that we only write the amount of bytes read 
  from the external file instead of writing 1024 bytes at a time i.e writing one block
  at a time. This will ensure only required bytes are written -- **NOT NECESSARY**
  Can implement this later as a follow up project!



## Observations

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


`copy_out`: open the external file in append mode!

Implemented but difference in hello.txt and h2.txt!
Maybe need to write/read using a `char buffer[BLOCK_SIZE]` instead of an `int buffer[]`?
Not necessary and important!

Alternatively to only write required number of bytes from the external file, we can
create a tmp buffer inside the while loop using the `read_bytes` variable.



