/**
 * Code for setting up the Global Descriptor Table.
 * 
 * Refer to:
 * Intel Software Developer Manual, Volume 3-A: Chapter 3.4.5: Segment Descriptors
 * https://wiki.osdev.org/Global_Descriptor_Table
 * https://wiki.osdev.org/GDT_Tutorial
 * 
 * @author Samuel Pires
*/

#include <kernel/arch/i386/system.h>

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define FLAT_MODEL_BASE 						0x0
#define FLAT_MODEL_LIMIT 						0xFFFFF

#define KERNEL_MODE_CODE_SEGMENT_ACCESS_BYTE 	0x9A
#define KERNEL_MODE_DATA_SEGMENT_ACCESS_BYTE 	0x92
#define USER_MODE_CODE_SEGMENT_ACCESS_BYTE	 	0xFA
#define USER_MODE_DATA_SEGMENT_ACCESS_BYTE	 	0xF2
#define TASK_STATE_SEGMENT_ACCESS_BYTE 			0x89

#define LEGACY_MODE_SEGMENT_FLAGS     			0xC


extern void load_kernel_segments(void);
extern void flush_tss(void);


/* Task State Segment */
struct tss_entry {
	uint32_t prev_tss; // The previous TSS - with hardware task switching these form a kind of backward linked list.
	uint32_t esp0;     // The stack pointer to load when changing to kernel mode.
	uint32_t ss0;      // The stack segment to load when changing to kernel mode.
	// Everything below here is unused.
	uint32_t esp1; // esp and ss 1 and 2 would be used when switching to rings 1 or 2.
	uint32_t ss1;
	uint32_t esp2;
	uint32_t ss2;
	uint32_t cr3;
	uint32_t eip;
	uint32_t eflags;
	uint32_t eax;
	uint32_t ecx;
	uint32_t edx;
	uint32_t ebx;
	uint32_t esp;
	uint32_t ebp;
	uint32_t esi;
	uint32_t edi;
	uint32_t es;
	uint32_t cs;
	uint32_t ss;
	uint32_t ds;
	uint32_t fs;
	uint32_t gs;
	uint32_t ldt;
	uint16_t trap;
	uint16_t iomap_base;
} tss;

/* Global Descriptor Table 
 * Except for a null descriptor in the fist position, this format is not global for all GDTs.
 * It's especially made to have a Flat-Memory Model, which essentially means there is no segmentation.
*/
struct gdt {
	uint8_t null_descriptor[8];
	uint8_t kernel_mode_code_segment[8];
	uint8_t kernel_mode_data_segment[8];
	uint8_t user_mode_code_segment[8];
	uint8_t user_mode_data_segment[8];
	uint8_t task_state_segment[8];
} gdt;

/* Segment Descriptor structure:
*	bits 63:56 - Base Address [31:24]
*	bits 55:52 - Flags (G, D/B, L, AVL)
*	bits 51:48 - Segment Limit [19:16]
*	bits 47:40 - Access Byte (P, DPL(3), S, E, D/C, R/W, A)
*	bits 39:16 - Base Address [23:0]
*	bits 15:0  - Segment Limit [15:0]
*/

/**
 * 	Encodes the Segment Descriptor with the given values.
 * 
 * 	@param entry 		the Segment Descriptor
 * 	@param base 		the Segment Descriptor's base address
 * 	@param limit		the Segment Descriptor's maximum address
 * 	@param access_byte 	the Segment Descriptor's access byte
 * 	@param flags 		the Segment Descriptor's flags
*/
static void encode_segment_descriptor(uint8_t entry[8], uint32_t base, uint32_t limit, uint8_t access_byte, uint8_t flags)
{
	// only 20 bits and 4 bits in the limit and flags, respectively, are used.

	// encode the base
	entry[2] = (uint8_t) (base & 0xFF);
	entry[3] = (uint8_t) ((base >> 8) & 0xFF);
	entry[4] = (uint8_t) ((base >> 16) & 0xFF);
	entry[7] = (uint8_t) ((base >> 24) & 0xFF);

	// encode the limit and flags
	entry[0] = (uint8_t) (limit & 0xFF);
	entry[1] = (uint8_t) ((limit >> 8) & 0xFF);
	entry[6] = (uint8_t) ((flags << 4) | ((limit >> 16) & 0x0F));

	// encode the access byte
	entry[5] = (uint8_t) access_byte;
}

/* Global Descriptor Table Pseudo-Descriptor 
 *  bits 36:16 - the Global Descriptor Table's base address
 *  bits 15:0 -  the Global Descriptor Table's size
*/
static uint16_t gdtd[3];

extern void kernel_end_of_stack(void); 

void gdt_init(void)
{
	tss.ss0 = 0x10;
	tss.esp0 = (uint32_t)kernel_end_of_stack;

	encode_segment_descriptor(gdt.null_descriptor, 0x0, 0x0, 0x0, 0x0);
	encode_segment_descriptor(gdt.kernel_mode_code_segment, FLAT_MODEL_BASE, FLAT_MODEL_LIMIT, KERNEL_MODE_CODE_SEGMENT_ACCESS_BYTE, LEGACY_MODE_SEGMENT_FLAGS);
	encode_segment_descriptor(gdt.kernel_mode_data_segment, FLAT_MODEL_BASE, FLAT_MODEL_LIMIT, KERNEL_MODE_DATA_SEGMENT_ACCESS_BYTE, LEGACY_MODE_SEGMENT_FLAGS);
	encode_segment_descriptor(gdt.user_mode_code_segment, FLAT_MODEL_BASE, FLAT_MODEL_LIMIT, USER_MODE_CODE_SEGMENT_ACCESS_BYTE, LEGACY_MODE_SEGMENT_FLAGS);
	encode_segment_descriptor(gdt.user_mode_data_segment, FLAT_MODEL_BASE, FLAT_MODEL_LIMIT, USER_MODE_DATA_SEGMENT_ACCESS_BYTE, LEGACY_MODE_SEGMENT_FLAGS);
	encode_segment_descriptor(gdt.task_state_segment, (uint32_t)&tss, sizeof(struct tss_entry)-1, TASK_STATE_SEGMENT_ACCESS_BYTE, 0x0);

	gdtd[2] = (uint16_t) (((uint32_t) &gdt >> 16) & 0xFFFF);
	gdtd[1] = (uint16_t) ((uint32_t) &gdt & 0xFFFF);
	gdtd[0] = (uint16_t) sizeof(struct gdt);

	asm volatile("lgdt [%0]" : : "r" (gdtd));
	
	load_kernel_segments();
	flush_tss();
}
