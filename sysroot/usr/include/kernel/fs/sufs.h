#pragma once

#include <stdint.h>

// TODO: Remove this when no programs are run from host pre-compilation
#ifdef __MYOS__
#include <kernel/fs/fs.h>
#include <kernel/arch/i386/drivers/ata.h>
#else
typedef void ata_dev_t;
#endif


#define SUFS_BOOTBLOCK_SIZE			(1 << 10) /* 1KB */
#define SUFS_SUPERBLOCK_OFFSET		SUFS_BOOTBLOCK_SIZE

#define SUFS_BLOCK_SIZE_MIN			512U
#define SUFS_BLOCK_SIZE_MAX			((1 << 10) * 64) /* 64KB */

#define SUFS_MAGIC			0xB2708A7E
#define SUFS_VOL_NAME_LEN	16U

struct sufs_superblock {
	uint32_t sb_magic;				/* magic number */

	uint32_t sb_block_size;			/* block size */
	uint32_t sb_block_count;		/* fs size (in blocks) */

	uint32_t sb_inode_count;		/* number of inodes */
	uint32_t sb_iblock_count;		/* number of inode blocks */
	uint32_t sb_dblock_count;		/* number of data blocks */
	uint32_t sb_free_inode_count;	/* number of free inodes */
	uint32_t sb_free_dblock_count;	/* number of free data blocks */

	uint32_t sb_inode_map_bsize;	/* inode map size (in blocks) */
	uint32_t sb_dblock_map_bsize;	/* data block map size (in blocks) */

	/* these fields can be computed from the others */
	uint32_t sb_inode_map_boff;		/* offset of inode map (in blocks) */
	uint32_t sb_dblock_map_boff;	/* offset of data block map (in blocks) */
	uint32_t sb_inodes_boff;		/* offset of inode region (in blocks) */
	uint32_t sb_dblocks_boff;		/* offset of data block region (in blocks) */

	uint32_t sb_secpb;				/* number of sectors per block */
	uint32_t sb_nindir;				/* number of entries in an indirect block */
	uint32_t sb_inopb;				/* number of inodes per block */
	uint32_t sb_mapentpb;			/* number of map entries per block */
	uint32_t sb_dentpb;				/* number of directory entries per block */

	uint32_t sb_roodir_inum;		/* inode number of root directory */

	uint64_t sb_time;				/* last time written */
	uint64_t sb_maxfilesize;		/* maximum representable file size */

	char sb_vol_name[SUFS_VOL_NAME_LEN];

	uint8_t sb_spare2[400];
};


#define	SUFS_NDADDR	12	/* Direct addresses in inode. */
#define	SUFS_NIADDR	3	/* Indirect addresses in inode. */

typedef uint32_t sufs_daddr_t;

struct sufs_dinode {
	uint64_t di_size;			/* File size */

	uint64_t di_ctime;			/* Creation time. */
	uint64_t di_atime;			/* Last access time. */
	uint64_t di_mtime;			/* Last modified time. */
	uint64_t di_itime;			/* Last inode change time */

	sufs_daddr_t di_db[SUFS_NDADDR];	/* Direct disk blocks */
	sufs_daddr_t di_ib[SUFS_NIADDR];	/* Indirect disk blocks */

	uint32_t di_inumber;		/* Inode number */
	uint32_t di_nblocks;		/* Number of blocks actually held */

	uint32_t di_uid;			/* User owner */
	uint32_t di_gid;			/* Group owner */

	uint32_t di_flags;			/* Status flags */

	uint16_t di_mode;			/* IFMT and permissions */
	uint16_t di_nlink;			/* File link count */

	uint32_t di_spare;
};


/* Maximum filename length, including null terminator */
#define SUFS_MAX_FILENAME_LEN 28

struct sufs_dentry {
	uint32_t de_inum;						/* inode number */
	char de_name[SUFS_MAX_FILENAME_LEN];	/* file name */
};


typedef struct sufs_dinode sufs_node_t;

void sufs_mount(ata_dev_t* dev);
void sufs_unmount(void);

sufs_node_t* sufs_open(char* path);
int sufs_close(sufs_node_t* fd);
ssize_t sufs_write(sufs_node_t* fd, void* data, uint64_t offset, size_t nbytes);
ssize_t sufs_read(sufs_node_t* fd, void* buf, uint64_t offset, size_t nbytes);

int sufs_create(char* path);
int sufs_unlink(char* path);
int sufs_mkdir(char* path);
int sufs_rmdir(char* path);
