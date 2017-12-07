
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

/** 
 * @brief  Creates a filesystem or opens the currently existing one
 * @note   
 * @param  fresh: New file system or no
 * @retval None
 */
void mksfs(int fresh)
{

    // reset the pointer of the directory table
    current_position = 0;

    if (fresh)
    {
        D printf("New file system  \n");

        // format virtual disk provided by emulator
        init_fresh_disk(LASTNAME_FIRSTNAME_DISK, BLOCK_SIZE, NUM_BLOCKS);

        // initialize superblock
        superblock_init();

        // set the first block as occupied by the superblock
        force_set_index(0);

        // initialize inode table, directory table & fd table
        for (int i = 0; i < ARRAYSIZE(inode_table); i++)
        {
            inode_table[i].empty = 1;
            inode_table[i].size = 0;
            inode_table[i].indirectPointer = -1;

            for (int j = 0; j < 12; j++)
            {
                inode_table[i].data_ptrs[j] = -1;
            }

            // initialize directory table and fd table
            set_fd(i, 1, -1, NULL, -1);
            set_root_table_entry(i, 1, -1, "");
        }

        // set the first entry of the fd as reserved by the root
        set_fd(0, 0, 0, &inode_table[0], 0);
        inode_table[0].empty = 0; // we set it to be used because we just assigned root to it

        // set the blocks occupied by the inode table
        for (int i = 1; i < ARRAYSIZE(inode_table); i++) // start from 1 because first block for sb
        {
            force_set_index(i);
        }

        // store bitmap itself in bitamp
        force_set_index(NUM_BLOCKS - 1);

        store_in_disk();

        return;
    }

    // initialize directory table and fd table, which resides in memory
    for (int i = 0; i < ARRAYSIZE(fd_table); i++)
    {
        set_fd(i, 1, -1, NULL, -1);
        set_root_table_entry(i, 1, -1, "");
    }

    D printf("Open existing file system \n");

    read_all_from_disk();

    return;
}

/** 
 * @brief  Function used to loop through the directory. Gets the next file name in the directory table.
 * @note   
 * @param  *fname: 
 * @retval 1 if found next filename, 0 otherwise
 */
int sfs_getnextfilename(char *fname)
{

    // this is probably the sketchies method of my assignment, so please do not base yourself on the quality of the code based on only this
    // this overbloated logic was there to account for the fact that I was having trouble with the fuse test and the "ls" command

    // I realized that, whenever I called "ls" twice, the files wouldn't appear the second time
    // I fixed that, but then test2 wouldn't run because of the weird logic the test implements
    // so I was forced to go for the approach below, which basically fits the logic of the "ls" command
    // and the weird logic of the tests together

    // I don't know why it was absolutely necesary to call the while so many times, and that it should return 0
    // even if there are no more files at the end of the table
    // in the case of "ls", that is unnecessary.
    // if we reach the end of our file's list, we simply iterate back

    // check if there are non empty entries later in the table
    // this is to account for the fact that we might remove one file that's in the middle of
    // directory table. in that case, we want to keep moving the current position pointer
    int i = current_position;
    int checker = 0;
    while (i <= ARRAYSIZE(root) && !checker)
    {
        if (strlen(root[i].name) != 0 && strlen(root[i].name) != NULL)
        {
            checker = 1;
        }
        i++;
    }

    // if no non-empty entries are left in the array, or current position exceeds the table, and the entry at the current position is garbage
    // we reset the current pointer
    if (current_position >= ARRAYSIZE(root) || (strlen(root[current_position].name) == 0 && !checker))
    {
        current_position = 0;
        return 0;
    }

    // otherwise, move the current position pointer until we find a non-empty entry
    while (root[current_position].empty == 1)
    {
        current_position++;

        // if we've exceeded our limit, then we reset the pointer
        if (current_position >= ARRAYSIZE(root))
        {
            current_position = 0;
            return 0;
        }
    }

    // copy string!
    strncpy(fname, root[current_position].name, MAX_FILE_NAME);
    current_position++;

    return 1;
}

/** 
 * @brief  Return size of a given file
 * @note   
 * @param  *path: File name
 * @retval size of given file, 0 if error
 */
int sfs_getfilesize(const char *path)
{
    int root_entry = find_predicate(ARRAYSIZE(root), LAMBDA(int _(int i) { return root[i].empty == 0 && strcmp(root[i].name, path) == 0; }));

    int inode_num = (root_entry != -1) ? root[root_entry].num : -1;

    if (inode_num == -1)
    {
        printf(CANNOT_FIND_FILE);
        return 0;
    }

    return inode_table[inode_num].size;
}

/** 
 * @brief  Opens a new file and returns idex that corresponds to the newly opened file in the fd table
 * @note   
 * @param  *name: name of file
 * @retval index of newly opened table, -1 if error
 */
int sfs_fopen(char *name)
{
    D printf("I'm in sfs_fopen\n");

    if (strlen(name) > MAX_FILE_NAME || strlen(name) == 0)
    {
        printf(INVALID_FILE_NAME);
        return -1;
    }

    int root_entry = find_predicate(ARRAYSIZE(root), LAMBDA(int _(int i) { return root[i].empty == 0 && strcmp(root[i].name, name) == 0; }));

    int inode_num = (root_entry != -1) ? root[root_entry].num : -1;

    // if the file exists (begin by this condition because we're more likely to open a file that exists than try to open one that doesn't)
    // this is the power of caching 😏
    if (inode_num != -1)
    {

        int fd_num = find_predicate(ARRAYSIZE(fd_table), LAMBDA(int _(int i) { return fd_table[i].inodeIndex == inode_num; }));

        if (fd_num != -1)
        {
            // if the file is already open, we return it
            D printf("File already open");
            return fd_num;
        }

        // find first non-used entry if the file isn't open
        int empty_fd = find_predicate(ARRAYSIZE(fd_table), LAMBDA(int _(int i) {
                                          return fd_table[i].empty == 1;
                                      }));

        if (empty_fd == -1)
        {
            printf(TABLE_FULL);
            return -1;
        }

        // we open the file
        set_fd(empty_fd, 0, inode_num, &inode_table[inode_num], inode_table[inode_num].size); // set file to append mode

        return empty_fd;
    }

    // find an available spot in the inode table
    int empty_inode = find_predicate(ARRAYSIZE(fd_table), LAMBDA(int _(int i) {
                                         return inode_table[i].empty == 1;
                                     }));

    if (empty_inode == -1)
    {
        printf(INODE_TABLE_FULL);
        return -1;
    }

    inode_table[empty_inode].empty = 0;

    // find an available spot in the dir table
    int empty_root_entry = find_predicate(ARRAYSIZE(fd_table), LAMBDA(int _(int i) {
                                              return root[i].empty == 1;
                                          }));

    if (empty_root_entry == -1)
    {
        printf(ROOT_TABLE_FULL);
        return -1;
    }

    // write file in root directory
    set_root_table_entry(empty_root_entry, 0, empty_inode, name);

    // find an available spot in the fd table
    int empty_fd_table_entry = find_predicate(ARRAYSIZE(fd_table), LAMBDA(int _(int i) {
                                                  return fd_table[i].empty == 1;
                                              }));

    if (empty_fd_table_entry == -1)
    {
        printf(TABLE_FULL);
        return -1;
    }

    // persist our newly open file
    set_fd(empty_fd_table_entry, 0, empty_inode, &inode_table[empty_inode], inode_table[empty_inode].size);
    save_inode_table();

    return empty_fd_table_entry;
}

/** 
 * @brief  Close a file
 * @note   
 * @param  fileID: 
 * @retval -1 on fail, 0 on success (why?)
 */
int sfs_fclose(int fileID)
{
    D printf("I'm in sfs_fclose  \n");

    if (fileID > INODES || fileID < 0 || fd_table[fileID].empty == 1)
    {
        printf(INVALID_FILE_ID);
        return -1;
    }

    // close by clearning the fd table
    set_fd(fileID, 1, -1, NULL, -1);
    return 0;
}

/** 
 * @brief  Reads bytes of data into buf starting from current file pointer
 * @note   
 * @param  fileID: 
 * @param  *buf: 
 * @param  length: 
 * @retval 
 */
int sfs_fread(int fileID, char *buf, int length)
{
    D printf("I'm in sfs_fread");
    if (length <= 0 || fileID >= INODES)
    {
        printf(WRONG_LENGTH);
        return 0;
    }

    if (buf == NULL)
    {
        printf(BUF_NULL);
        return 0;
    }

    if (fd_table[fileID].empty == 1)
    {
        printf(FILE_NOT_OPEN);
        return 0;
    }

    file_descriptor *fd = &fd_table[fileID];
    inode_t *inode = &inode_table[fd->inodeIndex];

    // wrap it around in case of buffer overflow
    // i.e. read something smaller than size of inode, or the whole file
    int wrapped_length = (fd->rwptr + length > inode->size) ? inode->size - fd->rwptr : length;

    // starting block
    int first_block = get_block(fd->rwptr);

    // which block we started at + which blocked we stopped writing at = total # of blocks to read
    int blocks_to_read = get_block(((fd->rwptr) + wrapped_length));

    D printf("BLOCKS TO READ: %i \n", blocks_to_read);

    D printf("Wrapped length: %i\n", wrapped_length);
    char *file_buffer = malloc(wrapped_length); // hold the whole file

    int buff_pointer = 0;

    // now, let us copy block by block
    for (int i = first_block; i <= blocks_to_read; i++)
    {
        D printf("I am inside the reading blocks loop!");
        char *block_buffer = malloc(sb.block_size); // hold each block

        int current_block;

        // if the block to read is in the direct pointer
        if (i < POINTER_SIZE)
        {
            current_block = inode->data_ptrs[i];
        }
        // if the block to read is in the indirect pointer
        else
        {
            int *ind_pointers = malloc(sb.block_size);

            read_blocks(inode->indirectPointer, 1, ind_pointers);
            D printf("Indirect pointers in read?: %i \n", ind_pointers[i - POINTER_SIZE]);
            current_block = ind_pointers[i - POINTER_SIZE];

            free(ind_pointers);
        }

        read_blocks(current_block, 1, block_buffer);

        // compute offsets! (probably the most crucial part of the whole assignment)

        // are we at the last block? in that case, we have to only account for the # of bytes that are necessary within the block
        int last_block_index = (i == blocks_to_read) ? get_bytes((fd->rwptr + wrapped_length)) : sb.block_size;
        // if we're at the first block, we start writing from the position of the pointer since the block has been used before
        // otherwise, we begin reading from the beginning of a block
        int block_start_offset = (i == first_block) ? get_bytes(fd->rwptr) : 0;
        // this only accounts for the case where we only read from one block, there we subtract the start offset from
        // the end offset, to therefore know exactly how many bytes we have to write
        int block_end_offset = (i == first_block) ? last_block_index - get_bytes(fd->rwptr) : last_block_index;

        // we copy the buffer into memory taking into account the computed offsets
        memcpy(&file_buffer[buff_pointer], &block_buffer[block_start_offset], block_end_offset);

        D printf("Did I even get in here 2? \n");
        buff_pointer = buff_pointer + last_block_index - block_start_offset;
        D printf("Line 425: What's buff pointer? %i\n", buff_pointer);

        free(block_buffer);
    }

    // copy the buffer into our whole file buffer
    memcpy(buf, file_buffer, wrapped_length);
    free(file_buffer);

    fd_table[fileID].rwptr = fd_table[fileID].rwptr + wrapped_length;

    D printf("Buffer pointer before exiting?: %i\n", buff_pointer);
    // we return block pointer to ensure that we've written the right amount :)
    return buff_pointer;
}

/** 
 * @brief  Write a given number of bytes of buffered data in buf of an open file
 * @note   
 * @param  fileID: 
 * @param  *buf: 
 * @param  length: 
 * @retval 
 */
int sfs_fwrite(int fileID, const char *buf, int length)
{
    D printf("I'm in sfs_fwrite\n");

    if (fd_table[fileID].empty == 1 || fd_table[fileID].inodeIndex == -1 || (fd_table[fileID].rwptr + length) > MAX_FILE_SIZE)
    {
        printf(INVALID_FILE_NAME);
        return -1;
    }

    file_descriptor *fd = &fd_table[fileID];
    inode_t *inode = &inode_table[fd->inodeIndex];

    // if the file is empty, assign new block to inode
    // dead code?
    if (inode->size <= 0)
    {
        inode->data_ptrs[0] = get_index();
    }

    // same logic as for read
    int first_block = get_block(fd->rwptr);
    int blocks_to_read = get_block((fd->rwptr) + length);
    int buff_pointer = 0;

    for (int i = first_block; i <= blocks_to_read; i++)
    {

        int current_block;
        char *buffer = malloc(BLOCK_SIZE);

        // if we're within the bounds of the direct blocks from the data pointers
        if (i < POINTER_SIZE)
        {
            if (inode->data_ptrs[i] == -1)
            {
                // get a free block
                int free_data = get_index();

                if (free_data == -1)
                {
                    printf(NO_FREE_SPACE);
                    return -1;
                }

                // assign new found block to pointer of inode
                current_block = free_data;
                inode->data_ptrs[i] = current_block;
            }

            current_block = inode->data_ptrs[i];
        }
        else
        {
            //indirect pointers
            int *indirect_pointers = malloc(BLOCK_SIZE);

            D printf("I am writing indirect pointers!\n");

            // if no indirect pointers initialized
            if (inode->indirectPointer == -1)
            {
                int free_indirect_data = get_index();

                if (free_indirect_data == -1)
                {
                    printf(NO_FREE_SPACE);
                    return -1;
                }

                // initialize empty data blocks
                for (int i = 0; i < MAX_INDIRECT_ADDRESSES; i++)
                {
                    indirect_pointers[i] = -1;
                }

                // persist our new indirect pointers at given blocks
                write_blocks(free_indirect_data, 1, indirect_pointers);
                inode->indirectPointer = free_indirect_data;

                // free indirect so that we can reuse it below
                free(indirect_pointers);
                indirect_pointers = malloc(BLOCK_SIZE);
            }

            // if indirect pointers already exist
            read_blocks(inode->indirectPointer, 1, indirect_pointers);

            if (indirect_pointers[i - POINTER_SIZE] == -1)
            {

                // if it's more than the # of indirect pointers we can hold
                if (i - POINTER_SIZE > MAX_INDIRECT_ADDRESSES)
                {
                    printf(NO_FREE_SPACE);
                    return -1;
                }

                int free_indirect_data = get_index();

                if (free_indirect_data == -1)
                {
                    printf(NO_FREE_SPACE);
                    return -1;
                }

                // assign indirect data
                indirect_pointers[i - POINTER_SIZE] = free_indirect_data;

                // persist the newly defined indirect pointers
                write_blocks(inode->indirectPointer, 1, indirect_pointers);

                D printf("Did I assign a pointer to an already initialized array of indirect pointers?");
            }

            current_block = indirect_pointers[i - POINTER_SIZE];
            free(indirect_pointers);
        }

        read_blocks(current_block, 1, buffer);

        // if we're at the first block, we have to start copying from whatever the first bytes of the first block might be, else we're starting from a new block
        // and therefore we have to read from the beginning

        // where we stop writing our file in the last block
        int last_block_index = (i == blocks_to_read) ? get_bytes(fd->rwptr + length) : sb.block_size;
        // where we start writing our file from given the current position of the rw pointer
        // if we start a new block, then we start from its first block
        int block_start_offset = (i == first_block) ? get_bytes(fd->rwptr) : 0;
        // this only accounts for the case where we only write to one block, there we subtract the start offset from
        // the end offset, to therefore know exactly how many bytes we have to write
        int block_end_offset = (i == first_block) ? last_block_index - get_bytes(fd->rwptr) : last_block_index;

        // copy new data to buffer for memory
        memcpy(&buffer[block_start_offset], &buf[buff_pointer], block_end_offset);

        // write into memory
        write_blocks(current_block, 1, buffer);

        buff_pointer = buff_pointer + last_block_index - block_start_offset;
        D printf("Line 425: What's buff pointer? \n", buff_pointer);

        free(buffer);
    }

    // if we wrote to a new block
    inode->size = (inode->size < fd->rwptr + length) ? fd->rwptr + length : inode->size;

    // place pointer to its new position if everything successful
    fd->rwptr = fd->rwptr + length;

    // persist changes in the inode table
    save_inode_table();

    D printf("Line 442: What's buff pointer? \n", buff_pointer);
    return buff_pointer;
}

/** 
 * @brief  Move rwpointer to a given location
 * @note   
 * @param  fileID: 
 * @param  loc: 
 * @retval 
 */
int sfs_fseek(int fileID, int loc)
{

    if (fd_table[fileID].empty == 1 || fd_table[fileID].inode == NULL || fd_table[fileID].inode == -1)
    {
        printf(SEEK_ERROR);
        return -1;
    }

    D printf("Size inode? %i, RW pointer? %i\n", fd_table[fileID].inode->size, fd_table[fileID].rwptr);

    // if the loc is outisde of the boundaries of our file, place the pointer to the end of the file
    // I don't know if this is a the best approach, but when I spoke to Amro he said that this is how
    // I should do it
    fd_table[fileID].rwptr = ((loc + fd_table[fileID].rwptr) > fd_table[fileID].inode->size) ? fd_table[fileID].inode->size : loc;

    return 0;
}

/** 
 * @brief  Rempove file from system
 * @note   
 * @param  *file: 
 * @retval 
 */
int sfs_remove(char *file)
{
    D printf("I'm in sfs_remove\n");

    int inode = -1;
    int i;

    // find to which inode the file belongs based on its name
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

    // clear its root table entry
    set_root_table_entry(i, 1, -1, "\0");

    // clear its inode table entry
    for (int j = 0; j < POINTER_SIZE; j++)
    {
        if (inode_table[inode].data_ptrs[j] != -1)
        {
            rm_index(inode_table[inode].data_ptrs[j]);
            inode_table[inode].data_ptrs[j] = -1;
        }
    }

    // clear indirect pointer as well if present
    if (inode_table[inode].indirectPointer != -1)
    {
        rm_index(inode_table[inode].indirectPointer);
        inode_table[inode].indirectPointer = -1;
    }

    // persist everything in memory
    store_in_disk();

    return 0;
}

#pragma endregion

#pragma region HELPER FUNCTIONS

/** 
 * @brief  Function to persist all of our in-memory structures
 * @note   
 */
int store_in_disk()
{
    save_inode_table();

    if (write_blocks((int)0, (int)1, &sb) == 0 || write_blocks(NUM_BLOCKS - 1, 1, free_bit_map) == 0 || write_blocks((int)sb.inode_table_len + 1, DIR_BLOCKS, root) == 0)
    {
        D printf("Error writing blocks");
        return 0;
    }

    return 1;
}

/** 
 * @brief  Function to read all of our in-memory structures at once
 * @note   
 * @retval 
 */
int read_all_from_disk()
{
    if (read_blocks(0, 1, (void *)&sb) == 0 || read_blocks(NUM_BLOCKS - 1, 1, free_bit_map) == 0 || read_blocks(1, (int)sb.inode_table_len, (void *)inode_table) == 0)
    {
        D printf("Error reading blocks");
        return 0;
    }

    return 1;
}

/** 
 * @brief  Find an entry based on a predicate
 * @note   This can be done because several of our methods are just global variables and it's easy to create a lambda and pass it as parameter somewhere else
 *          You can technically find any index satisfying a predicate. For the purpose of this assignment, it was mostly 
 * @param  size: 
 * @param  (*predicate: 
 * @retval 
 */
int find_predicate(int size, int (*predicate)(int))
{
    for (int i = 0; i < size; i++)
    {
        if (predicate(i))
        {
            return i;
        }
    }

    return -1;
}

/** 
 * @brief  Function to initialize superblock
 * @note   
 * @retval 
 */
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

/** 
 * @brief  Function to change an entry of the fd table
 * @note   
 * @param  fd_index: index in the fd table
 * @param  empty: if it's empty or not
 * @param  inodeIndex: index of the inode
 * @param  *inode: address of the inode
 * @param  rwptr: rw pointer
 * @retval 
 */
int set_fd(int fd_index, int empty, uint64_t inodeIndex, inode_t *inode, uint64_t rwptr)
{
    fd_table[fd_index] = (file_descriptor){.empty = empty, .inodeIndex = inodeIndex, .inode = inode, .rwptr = rwptr};

    return 1;
}

/** 
 * @brief  Function used to change the root table
 * @note   
 * @param  root_index: index inside of the directory table
 * @param  empty: if it's empty or not
 * @param  inode_num: associated inode number
 * @param  name[MAX_FILE_NAME]: filename
 * @retval 
 */
int set_root_table_entry(int root_index, int empty, int inode_num, char name[MAX_FILE_NAME])
{
    root[root_index] = (directory_entry){.num = inode_num, .empty = empty};
    strcpy(root[root_index].name, name);

    write_blocks((int)1 + sb.inode_table_len, DIR_BLOCKS, root);

    return 1;
}

/** 
 * @brief  Wrapper for safely persisting the inode table
 * @note   
 * @retval 
 */
int save_inode_table()
{
    void *buf2 = calloc(1, BLOCK_SIZE * sb.inode_table_len);
    memcpy(buf2, &inode_table, sizeof(inode_table));
    int result = write_blocks(1, sb.inode_table_len, buf2);
    free(buf2);

    return result;
}

/** 
 * @brief  Simple function to get the respective block of an address
 * @note   
 * @param  length: 
 * @retval 
 */
uint64_t get_block(uint64_t length)
{
    return length / BLOCK_SIZE;
}

/** 
 * @brief  Simple function to get the byte inside of a block
 * @note   
 * @param  length: 
 * @retval 
 */
uint64_t get_bytes(uint64_t length)
{
    return length % BLOCK_SIZE;
}

#pragma endregion

#pragma region BITMAP REGION

/** 
 * @brief  Set an entry of the bitmap as used
 * @note   Provided by assignment
 * @param  index: 
 * @retval None
 */
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

/** 
 * @brief  Get a free space in the bitmap
 * @note   Provided by assignment
 * @retval 
 */
uint32_t get_index()
{
    uint32_t i = 0;

    // find the first section with a free bit
    // let's ignore overflow for now...
    while (free_bit_map[i] == 0 && i < 1023)
    {
        i++;
    }

    if (i >= (BLOCK_SIZE - 1))
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

/** 
 * @brief  Free a given block
 * @note   Provided by assignment
 * @param  index:
 * @retval None
 */
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