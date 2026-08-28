/**
 * Code for the Kernel main function.
 * 
 * @author Samuel Pires
*/

#include <kernel/multiboot.h>
#include <kernel/system.h>
#include <kernel/mm/mm.h>
#include <kernel/fs/fs.h>

#include <kernel/arch/i386/drivers/vga.h>
#include <kernel/arch/i386/drivers/serial.h>
#include <kernel/arch/i386/drivers/pic.h>
#include <kernel/arch/i386/drivers/pci.h>
#include <kernel/arch/i386/drivers/ata.h>
#include <kernel/arch/i386/io.h>
#include <kernel/arch/i386/system.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>


extern void gdt_init(void);
extern void idt_init(void);


int kmain(multiboot_info_t* mbi, uint32_t magic)
{
	vga_init();
	serial_init();

	if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
		PANIC("Invalid magic number");
	}

	mm_init(mbi);
	printf("Detected Memory\n");
	printf("Initialized Page Allocator\n");
	printf("Initialized Slab Allocator\n");

	gdt_init();
	printf("Loaded GDT\n");

	pic_init();
	printf("Initialized PIC\n");

	idt_init();
	printf("Loaded IDT\n");

	ata_init();
	printf("Detected %hhu ATA Device(s)\n", num_ata_devs);

	if (num_ata_devs > 0) {
		fs_init(ata_devs + 0);
		printf("Initialized File System\n");
	}

	printf("Finished Loading\n");

	return 0;
}
