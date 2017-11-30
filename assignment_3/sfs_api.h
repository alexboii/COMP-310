#ifndef _INCLUDE_SFS_API_H_
#define _INCLUDE_SFS_API_H_

#include <stdint.h>
#include "bitmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fuse.h>
#include <strings.h>
#include "disk_emu.h"

/* macros */
#define FREE_BIT(_data, _which_bit) \
    _data = _data | (1 << _which_bit)

#define USE_BIT(_data, _which_bit) \
    _data = _data & ~(1 << _which_bit)

#define IS_INDEXABLE(arg) (sizeof(arg[0]))
#define IS_ARRAY(arg) (IS_INDEXABLE(arg) && (((void *)&arg) == ((void *)arg)))
#define ARRAYSIZE(arr) (IS_ARRAY(arr) ? (sizeof(arr) / sizeof(arr[0])) : 0)

#define POINTER_SIZE 12

// for debugging purposes
#ifdef DEBUG
#define D
#else
#define D for (; 0;)
#endif

typedef struct superblock_t
{
    uint64_t magic;
    uint64_t block_size;
    uint64_t fs_size;
    uint64_t inode_table_len;
    uint64_t root_dir_inode;
} superblock_t;

typedef struct inode_t
{
    unsigned int mode;
    unsigned int link_cnt;
    unsigned int uid;
    unsigned int gid;
    unsigned int size;
    unsigned int data_ptrs[POINTER_SIZE];
    unsigned int indirectPointer; // points to a data block that points to other data blocks (Single indirect)
    int free;
} inode_t;

/*
 * inodeIndex    which inode this entry describes
 * inode  pointer towards the inode in the inode table
 *rwptr    where in the file to start   
 */
typedef struct file_descriptor
{
    uint64_t inodeIndex;
    inode_t *inode; //
    uint64_t rwptr;
} file_descriptor;

typedef struct directory_entry
{
    int num;                  // represents the inode number of the entery.
    char name[MAX_FILE_NAME]; // represents the name of the entery.
    int free;
} directory_entry;

#define MAX_FILE_NAME 20
#define MAX_EXTENSION_NAME 3

#define LASTNAME_FIRSTNAME_DISK "sfs_disk.disk"
#define NUM_BLOCKS 1024
#define BLOCK_SIZE NUM_BLOCKS
#define MAGIC 0xACBD0005

// inode constants
#define INODE_NO 14
#define INODE_BLOCKS_NO (sizeof(inode_t) * INODE_NO / BLOCK_SIZE + 1)
#define DIR_BLOCKS_NO (sizeof(entry_t) * INODE_NO / BLOCK_SIZE + 1)
#define DUMMY_INITIALIZER -1

//maximum number of data blocks on the disk.
#define BITMAP_ROW_SIZE (NUM_BLOCKS / 8) // this essentially mimcs the number of rows we have in the bitmap. we will have 128 rows.

void mksfs(int fresh);
int sfs_getnextfilename(char *fname);
int sfs_getfilesize(const char *path);
int sfs_fopen(char *name);
int sfs_fclose(int fileID);
int sfs_fread(int fileID, char *buf, int length);
int sfs_fwrite(int fileID, const char *buf, int length);
int sfs_fseek(int fileID, int loc);
int sfs_remove(char *file);
void init_sb();

#endif //_INCLUDE_SFS_API_H_
