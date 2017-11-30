
#include "sfs_api.h"

// #define DEBUG // toggle debug statements

//initialize all bits to high

#pragma region GLOBAL DEFINITIONS

superblock_t sb;
inode_t inode_table[INODE_NO];
entry_t dir_table[NUM_INODES - 1];

#pragma endregion

#pragma region API CALLS

void mksfs(int fresh)
{
    if (fresh)
    {
        D printf("Making a new file system \n");

        // format virtual disk provided by emulator
        init_fresh_disk(JITS_DISK, BLOCK_SIZE, NUM_BLOCKS);

        // initialize superblock
        superblock_init();
        force_set_index(0);
        write_blocks(0, 1, (void *)&sb);

        // initialize node table
        for (int i = 1; i < ARRAYSIZE(inode_table); i++)
        {
            inode_table->free = 1;
        }

        // initialize node table
        for (i = 1; i < INODE_BLOCKS_NO; i++)
        {
            force_set_index(i);
        }

        // initialize root directory
        inode_table[0] = (inode_t){.mode = 777, .link_cnt = 1, .uid = 0, .gid = 0, .size = 0, .free = 0};

        // initialize directory table
        for (i = 0; i < ARRAYSIZE(dir_table); i++)
        {
            inode_table->free = 1;
        }

        for (i = 0; i < DIR_BLOCKS_NO; i++)
        {
            int inode_wrapper = i + INODE_BLOCKS_NO + 1;
            // initialize inode pointers from root
            inode_table[0].data_ptrs[i] = (unsigned int)inode_wrapper;
            force_set_index(inode_wrapper);
        }

        for (i = 0; i < POINTER_SIZE; i++)
        {
            if (inode_table[0].data_ptrs[i] == NULL)
            {
                inode_table[0].data_ptrs[i] = DUMMY_INITIALIZER;
            }
        }

        // TODO: Create method => store_in_memory();
        // write bitmap into memory block?

        return;
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

#pragma HELPER FUNCTIONS

void superblock_init()
{
    sb->magic = MAGIC;
    sb->block_size = BLOCK_SIZE;
    sb->fs_size = NUM_BLOCKS * BLOCK_SIZE;
    sb->inode_table_len = INODE_BLOCKS_NO;
    sb->root_dir_inode = 0;
}

#pragma endregion