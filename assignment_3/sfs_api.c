
// ALEXANDER BRATYSHKIN
// 260684228

#include "sfs_api.h"

#define DEBUG // toggle debug statements

//initialize all bits to high

#pragma region GLOBAL DEFINITIONS

// seriously, sorry again for these global variables, but I think it is forgiveable in this assignment since we're implementing an API, no main()
superblock_t *sb;
inode_t inode_table[INODES];
directory_entry root[INODES];
file_descriptor fd_table[INODES];
uint8_t free_bit_map[BITMAP_ROW_SIZE] = {[0 ... BITMAP_ROW_SIZE - 1] = UINT8_MAX};
int current_position = 0;

#pragma endregion

#pragma region API CALLS

void mksfs(int fresh)
{
    // TODO: Add debug statements

    if (fresh)
    {
        D printf("New file system \n");

        // format virtual disk provided by emulator
        assert(init_fresh_disk(LASTNAME_FIRSTNAME_DISK, BLOCK_SIZE, NUM_BLOCKS));

        // initialize superblock
        assert(superblock_init());
        force_set_index(0);

        // initialize inode table
        for (int i = 0; i < ARRAYSIZE(inode_table); i++)
        {
            inode_table->empty = 1;
        }

        // initialize fd table
        for (int i = 0; i < ARRAYSIZE(fd_table); i++)
        {
            fd_table->empty = 1;
        }

        set_fd(0, 0, 0, &inode_table[0], 0);

        // initialize node table
        for (int i = 1; i < INODE_BLOCKS_NO; i++) // start from 1 because first block for sb
        {
            force_set_index(i);
        }

        // initialize directory's pointers for root
        for (int i = 0; i < DIR_BLOCKS; i++)
        {
            int inode_wrapper = i + INODE_BLOCKS_NO + 1;
            inode_table[0].data_ptrs[i] = (unsigned int)inode_wrapper;
            force_set_index(inode_wrapper); // store root directory in bitmap
        }

        // store inode table in bitmap
        for (int i = 1; i < FS_SIZE_INITIAL + 1; i++) // start from 1 because first block for sb
        {
            force_set_index(i);
        }

        // store bitmap itself in bitamp
        force_set_index(NUM_BLOCKS - 1);

        assert(store_in_disk());

        return;
    }

    D printf("Open existing file system \n");

    assert(read_blocks(0, 1, &sb));

    return;
}

int sfs_getnextfilename(char *fname)
{
    D printf("Inside of sfs_getnextfilename");

    // TODO: Make this cleaner, better

    if (root[current_position].empty == 1 || current_position >= INODES)
    {
        printf(CANNOT_FIND_FILE);
        return 0;
    }

    current_position++;
    strcpy(fname, root[current_position].name);

    return 1;
}

int sfs_getfilesize(const char *path)
{
    int inode_num = find_file_inode_table((char *)path);

    if (inode_num == -1)
    {
        printf(CANNOT_FIND_FILE);
        return 0;
    }

    return inode_table[inode_num].size;
}

int sfs_fopen(char *name)
{
    D printf("I'm in sfs_fopen\n");

    if (strlen(name) > MAX_FILE_NAME || strlen(name) == 0)
    {
        printf(INVALID_FILE_NAME);
        return -1;
    }

    int inode_num = find_file_inode_table(name);

    // if the file exists (begin by this condition because we're more likely to open a file that exists than try to open one that doesn't)
    if (inode_num != -1)
    {
        int fd_num = find_file_fd_table(inode_num);
        if (fd_num != -1)
        {
            // if the file is already open, we return it
            return fd_num;
        }

        // find first non-used entry if the file isn't open
        int empty_fd = find_empty_fd();

        if (empty_fd == -1)
        {
            printf(TABLE_FULL);
            return -1;
        }

        // we open the file
        set_fd(empty_fd, 0, inode_num, &inode_table[inode_num], inode_table[inode_num].size); // set file to append mode

        return empty_fd;
    }

    int empty_inode = find_empty_inode();

    if (empty_inode == -1)
    {
        printf(INODE_TABLE_FULL);
        return -1;
    }

    int empty_root_entry = find_empty_dir();

    if (empty_root_entry == -1)
    {
        printf(ROOT_TABLE_FULL);
        return -1;
    }

    // write file in root directory
    set_root_table_entry(empty_root_entry, 0, empty_inode, name);

    // inode_table[empty_inode].size = 0;

    int empty_fd_table_entry = find_empty_fd();

    if (empty_fd_table_entry == -1)
    {
        printf(TABLE_FULL);
        return -1;
    }

    set_fd(empty_fd_table_entry, 0, empty_inode, &inode_table[empty_inode], inode_table[empty_inode].size);
    write_blocks(1, sb->inode_table_len, inode_table);
    write_blocks(1 + sb->inode_table_len, DIR_BLOCKS, root);

    return empty_fd_table_entry;
}

int sfs_fclose(int fileID)
{
    D printf("I'm in sfs_fclose\n");

    if (fileID > INODES || fileID < 0)
    {
        printf(INVALID_FILE_ID);
        return -1;
    }

    set_fd(fileID, 1, -1, NULL, -1);
    return 0;
}

int sfs_fread(int fileID, char *buf, int length)
{
    D printf("I'm in sfs_fread");
    if (length <= 0)
    {
        printf(WRONG_LENGTH);
        return 0;
    }

    if (buf == NULL)
    {
        printf(BUF_NULL);
        return 0;
    }

    if (fd_table[fileID].empty == 0)
    {
        printf(FILE_NOT_OPEN);
        return 0;
    }

    file_descriptor *fd = &fd_table[fileID];
    inode_t *inode = &inode_table[fd->inodeIndex];

    int wrapped_length = (fd->rwptr + length > inode->size) ? inode->size - fd->rwptr : length;

    if (wrapped_length <= 0)
    {
        return 0;
    }

    // starting block
    int offset = fd->rwptr / sb->block_size;

    // which block we started at + which blocked we stopped writing at = total # of blocks to read
    int blocks_to_read = ((fd->rwptr % sb->block_size) + wrapped_length) / sb->block_size + 1;

    D printf("BLOCKS TO READ: %i \n", blocks_to_read);

    char *block_buffer = calloc(1, sb->block_size);                           // hold each block
    char *file_buffer = calloc(1, (size_t)(blocks_to_read * sb->block_size)); // hold the whole file

    int *ind_pointers = malloc(sb->block_size);

    for (int i = 0; i < blocks_to_read; i++)
    {
        if (i != 0)
        {
            memset(block_buffer, 0, sb->block_size); // reset buffer from previous iteration
        }

        int current_block;

        // if the block to read is in the direct pointer
        if (i < POINTER_SIZE)
        {
            current_block = inode->data_ptrs[i];
        }

        // if the block to read is in the indirect pointer
        else
        {
            if (inode->indirectPointer != -1)
            {
                read_blocks(inode->indirectPointer, 1, ind_pointers);
                current_block = ind_pointers[i - POINTER_SIZE];
            }
            else
            {
                free(block_buffer);
                free(file_buffer);
                return 0;
            }
        }

        read_blocks(current_block, 1, block_buffer);
        memcpy(file_buffer + (i + sb->block_size), block_buffer, sb->block_size); // add current block to whole file
    }

    // memcpy(buf, file_buffer, wrapped_length);

    memcpy(buf, file_buffer + fd->rwptr, wrapped_length);

    free(block_buffer);
    free(file_buffer);
    free(ind_pointers);

    return wrapped_length;
}

int sfs_fwrite(int fileID, const char *buf, int length)
{
    D printf("I'm in sfs_fwrite\n");
}

int sfs_fseek(int fileID, int loc)
{
    D printf("I'm in sfs_fseek\n");
}

int sfs_remove(char *file)
{
    D printf("I'm in sfs_remove\n");
}

#pragma endregion

#pragma region HELPER FUNCTIONS

int store_in_disk()
{
    if (write_blocks(0, 1, &sb) == 0 || write_blocks(NUM_BLOCKS - 1, 1, free_bit_map) == 0 || write_blocks(1, FS_SIZE_INITIAL, inode_table) == 0 || write_blocks(FS_SIZE_INITIAL + 1, DIR_BLOCKS, root) == 0)
    {
        return 0;
    }

    return 1;
}

int read_all_from_disk()
{
    if (read_blocks(0, 1, &sb) == 0 || read_blocks(NUM_BLOCKS - 1, 1, free_bit_map) == 0 || read_blocks(1, FS_SIZE_INITIAL, inode_table) == 0 || read_blocks(FS_SIZE_INITIAL + 1, DIR_BLOCKS, fd_table) == 0)
    {
        return 0;
    }

    return 1;
}

// TODO: Refactor to:
// int find_empty_thing(void* thing, int length, int size, bool (isEmpty*)(void*, int))
// {
//     for (int i = 0; i < length * size; i+=size)
//     {
//         if (isEmpty(thing, i))
//         {
//             return i/size;
//         }
//     }

//     return -1;
// }

int find_file_inode_table(char *name)
{
    for (int i = 0; i < ARRAYSIZE(root); i++)
    {
        if (root[i].empty == 0 && strcmp(root[i].name, name) == 0)
        {
            return root[i].num;
        }
    }

    return -1;
}

int find_file_fd_table(int inode_num)
{
    for (int i = 0; i < ARRAYSIZE(fd_table); i++)
    {
        if (fd_table[i].inodeIndex == inode_num)
        {
            return i;
        }
    }

    return -1;
}

int superblock_init()
{
    sb->magic = MAGIC;
    sb->block_size = BLOCK_SIZE;
    sb->fs_size = FS_SIZE_INITIAL;
    sb->inode_table_len = INODE_BLOCKS_NO;
    sb->root_dir_inode = 0;

    // TODO: think of some ways this method would fail?
    return 1;
}

int set_fd(int fd_index, int empty, uint64_t inodeIndex, inode_t *inode, uint64_t rwptr)
{
    fd_table[fd_index] = (file_descriptor){.empty = empty, .inodeIndex = inodeIndex, .inode = inode, .rwptr = rwptr};

    return 1;
}

int set_root_table_entry(int root_index, int empty, int inode_num, char name[MAX_FILE_NAME])
{
    root[root_index] = (directory_entry){.num = inode_num, .empty = empty};
    strcpy(root[root_index].name, name);
    // TODO: Maybe null terminate the string?

    return 1;
}

int find_empty_fd()
{
    for (int i = 0; i < ARRAYSIZE(fd_table); i++)
    {
        if (fd_table[i].empty == 1)
        {
            return i;
        }
    }

    return -1;
}

int find_empty_inode()
{
    for (int i = 0; i < ARRAYSIZE(inode_table); i++)
    {
        if (inode_table[i].empty == 1)
        {
            return i;
        }
    }

    return -1;
}

int find_empty_dir()
{
    for (int i = 0; i < ARRAYSIZE(root); i++)
    {
        if (root[i].empty == 1)
        {
            return i;
        }
    }

    return -1;
}

#pragma endregion

int main()
{
    // mksfs(1);
    sfs_fopen("text.txt");
}

#pragma region BITMAP REGION

void force_set_index(uint32_t index)
{
    // get index in array of which bit to free
    uint32_t i = index / 8;

    // get which bit to use
    uint8_t bit = index % 8;

    // use bit
    USE_BIT(free_bit_map[i], bit);
}

uint32_t get_index()
{
    uint32_t i = 0;

    // find the first section with a free bit
    // let's ignore overflow for now...
    while (free_bit_map[i] == 0)
    {
        i++;
    }

    // now, find the first free bit
    /*
        The ffs() function returns the position of the first (least
       significant) bit set in the word i.  The least significant bit is
       position 1 and the most significant position is, for example, 32 or
       64.  
    */
    // Since ffs has the lsb as 1, not 0. So we need to subtract
    uint8_t bit = ffs(free_bit_map[i]) - 1;

    // set the bit to used
    USE_BIT(free_bit_map[i], bit);

    //return which block we used
    return i * 8 + bit;
}

void rm_index(uint32_t index)
{

    // get index in array of which bit to free
    uint32_t i = index / 8;

    // get which bit to free
    uint8_t bit = index % 8;

    // free bit
    FREE_BIT(free_bit_map[i], bit);
}

#pragma endregion