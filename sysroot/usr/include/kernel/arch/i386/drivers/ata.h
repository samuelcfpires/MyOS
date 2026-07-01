#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <kernel/utils.h>


typedef struct ata_dev_s {
	uint8_t id;
	uint16_t port_base;
	bool master;

	uint32_t logical_sector_size;
	uint32_t physical_sector_size;
	uint16_t logical_sector_alignment;

	/* if sectors = 0, mode is not supported */
	uint32_t lba28_num_sectors;
	uint64_t lba48_num_sectors;

	uint8_t supported_udma_modes;	/* mode n is supported if bit n-1 is set */
	uint8_t active_udma_mode;		/* active UDMA mode */
} ata_dev_t;


/* An array of connected ATA devices */
/* Only the first num_ata_devs devices are valid. */
extern ata_dev_t ata_devs[4];

/* Number of connected ATA devices. */
/* The device ID passed to the write and read functions must be lower than this. */
extern unsigned char num_ata_devs;


#define ATA_NUM_SECTORS(dev) ({ __typeof__(dev) _dev = (dev); MAX(_dev->lba28_num_sectors, _dev->lba48_num_sectors); })


/**
 * Initializes the ATA driver.
 * 
 * Should only be called once.
 * 
 * @return the number of connected ATA devices
*/
int ata_init(void);


/**
 * Writes to an ATA dev.
 * 
 * @param dev a pointer to the dev
 * @param data a pointer to write the data from
 * @param lba the lba to start writing the data to
 * @param sector_count the number of sectors to write
 * 
 * @return 0 if the write was successful, -1 otherwise
*/
int ata_write(const ata_dev_t* dev, const void* data, uint64_t lba, uint16_t sector_count);

/**
 * Reads from an ATA dev.
 * 
 * @param dev_id a pointer to the dev
 * @param buf a pointer to where to store the data
 * @param lba the lba to start reading the data from
 * @param sector_count the number of sectors to read
 * 
 * @return 0 if the read was successful, -1 otherwise
*/
int ata_read(const ata_dev_t* dev, void* buf, uint64_t lba, uint16_t sector_count);
