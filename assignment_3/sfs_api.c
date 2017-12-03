
// ALEXANDER BRATYSHKIN
// 260684228

#include "sfs_api.h"

#define DEBUG // toggle debug statements

//initialize all bits to high

#pragma region GLOBAL DEFINITIONS

// seriously, sorry again for these global variables, but I think it is forgiveable in this assignment since we're implementing an API, no main()
superblock_t sb;
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
        init_fresh_disk(LASTNAME_FIRSTNAME_DISK, BLOCK_SIZE, NUM_BLOCKS);

        // initialize superblock
        superblock_init();
        force_set_index(0);

        // initialize inode table
        // TODO: Initialize this properly
        for (int i = 0; i < ARRAYSIZE(inode_table); i++)
        {
            inode_table[i].empty = 1;
            inode_table[i].size = -1;
            inode_table[i].indirectPointer = -1;

            for (int j = 0; j < 12; j++)
            {
                inode_table[i].data_ptrs[j] = -1;
            }

            root[i].empty = 1;
        }

        // initialize fd table
        for (int i = 0; i < ARRAYSIZE(fd_table); i++)
        {
            set_fd(i, 1, -1, NULL, -1);
        }

        set_fd(0, 0, 0, &inode_table[0], 0);
        inode_table[0].empty = 0; // we set it to be used because we just assigned root to it

        // initialize node table
        for (int i = 1; i < ARRAYSIZE(inode_table); i++) // start from 1 because first block for sb
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

        // store bitmap itself in bitamp
        force_set_index(NUM_BLOCKS - 1);

        store_in_disk();

        return;
    }

    D printf("Open existing file system \n");

    read_blocks(0, 1, &sb);

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
            D printf("File already open");
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

    inode_table[empty_inode].empty = 0;

    int empty_root_entry = find_empty_dir();

    if (empty_root_entry == -1)
    {
        printf(ROOT_TABLE_FULL);
        return -1;
    }

    // write file in root directory
    set_root_table_entry(empty_root_entry, 0, empty_inode, name);

    int empty_fd_table_entry = find_empty_fd();

    if (empty_fd_table_entry == -1)
    {
        printf(TABLE_FULL);
        return -1;
    }

    set_fd(empty_fd_table_entry, 0, empty_inode, &inode_table[empty_inode], inode_table[empty_inode].size);
    write_blocks(1, sb.inode_table_len, inode_table);
    write_blocks(1 + sb.inode_table_len, DIR_BLOCKS, root);

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
    int offset = fd->rwptr / sb.block_size;

    // which block we started at + which blocked we stopped writing at = total # of blocks to read
    int blocks_to_read = ((fd->rwptr % sb.block_size) + wrapped_length) / sb.block_size + 1;

    D printf("BLOCKS TO READ: %i \n", blocks_to_read);

    char *block_buffer = calloc(1, sb.block_size);                           // hold each block
    char *file_buffer = calloc(1, (size_t)(blocks_to_read * sb.block_size)); // hold the whole file

    int *ind_pointers = malloc(sb.block_size);

    for (int i = 0; i < blocks_to_read; i++)
    {
        if (i != 0)
        {
            memset(block_buffer, 0, sb.block_size); // reset buffer from previous iteration
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
        memcpy(file_buffer + (i + sb.block_size), block_buffer, sb.block_size); // add current block to whole file
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

    if (fd_table[fileID].empty == 1 || fd_table[fileID].inodeIndex == -1 || length > MAX_FILE_NAME)
    {
        printf(INVALID_FILE_NAME);
        return -1;
    }

    file_descriptor *fd = &fd_table[fileID];
    inode_t *inode = &inode_table[fd->inodeIndex];

    // if the file is empty, assign new block to inode
    if (inode->size <= 0)
    {
        inode->data_ptrs[0] = get_index();
    }

    int first_block = fd->rwptr / sb.block_size;
    int blocks_to_read = ((fd->rwptr) + length) / sb.block_size; // MAYBE DO +1?
    int buffer_pointer = 0;

    for (int i = first_block; i <= blocks_to_read; i++)
    {

        int current_block;
        char *buffer = malloc(BLOCK_SIZE);

        if (first_block < POINTER_SIZE)
        {
            if (inode->data_ptrs[i] == -1)
            {
                int free_data = get_index();
                if (free_data == -1)
                {
                    D printf("No free space in BM");
                    return -1;
                }

                current_block = free_data;
                inode->data_ptrs[i] = current_block;
            }
            else
            {
                current_block = inode->data_ptrs[i];
            }
        }
        else
        {
            //indirect pointers
            int *indirect_pointers = malloc(BLOCK_SIZE);

            D printf("Am I here? idnirect pointers 1");

            // if no indirect pointers initialized
            if (inode->indirectPointer == -1)
            {
                // TODO: Refactor this
                int free_indirect_data = get_index();

                if (free_indirect_data == -1)
                {
                    D printf("No free space in BM");
                    return -1;
                }

                // initialize empty data blocks
                // TODO: Extract 256 constant
                for (int i = 0; i < 256; i++)
                {
                    indirect_pointers[i] = -1;
                }

                write_blocks(free_indirect_data, 1, indirect_pointers);
                inode->indirectPointer = free_indirect_data;

                free(indirect_pointers);
                // NO THIS WILL CAUSE A BUG, CONSIDER MAKING INTO WHILE LOOP INSTEAD
                // THIS WILL NOT REINITIALIZE THE COUNTER I++ TO BE THE SAME
                continue;
            }

            // if indirect pointers already exist
            read_blocks(inode->indirectPointer, 1, indirect_pointers);

            if (indirect_pointers[i - POINTER_SIZE] == -1)
            {

                int free_indirect_data = get_index();

                if (free_indirect_data == -1)
                {
                    D printf("No free space in BM");
                    return -1;
                }

                indirect_pointers[i - 12] = free_indirect_data;

                // persist the newly defined indirect pointers
                write_blocks(inode->indirectPointer, 1, indirect_pointers);

                current_block = indirect_pointers[i - 12];

                D printf("Am I here? idnirect pointers 2");
                // MIGHT CAUSE A BUG
                free(indirect_pointers);
            }
        }

        read_blocks(current_block, 1, buffer);

        // if we're at the first block, we have to start copying from whatever the first bytes of the first block might be, else we're starting from a new block
        // and therefore we have to read from the beginning
        int last_block_index = (i == blocks_to_read) ? (fd->rwptr + length) % sb.block_size : sb.block_size;
        int block_start_offset = (i == first_block) ? fd->rwptr % sb.block_size : 0;
        int block_end_offset = (i == first_block) ? last_block_index - fd->rwptr % sb.block_size : 0;
        memcpy(&buffer[block_start_offset], &buf[buffer_pointer], block_end_offset);
        write_blocks(current_block, 1, buffer);

        buffer_pointer = (i == first_block) ? buffer_pointer + last_block_index : i;

        free(buffer);
    }

    // REFACTOR THIS?
    if (inode->size < fd->rwptr + length)
    {
        inode->size = fd->rwptr + length;
    }

    // place pointer to its new position if everything successful :)
    fd->rwptr = fd->rwptr + length;

    // persist changes in the inode table
    write_blocks(1, sb.inode_table_len, inode_table);

    return buffer_pointer;
}

int sfs_fseek(int fileID, int loc)
{
    D printf("I'm in sfs_fseek\n");

    // TODO: Do error checking here

    fd_table[fileID].rwptr = loc;
    return 0;
}

int sfs_remove(char *file)
{
    D printf("I'm in sfs_remove\n");

    int inode = -1;
    int i;

    // refactor into own method
    for (i = 0; i < ARRAYSIZE(root); i++)
    {
        if (root[i].empty == 0 && strcmp(root[i].name, file) == 0)
        {
            inode = root[i].num;
            break;
        }
    }

    if (inode == -1)
    {
        printf(FILE_NOT_FOUND);
        return -1;
    }

    for (int j = 0; j < POINTER_SIZE; j++)
    {
        if (inode_table[inode].data_ptrs[j] != -1)
        {
            rm_index(inode_table[i].data_ptrs[j]);
            inode_table[inode].data_ptrs[j] = -1;
        }
    }

    if (inode_table[inode].indirectPointer != -1)
    {
        rm_index(inode_table[inode].indirectPointer);
        inode_table[inode].indirectPointer = -1;
    }

    set_root_table_entry(i, 1, -1, "\0");

    write_blocks(1, sb.inode_table_len, inode_table);
    write_blocks(1 + sb.inode_table_len, DIR_BLOCKS, root);

    return 0;
}

#pragma endregion

#pragma region HELPER FUNCTIONS

int store_in_disk()
{
    if (write_blocks((int)0, (int)1, &sb) == 0 || write_blocks(NUM_BLOCKS - 1, 1, free_bit_map) == 0 || write_blocks(1, INODE_BLOCKS_NO, inode_table) == 0 || write_blocks(INODE_BLOCKS_NO + 1, DIR_BLOCKS, root) == 0)
    {
        D printf("Error writing blocks");
        return 0;
    }

    return 1;
}

int read_all_from_disk()
{
    if (read_blocks(0, 1, (void *)&sb) == 0 || read_blocks(NUM_BLOCKS - 1, 1, free_bit_map) == 0 || read_blocks(1, INODE_BLOCKS_NO, inode_table) == 0 || write_blocks(INODE_BLOCKS_NO + 1, DIR_BLOCKS, root) == 0)
    {
        D printf("Error reading blocks");
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
    sb.magic = MAGIC;
    sb.block_size = BLOCK_SIZE;
    sb.fs_size = FS_SIZE_INITIAL;
    sb.inode_table_len = INODE_BLOCKS_NO;
    sb.root_dir_inode = 0;

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

    // int main()
    // {
    //     // mksfs(1);
    //     sfs_fopen("text.txt");
    // }

#pragma region BITMAP REGION

void force_set_index(uint32_t index)
{
    // get index in array of which bit to free
    uint32_t i = index / 8;

    // get which bit to use
    uint8_t bit = index % 8;

    // use bit
    USE_BIT(free_bit_map[i], bit);

    // persist the bm
    write_blocks(1023, 1, free_bit_map);
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

    if (i >= BLOCK_SIZE - 1)
    {
        // nothing available
        return -1;
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

    // persist the bm
    write_blocks(1023, 1, free_bit_map);

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

    // persist the bm
    write_blocks(1023, 1, free_bit_map);
}

#pragma endregion