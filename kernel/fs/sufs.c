/**
 * SUFS (Simple Unix File System) implementation.
 * A simplified version of the Unix File System created by me.
 * 
 * @author Samuel Pires
 */

#include <stdio.h>
#include <string.h>

#include <kernel/fs/sufs.h>
#include <kernel/fs/fs.h>
#include <kernel/utils.h>
#include <kernel/mm/mm.h>
#include <kernel/ds/bitmap.h>
#include <kernel/system.h>


#define SUPERBLOCK_SECTOR	(SUFS_SUPERBLOCK_OFFSET / dev->logical_sector_size)

#define time(NULL) 0 // TODO

ata_dev_t* dev;
struct sufs_superblock sb;
struct sufs_dinode root_inode;
void *block_buf, *map_block_buf, *indirect_block_buf;


static void write_superblock();
static void write_inode(const struct sufs_dinode* inode);
static uint32_t search_dir(const struct sufs_dinode* dir_inode, const char* name);
static int write_to_dir(struct sufs_dinode* dir_inode, uint32_t inum, const char* name);
static int remove_from_dir(struct sufs_dinode* dir_inode, uint32_t inum);

static void remove_dir_dblock(struct sufs_dinode* dir_inode, uint32_t idx);
static uint32_t get_data_block(const struct sufs_dinode* inode, uint32_t idx);
static uint32_t alloc_data_block(struct sufs_dinode* inode, uint32_t idx);

static int create_file(char* path, int mode);
static int delete_file(char* path, bool is_dir);

static struct sufs_dinode* iget(uint32_t inum, struct sufs_dinode* iout);
static void iput(struct sufs_dinode* inode);
static struct sufs_dinode* namei(const char* path, struct sufs_dinode* iout);
static struct sufs_dinode* ialloc(struct sufs_dinode* iout);
static void ifree(uint32_t inum);

static uint32_t dballoc(void);
static void dbfree(uint32_t dblock);

#define dev_read_sector(_buf,_lba)			ata_read(dev, _buf, _lba, 1)
#define dev_write_sector(_buf,_lba) 		ata_write(dev, _buf, _lba, 1)
#define dev_read_block(_buf,_block_idx)		ata_read(dev, _buf, (_block_idx) * sb.sb_secpb, sb.sb_secpb)
#define dev_write_block(_buf,_block_idx)	ata_write(dev, _buf, (_block_idx) * sb.sb_secpb, sb.sb_secpb)


/* Header Implementation */

#define PRINT_AND_RET(msg)	({ printf(msg); return; })

void sufs_mount(ata_dev_t* _dev)
{
	dev = _dev;
	if (dev->logical_sector_size < 512)
		PRINT_AND_RET("Sectors size smaller than 512 bytes not supported\n");
	if (dev->logical_sector_size != 512)	// TODO: support other sector sizes
		PRINT_AND_RET("Sector size different from 512 bytes not supported\n");

	dev_read_sector(&sb, SUPERBLOCK_SECTOR);

	if (sb.sb_magic != SUFS_MAGIC)
		PRINT_AND_RET("Invalid magic number\n");

	if (sb.sb_block_size < MAX(SUFS_BLOCK_SIZE_MIN, dev->logical_sector_size) ||
			sb.sb_block_size > SUFS_BLOCK_SIZE_MAX || !IS_POWER_OF_2(sb.sb_block_size))
		PRINT_AND_RET("Invalid block size\n");

	uint32_t sb_block = SUFS_SUPERBLOCK_OFFSET / sb.sb_block_size;
	if (sb.sb_block_count > ATA_NUM_SECTORS(dev) / (sb.sb_block_size / dev->logical_sector_size))
		PRINT_AND_RET("Invalid block count\n");

	if (sb.sb_iblock_count == 0)
		PRINT_AND_RET("Invalid inode block count\n");
	if (sb.sb_dblock_count == 0)
		PRINT_AND_RET("Invalid data block count\n");
	if (sb.sb_inode_count > sb.sb_iblock_count * sb.sb_inopb)
		PRINT_AND_RET("Invalid inode count\n");
	if (sb.sb_free_inode_count >= sb.sb_inode_count)
		PRINT_AND_RET("Invalid free inode count\n");
	if (sb.sb_free_dblock_count >= sb.sb_dblock_count)
		PRINT_AND_RET("Invalid free data block count\n");

	if (sb.sb_block_count < sb_block + 1 + sb.sb_inode_map_bsize +
			sb.sb_dblock_map_bsize + sb.sb_iblock_count + sb.sb_dblock_count)
		PRINT_AND_RET("Invalid block count\n");

	if (sb.sb_inode_map_bsize == 0 ||
			sb.sb_inode_map_bsize * sb.sb_mapentpb < sb.sb_inode_count)
		PRINT_AND_RET("Invalid inode map block size\n");
	if (sb.sb_dblock_map_bsize == 0 ||
			sb.sb_dblock_map_bsize * sb.sb_mapentpb < sb.sb_dblock_count)
		PRINT_AND_RET("Invalid data block map block size\n");

	if (sb.sb_inode_map_boff < sb_block + 1 ||
			sb.sb_inode_map_boff + sb.sb_inode_map_bsize > sb.sb_dblock_map_boff)
		PRINT_AND_RET("Invalid inode map block offset\n");
	if (sb.sb_dblock_map_boff < sb.sb_inode_map_boff + sb.sb_inode_map_bsize ||
			sb.sb_dblock_map_boff + sb.sb_dblock_map_bsize > sb.sb_inodes_boff)
		PRINT_AND_RET("Invalid data block map block offset\n");
	if (sb.sb_inodes_boff < sb.sb_dblock_map_boff + sb.sb_dblock_map_bsize ||
			sb.sb_inodes_boff + sb.sb_iblock_count > sb.sb_dblocks_boff)
		PRINT_AND_RET("Invalid inode region block offset\n");
	if (sb.sb_dblocks_boff < sb.sb_inodes_boff + sb.sb_iblock_count ||
			sb.sb_dblocks_boff + sb.sb_dblock_count > sb.sb_block_count)
		PRINT_AND_RET("Invalid data block region block offset\n");

	if (sb.sb_secpb != sb.sb_block_size / dev->logical_sector_size)
		PRINT_AND_RET("Invalid number of sectors per block\n");
	if (sb.sb_nindir != sb.sb_block_size / sizeof(sufs_daddr_t))
		PRINT_AND_RET("Invalid number of entries per indirect block\n");
	if (sb.sb_inopb != sb.sb_block_size / sizeof(struct sufs_dinode))
		PRINT_AND_RET("Invalid number of inodes per block\n");
	if (sb.sb_mapentpb != sb.sb_block_size * 8)
		PRINT_AND_RET("Invalid number of map entries per block\n");
	if (sb.sb_dentpb != sb.sb_block_size / sizeof(struct sufs_dentry))
		PRINT_AND_RET("Invalid number of directory entries per block\n");

	if (sb.sb_roodir_inum == 0 || sb.sb_roodir_inum >= sb.sb_inode_count)
		PRINT_AND_RET("Invalid root directory inode number\n");

	uint64_t expected_maxfilesize = SUFS_NDADDR;
	for (int i = 0; i < SUFS_NIADDR; i++) {
		uint32_t extra = sb.sb_nindir;
		for (int j = 0; j < i; j++)
			extra *= sb.sb_nindir;
		expected_maxfilesize += extra;
	}
	expected_maxfilesize *= sb.sb_block_size;

	if (sb.sb_maxfilesize != expected_maxfilesize) {
		printf("Unexpected maximum file size parameter found. Value corrected.\n");
		sb.sb_maxfilesize = expected_maxfilesize;
		write_superblock();
	}

	block_buf = kmalloc(sb.sb_block_size);
	map_block_buf = kmalloc(sb.sb_block_size);
	indirect_block_buf = kmalloc(sb.sb_block_size);
	iget(sb.sb_roodir_inum, &root_inode);
}

void sufs_unmount(void)
{
	kfree(block_buf);
	kfree(map_block_buf);
	kfree(indirect_block_buf);
}


struct sufs_dinode* sufs_open(char* path)
{
	struct sufs_dinode* inode = namei(path, NULL);
	if (inode == NULL) {
		fs_errno = ENOENT;
		return NULL;
	}

	return inode;
}

int sufs_close(struct sufs_dinode* inode)
{
	iput(inode);
	return 0;
}

ssize_t sufs_write(struct sufs_dinode* inode, void* data, uint64_t offset, size_t nbytes)
{
	if (inode->di_mode & IFDIR) {
		fs_errno = EISDIR;
		return -1;
	}

	if (offset + nbytes > sb.sb_maxfilesize) {
		fs_errno = EFBIG;
		return -1;
	}

	uint64_t end_offset = offset + nbytes;
	uint32_t end_block_idx = end_offset / sb.sb_block_size;

	if (end_block_idx > SUFS_NDADDR)	// TODO
		PANIC("Indirect blocks not supported yet");

	uint32_t i = offset / sb.sb_block_size;
	size_t to_write;
	uint32_t block_idx;
	size_t data_offset = 0;

	// Write to the first block if offset is not block-aligned
	if (offset % sb.sb_block_size > 0) {
		uint32_t block_offset = offset % sb.sb_block_size;
		to_write = MIN(nbytes, sb.sb_block_size - block_offset);
		block_idx = i < inode->di_nblocks ?
					get_data_block(inode, i) : alloc_data_block(inode, i);
		if (block_idx == 0) {
			fs_errno = ENOSPC;
			return -1;
		}

		dev_read_block(block_buf, block_idx);
		memcpy(block_buf + block_offset, data, to_write);
		dev_write_block(block_buf, block_idx);

		i++;
		nbytes -= to_write;
		data_offset += to_write;
	}

	// Write to the remaining blocks
	for (; i < end_block_idx; i++) {
		block_idx = i < inode->di_nblocks ?
					get_data_block(inode, i) : alloc_data_block(inode, i);
		if (block_idx == 0) {
			fs_errno = ENOSPC;
			return data_offset > 0 ? (ssize_t)data_offset : -1;
		}

		memcpy(block_buf, data + data_offset, sb.sb_block_size);
		dev_write_block(block_buf, block_idx);

		nbytes -= sb.sb_block_size;
		data_offset += sb.sb_block_size;
	}

	// Write to the last block if last byte is not block-aligned
	if (nbytes > 0) {
		block_idx = i < inode->di_nblocks ?
					get_data_block(inode, end_block_idx) :
					alloc_data_block(inode, end_block_idx);
		dev_read_block(block_buf, end_block_idx);
		memcpy(block_buf, data + data_offset, nbytes);
		dev_write_block(block_buf, block_idx);

		data_offset += nbytes;
		nbytes = 0;
	}

	inode->di_size = MAX(inode->di_size, end_offset);
	inode->di_mtime = time(NULL);
	write_inode(inode);

	return data_offset;
}

ssize_t sufs_read(struct sufs_dinode* inode, void* buf, uint64_t offset, size_t nbytes)
{
	if (offset >= inode->di_size) {
		fs_errno = EINVAL;
		return -1;
	}

	nbytes = MIN(nbytes, inode->di_size - offset);

	uint64_t end_offset = offset + nbytes;
	uint32_t end_block_idx = end_offset / sb.sb_block_size;

	uint32_t i = offset / sb.sb_block_size;
	size_t to_read;
	uint32_t block_idx;
	size_t buf_offset = 0;

	// Read the first block if offset is not block-aligned
	if (offset % sb.sb_block_size > 0) {
		uint32_t block_offset = offset % sb.sb_block_size;
		to_read = MIN(nbytes, sb.sb_block_size - block_offset);
		block_idx = get_data_block(inode, i);

		dev_read_block(block_buf, block_idx);
		memcpy(buf, block_buf + block_offset, to_read);

		i++;
		nbytes -= to_read;
		buf_offset += to_read;
	}

	// Read the remaining blocks
	for (; i < end_block_idx; i++) {
		block_idx = get_data_block(inode, i);
		to_read = MIN(nbytes, sb.sb_block_size);

		dev_read_block(block_buf, block_idx);
		memcpy(buf + buf_offset, block_buf, to_read);

		nbytes -= to_read;
		buf_offset += to_read;
	}

	// Read the last block if last byte is not block-aligned
	if (nbytes > 0) {
		block_idx = get_data_block(inode, end_block_idx);
		dev_read_block(block_buf, block_idx);
		memcpy(buf + buf_offset, block_buf, nbytes);

		buf_offset += nbytes;
		nbytes = 0;
	}

	return buf_offset;
}

int sufs_create(char* path)
{
	return create_file(path, (IFREG | S_IRWXU | S_IRWXG | S_IRWXO));
}

int sufs_unlink(char* path)
{
	return delete_file(path, false);
}

int sufs_mkdir(char* path)
{
	return create_file(path, (IFDIR | S_IRWXU | S_IRWXG | S_IRWXO));
}

int sufs_rmdir(char* path)
{
	if (!strcmp(path, ROOT_DIR)) {
		fs_errno = EINVAL;
		return -1;
	}

	return delete_file(path, true);
}


#ifdef SUFS_DEBUG
#include "../data_structures/stack/stack.h"
void sufs_dump_dir_tree(void)
{
	struct dir {
		struct sufs_dinode inode;
		char name[SUFS_MAX_FILENAME_LEN];
	};

	struct dir* curr_dir = kmalloc(sizeof(struct dir));
	memcpy(&curr_dir->inode, &root_inode, sizeof(struct sufs_dinode));
	curr_dir->name[0] = '\0';

	Stack dir_stack;
	stack_init(&dir_stack);
	stack_push(&dir_stack, curr_dir);

	void* my_block_buf = kmalloc(sizeof(struct sufs_dentry), sb.sb_block_size);

	struct sufs_dinode inode;
	while (dir_stack.size > 0) {
		curr_dir = stack_pop(&dir_stack);

		for (uint32_t i = 0; i < curr_dir->inode.di_nblocks; i++) {
			dev_read_block(my_block_buf, get_data_block(inode, i, false););

			struct sufs_dentry* dentries = my_block_buf;
			uint32_t sb.sb_dentpb = sb.sb_block_size / sizeof(struct sufs_dentry);
			for (uint32_t j = 0; j < sb.sb_dentpb; j++) {
				if (dentries[j].de_inum > 0) {
					printf("%s/%s\n", curr_dir->name, dentries[j].de_name);
					iget(dentries[j].de_inum, &inode);
					if (j >= 2 && inode.di_mode & IFDIR) {
						struct dir* new_dir = kmalloc(sizeof(struct dir));
						memcpy(&new_dir->inode, &inode, sizeof(struct sufs_dinode));
						if (snprintf(new_dir->name, SUFS_MAX_FILENAME_LEN, "%s/%s", curr_dir->name, dentries[j].de_name) >= SUFS_MAX_FILENAME_LEN) {
							printf("Error: child name exceeds max length. Stopping.\n");
							goto ret;
						}
						stack_push(&dir_stack, new_dir);
					}
				}
			}
		}

		kfree(curr_dir);
	}

	ret:
	kfree(my_block_buf);
	stack_free(&dir_stack, false);
}
#endif


/* Helper Functions */

/**
 * Writes the superblock to disk.
 */
static void write_superblock(void)
{
	sb.sb_time = time(NULL);
	dev_write_sector(&sb, SUPERBLOCK_SECTOR);
}

/**
 * Writes an inode to disk.
 * 
 * @param inode the inode to write
 */
static void write_inode(const struct sufs_dinode* inode)
{
	uint32_t inode_block = sb.sb_inodes_boff + inode->di_inumber / sb.sb_inopb;
	uint32_t block_offset = (inode->di_inumber % sb.sb_inopb) * sizeof(struct sufs_dinode);

	dev_read_block(block_buf, inode_block);
	memcpy(block_buf + block_offset, inode, sizeof(struct sufs_dinode));
	dev_write_block(block_buf, inode_block);
}

/**
 * Searches a directory for a file and returns its inode number.
 * 
 * @param dir_inode the inode of the directory
 * @param name the name of the file
 * 
 * @return the inode number of the file or 0 if it wasn't found
 */
static uint32_t search_dir(const struct sufs_dinode* dir_inode, const char* name)
{
	for (uint32_t i = 0; i < dir_inode->di_nblocks; i++) {
		dev_read_block(block_buf, get_data_block(dir_inode, i));

		struct sufs_dentry* dentries = block_buf;
		for (uint32_t j = 0; j < sb.sb_dentpb; j++) {
			if (dentries[j].de_inum > 0 && !strncmp(dentries[j].de_name, name, SUFS_MAX_FILENAME_LEN - 1))
				return dentries[j].de_inum;
		}
	}

	return 0;
}

/**
 * Writes a directory entry to a directory.
 * 
 * Does not write the directory inode to disk.
 * 
 * Sets fs_errno on failure.
 * 
 * @param dir_inode the inode of the directory
 * @param inum the inode number of the entry to write
 * @param name a null-terminated string containing the name of the entry to write
 * 
 * @return 0 on success, -1 on failure
 */
static int write_to_dir(struct sufs_dinode* dir_inode, uint32_t inum, const char* name)
{
	uint32_t block_idx;
	for (uint32_t i = 0; i < dir_inode->di_nblocks; i++) {
		block_idx = get_data_block(dir_inode, i);
		dev_read_block(block_buf, block_idx);

		struct sufs_dentry* dentries = block_buf;
		for (uint32_t j = 0; j < sb.sb_dentpb; j++) {
			if (dentries[j].de_inum == 0) {
				dentries[j].de_inum = inum;
				strncpy(dentries[j].de_name, name, SUFS_MAX_FILENAME_LEN - 1);
				dentries[j].de_name[SUFS_MAX_FILENAME_LEN - 1] = '\0';

				dev_write_block(block_buf, block_idx);
				return 0;
			}
		}
	}

	if (dir_inode->di_nblocks >= sb.sb_maxfilesize / sb.sb_block_size) {
		fs_errno = EFBIG;
		return -1;
	}

	// Allocate a new block
	block_idx = alloc_data_block(dir_inode, dir_inode->di_nblocks);

	memset(block_buf, 0, sb.sb_block_size);
	struct sufs_dentry* dentry = block_buf;
	dentry->de_inum = inum;
	strncpy(dentry->de_name, name, SUFS_MAX_FILENAME_LEN - 1);
	dentry->de_name[SUFS_MAX_FILENAME_LEN - 1] = '\0';

	dev_write_block(block_buf, block_idx);
	return 0;
}

/**
 * Removes a directory entry from a directory.
 * 
 * Does not write the directory inode to disk.
 * 
 * @param dir_inode the inode of the directory
 * @param inum the inode number of the entry to remove
 * 
 * @return 0 on success, -1 on failure
 */
static int remove_from_dir(struct sufs_dinode* dir_inode, uint32_t inum)
{
	uint32_t i, block_idx = 0;
	struct sufs_dentry* dentries = block_buf;

	for (i = 0; i < dir_inode->di_nblocks; i++) {
		block_idx = get_data_block(dir_inode, i);
		dev_read_block(block_buf, block_idx);

		for (uint32_t j = 0; j < sb.sb_dentpb; j++) {
			if (dentries[j].de_inum == inum) {
				dentries[j].de_inum = 0;

				if (i > 0) {	// kfree block if it becomes empty
					for (j = 0; j < sb.sb_dentpb; j++)
						if (dentries[j].de_inum > 0)
							goto write_ret;

					dbfree(block_idx);
					remove_dir_dblock(dir_inode, i);
					return 0;
				}
				
				write_ret:
				dev_write_block(block_buf, block_idx);
				return 0;
			}
		}
	}

	for (uint32_t j = 0; j < sb.sb_dentpb; j++) {
		if (dentries[j].de_inum ) {
			dentries[j].de_inum = 0;
				
			dev_write_block(block_buf, block_idx);
			return 0;
		}
	}

	return -1;
}


/**
 * Removes a data block from a directory inode.
 * 
 * Updates the inode but does not write it to disk.
 * 
 * @param dir_inode the directory inode
 * @param idx the index of the data block in the inode
 */
static void remove_dir_dblock(struct sufs_dinode* dir_inode, uint32_t idx)
{
	// If it only had direct blocks, adjust the array
	if (dir_inode->di_nblocks < SUFS_NDADDR) {
		dbfree(get_data_block(dir_inode, idx));
		memmove(dir_inode->di_db + idx, dir_inode->di_db + idx + 1,
				(dir_inode->di_nblocks - idx - 1) * sizeof(uint32_t));
		dir_inode->di_db[dir_inode->di_nblocks--] = 0;
		return;
	}

	// Otherwise, copy the last block's contents to the one being removed
	uint32_t last_block_idx = get_data_block(dir_inode, dir_inode->di_nblocks - 1);
	if (idx < dir_inode->di_nblocks - 1) {
		dev_read_block(block_buf, last_block_idx);
		dev_write_block(block_buf, get_data_block(dir_inode, idx));
	}
	dbfree(last_block_idx);

	// TODO: zero out the last block in indirect blocks
}

/**
 * Returns the block number of a data block in an inode.
 * 
 * @param inode the inode
 * @param idx the index of the data block in the inode
 * 
 * @return the block number or 0 if it doesn't exist
 */
static uint32_t get_data_block(const struct sufs_dinode* inode, uint32_t idx)
{
	if (idx >= SUFS_NDADDR) {	// TODO: don't forget to use indirect_block_buf
		PANIC("Indirect blocks not supported yet");
		fs_errno = EFBIG;
		return 0;
	}

	return inode->di_db[idx];
}

/**
 * Allocates a new data block for an inode.
 * 
 * Updates the inode but does not write it to disk.
 * 
 * @param inode the inode
 * @param idx the index of the data block in the inode
 * 
 * @return the block number or 0 if allocation failed
 */
static uint32_t alloc_data_block(struct sufs_dinode* inode, uint32_t idx)
{
	if (idx >= SUFS_NDADDR) {	// TODO: don't forget to use indirect_block_buf
		PANIC("Indirect blocks not supported yet");
		fs_errno = EFBIG;
		return 0;
	}

	uint32_t block_idx = dballoc();
	if (block_idx == 0)
		return 0;

	inode->di_nblocks++;
	inode->di_db[idx] = block_idx;
	return block_idx;
}

/**
 * Creates a file in the file system.
 * 
 * Sets fs_errno on failure.
 * 
 * @param path the path of the file
 * @param mode the mode of the file
 * 
 * @return 0 on success, -1 on failure
 */
static int create_file(char* path, int mode)
{
	char* name = strrchr(path, PATH_SEPARATOR) + 1;
	if (strlen(name) >= SUFS_MAX_FILENAME_LEN) {
		fs_errno = ENAMETOOLONG;
		return -1;
	}

	name[-1] = '\0';

	// Check if the parent directory exists
	struct sufs_dinode iparent;
	if (namei(path, &iparent) == NULL) {
		fs_errno = ENOENT;
		return -1;
	}

	name[-1] = PATH_SEPARATOR;

	// Check if parent is a directory
	if (!(iparent.di_mode & IFDIR)) {
		fs_errno = ENOTDIR;
		return -1;
	}

	// Check if the file already exists
	if (search_dir(&iparent, name) > 0) {
		fs_errno = EEXIST;
		return -1;
	}

	// Allocate an inode
	struct sufs_dinode inode;
	if (ialloc(&inode) == NULL) {
		fs_errno = ENOSPC;
		return -1;
	}

	if (write_to_dir(&iparent, inode.di_inumber, name) < 0) {
		ifree(inode.di_inumber);
		return -1;
	}

	inode.di_mode = mode;
	inode.di_uid = 0;
	inode.di_gid = 0;
	inode.di_nlink = 1;
	inode.di_ctime = time(NULL);
	inode.di_atime = inode.di_ctime;
	inode.di_mtime = inode.di_ctime;
	inode.di_itime = inode.di_ctime;

	// If a directory, write the "." and ".." entries
	if (mode & IFDIR) {
		inode.di_nlink++;
		inode.di_nblocks = 1;
		inode.di_db[0] = dballoc();
		if (inode.di_db[0] == 0) {
			ifree(inode.di_inumber);
			fs_errno = ENOSPC;
			return -1;
		}

		memset(block_buf, 0, sb.sb_block_size);

		struct sufs_dentry* de = block_buf;
		de->de_inum = inode.di_inumber;
		strcpy(de->de_name, ".");
		de++;
		de->de_inum = iparent.di_inumber;
		strcpy(de->de_name, "..");

		dev_write_block(block_buf, inode.di_db[0]);
	}

	write_inode(&inode);
	write_inode(&iparent);
	return 0;
}

/**
 * Removes a file from the file system.
 * 
 * Sets fs_errno on failure.
 * 
 * @param path the path of the file
 * @param is_dir whether the file is a directory
 * 
 * @return 0 on success, -1 on failure
 */
static int delete_file(char* path, bool is_dir)
{
	char* name = strrchr(path, PATH_SEPARATOR) + 1;
	if (strlen(name) >= SUFS_MAX_FILENAME_LEN) {
		fs_errno = ENAMETOOLONG;
		return -1;
	}

	name[-1] = '\0';

	// Check if parent directory exists
	struct sufs_dinode iparent;
	if (namei(path, &iparent) == NULL) {
		fs_errno = ENOENT;
		return -1;
	}

	name[-1] = PATH_SEPARATOR;

	// Check if file exists
	uint32_t inum = search_dir(&iparent, name);
	if (inum == 0) {
		fs_errno = ENOENT;
		return -1;
	}

	// Check if file is a different type than expected
	struct sufs_dinode inode;
	iget(inum, &inode);
	if (!is_dir && inode.di_mode & IFDIR) {
		fs_errno = EISDIR;
		return -1;
	} else if (is_dir && !(inode.di_mode & IFDIR)) {
		fs_errno = ENOTDIR;
		return -1;
	}

	// If a directory, check if it's empty
	if (is_dir) {
		if (inode.di_nblocks > 1) {
			fs_errno = ENOTEMPTY;
			return -1;
		}

		dev_read_block(block_buf, inode.di_db[0]);
		struct sufs_dentry* dentries = block_buf;
		for (uint32_t i = 2; i < sb.sb_dentpb; i++) {
			if (dentries[i].de_inum > 0) {
				fs_errno = ENOTEMPTY;
				return -1;
			}
		}
	}

	for (uint32_t i = 0; i < inode.di_nblocks; i++)
		dbfree(get_data_block(&inode, i));
	// TODO: more efficient approach to free indirect blocks

	remove_from_dir(&iparent, inode.di_inumber);
	write_inode(&iparent);
	ifree(inum);
	return 0;
}


/**
 * Retrieves an inode from disk.
 * 
 * If iout is NULL, an inode is allocated.
 * 
 * Original: Allocates an incore copy of an inode.
 * 
 * @param inum the inode number
 * @param iout a pointer to hold the inode (can be NULL)
 * 
 * @return the allocated inode
 */
static struct sufs_dinode* iget(uint32_t inum, struct sufs_dinode* iout)
{
	uint32_t inode_block = sb.sb_inodes_boff + inum / sb.sb_inopb;
	uint32_t block_offset = (inum % sb.sb_inopb) * sizeof(struct sufs_dinode);

	if (iout == NULL)
		iout = kmalloc(sizeof(struct sufs_dinode));

	dev_read_block(block_buf, inode_block);
	memcpy(iout, block_buf + block_offset, sizeof(struct sufs_dinode));

	return iout;
}

/**
 * Writes an inode to disk and frees it.
 * 
 * Original: Releases an inode.
 * 
 * @param inode the inode to write to disk
 */
static void iput(struct sufs_dinode* inode)
{
	write_inode(inode);
	kfree(inode);
}

/**
 * Converts a path to an inode.
 * 
 * If iout is NULL, an inode is allocated.
 * 
 * @param path a null-terminated string containing the path to be converted
 * @param iout a pointer to hold the inode (can be NULL)
 * 
 * 
 * @return a pointer to the inode or NULL if the file doesn't exist
 */
static struct sufs_dinode* namei(const char* path, struct sufs_dinode* iout)
{
	struct sufs_dinode curr_inode = root_inode;

	const char* name = path;
	if (!strcmp(name, ROOT_DIR) || !strcmp(name, ""))
		name = NULL;

	while (name != NULL) {
		name++;

		char* next = strchr(name, PATH_SEPARATOR);
		if (next != NULL)
			*next = '\0';

		uint32_t inum = search_dir(&curr_inode, name);
		if (inum == 0)
			return NULL;

		iget(inum, &curr_inode);

		if (next != NULL)
			*next = PATH_SEPARATOR;
		name = next;
	}

	if (iout == NULL)
		iout = kmalloc(sizeof(struct sufs_dinode));

	memcpy(iout, &curr_inode, sizeof(struct sufs_dinode));
	return iout;
}

/**
 * Allocates an inode.
 * 
 * The returned inode is zeroed except for the inode number.
 * 
 * @param iout a pointer to hold the inode (can be NULL)
 * 
 * @return a pointer to the inode or NULL if no inodes are available
 */
static struct sufs_dinode* ialloc(struct sufs_dinode* iout)
{
	uint32_t inum = 0;

	uint32_t i;
	for (i = 0; i < sb.sb_inode_map_bsize; i++) {
		dev_read_block(map_block_buf, sb.sb_inode_map_boff + i);
		inum = bitmap_alloc(map_block_buf, sb.sb_block_size, 1, NULL);
		if (inum > 0)
			break;
	}

	if (inum == 0)
		return NULL;

	inum += i * sb.sb_mapentpb;
	if (inum >= sb.sb_inode_count)
		return NULL;

	dev_write_block(map_block_buf, sb.sb_inode_map_boff + i);

	sb.sb_free_inode_count--;
	write_superblock();

	if (iout == NULL)
		iout = kmalloc(sizeof(struct sufs_dinode));

	memset(iout, 0, sizeof(struct sufs_dinode));
	iout->di_inumber = inum;
	return iout;
}

/**
 * Frees an inode.
 * 
 * @param inum the number of the inode to be freed
 */
static void ifree(uint32_t inum)
{
	uint32_t block_index = sb.sb_inode_map_boff + (inum / sb.sb_mapentpb);
	uint32_t bit_offset = inum % sb.sb_mapentpb;

	dev_read_block(map_block_buf, block_index);
	bitmap_free(map_block_buf, bit_offset, 1);
	dev_write_block(map_block_buf, block_index);

	sb.sb_free_inode_count++;
	write_superblock();
}


/**
 * Allocates a data block.
 * 
 * @return the index of the allocated block or 0 if no blocks are available
 */
static uint32_t dballoc(void)
{
	uint32_t block_index = 0;

	uint32_t i;
	for (i = 0; i < sb.sb_dblock_map_bsize; i++) {
		dev_read_block(map_block_buf, sb.sb_dblock_map_boff + i);
		block_index = bitmap_alloc(map_block_buf, sb.sb_block_size, 1, NULL);
		if (block_index > 0)
			break;
	}

	if (block_index == 0)
		return 0;

	block_index += sb.sb_dblocks_boff + (i * sb.sb_mapentpb);
	if (block_index >= sb.sb_dblock_count)
		return 0;

	dev_write_block(map_block_buf, sb.sb_dblock_map_boff + i);

	sb.sb_free_dblock_count--;
	write_superblock();

	return block_index;
}

/**
 * Frees a data block.
 * 
 * @param block_idx the index of the block to be freed
 */
static void dbfree(uint32_t block_idx)
{
	uint32_t dblock_idx = block_idx - sb.sb_dblocks_boff;
	uint32_t map_block = sb.sb_dblock_map_boff + (dblock_idx / sb.sb_mapentpb);
	uint32_t bit_offset = dblock_idx % sb.sb_mapentpb;

	dev_read_block(map_block_buf, map_block);
	bitmap_free(map_block_buf, bit_offset, 1);
	dev_write_block(map_block_buf, map_block);

	sb.sb_free_dblock_count++;
	write_superblock();
}
