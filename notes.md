# Project Notes

Testing
-------

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


TODO
----

- In the terminal prompt have the current path (or pwd) before the terminal `$` sign 
  just for the extra aesthetics. -- **DONE**

- During cpin, can we make the v6file such that we only write the amount of bytes read 
  from the external file instead of writing 1024 bytes at a time i.e writing one block
  at a time. This will ensure only required bytes are written -- **DONE**
  
- Print error messages and warnings for every kind of user exceptions. Example: if the
filename is greater than 14 chars etc.


Observations and Notes
----------------------

### Part 2

First, I need to implement a String Slicing algorithm? Or use strtok? Need to get this
right since it will be used again and again.

Need to have a concept of `pwd` for cd, mkdir, rm, cpout. If the provided v6-file is
not present in the pwd or through the absolute path, we should throw an error! 

pwd in the code will be a variable of type `dir_type`: containing the inode (number) for 
it and the name (`char* filename`). So whenever we do a `cd` operation, we should update
the pwd. Init pwd will be root! There will two variables: `pwd_path, pwd_inode`.

So I need a Func:: to get me the inode, etc details about a file/dir given its path 
(relative or absolute). This will be used multiple times across different commands. And
should throw an error message if not found!

Will also need a function Func:: to check the validity of provided path -- either relative
or absolute. Return -1 if not valid! or bool false. Also a simple function to check whether
a path is absolute or relative -- just need to check if it starts with "/" -- absolute
otherwise its a relative path and should check in the pwd.

For relative path, use the pwd inode and iterate through it :: another function!
Func:: Checking whether a file exists in the pwd -- iterate through the pwd inode's 
data blocks one `dir_entry` buffer at a time and try a match with the filename using
the `filename_in_direntry` function (maybe a bit badly-named?).

For absolute path eg, `/usr/ninad/f1`, need to break up the slashes and starting from
root inode, go into the subdirs to find the `dir_entry` associated with the file `f1`.
In this functio, Func:: have a concept of parent and child? Using strtok (to split up)?
Will have to write to understand better and later on can make the commonly used code into
functions.

**rm**: it is only for plain file in the pwd. Its not for dir. So check for that.
- Check if plain file and it exists in the pwd (if provided a relative path) or in the 
  absolute path provided.
- Then free up the inode number (by setting appropriate flags) for that file
- Free up the data associated data blocks for it -- by over-writing it with zero
- Free up the data blocks (from the given number) by adding them to free list?
- Remove the associated `dir_entry`.
- Only small files btw. Note it will be kind of reverse for cpin.
- If the directory is empty, then delete it.

**mkdir**: Should have two entries for "." and "..". Will make a new entry in the pwd
file's (dir) inode for the new directory entry. Should not allow if the some parent dir
of the final dir does not exist. Need to implement a function to check whether a given
path is VALID i.e its dir entries exist.

**cd**: It should be able to work with "." and "..". The file path provided can be 
relative or absolute! If it does not start with "/" then it is a relative path.

**cpin, cpout**: Should work with relative and absolute file paths. Check for the existence
of the basedir -- using `dirname()` : if not ERROR. Create a new entry in this. Else if
just a file name is given, then create an entry in the pwd.



### Part 1

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

