# unix-v6fs

Implementation of some aspects of the unix v6 file system commands like:

- initfs : initialize fs with given number of inodes.
- cpin : copy in external file
- cpout : copy out external file
- cd : change dir
- rm : delete file or empty directory
- mkdir : make new directory
- cd : Change current directory
- pwd : Print current directory
- ls : Print list of files and folders in current directory

There is only a single source file `fsaccess.c` located under the src folder. The file
`h1.txt` can be used as a sample external file to test the functionality.


