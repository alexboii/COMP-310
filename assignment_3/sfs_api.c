
#include "sfs_api.h"

// #define DEBUG // toggle debug statements

//initialize all bits to high

#pragma region GLOBAL DEFINITIONS

uint8_t free_bit_map[BITMAP_ROW_SIZE] = {[0 ... BITMAP_ROW_SIZE - 1] = UINT8_MAX};
superblock_t sb;
inode_t inode_table[INODE_NO];

#pragma endregion

void superblock_init()
{
    sb->magic = MAGIC;
    sb->block_size = BLOCK_SIZE;
    sb->fs_size = NUM_BLOCKS * BLOCK_SIZE;
    sb->inode_table_len = INODE_BLOCKS_NO;
    sb->root_dir_inode = 0;
}

void inode_table_init()
{
    for (int i = 1; i < ARRAYSIZE(inode_table); i++)
    {
        inode_table->free = 0;
    }
}

#pragma region API CALLS

void mksfs(int fresh)
{
    if (fresh)
    {
        D printf("Making a new file system \n");

        superblock_init();
    }
}
int sfs_getnextfilename(char *fname)
{
}
int sfs_getfilesize(const char *path)
{
}
int sfs_fopen(char *name)
{
}
int sfs_fclose(int fileID)
{
}
int sfs_fread(int fileID, char *buf, int length)
{
}
int sfs_fwrite(int fileID, const char *buf, int length)
{
}
int sfs_fseek(int fileID, int loc)
{
}
int sfs_remove(char *file)
{
}

#pragma endregion