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

#include <stdint.h>

#define FLAT_MODEL_BASE 						0x0
#define FLAT_MODEL_LIMIT 						0xFFFFF

#define KERNEL_MODE_CODE_SEGMENT_ACCESS_BYTE 	0x9A
#define KERNEL_MODE_DATA_SEGMENT_ACCESS_BYTE 	0x92
#define USER_MODE_CODE_SEGMENT_ACCESS_BYTE	 	0xFA
#define USER_MODE_DATA_SEGMENT_ACCESS_BYTE	 	0xF2
#define TASK_STATE_SEGMENT_ACCESS_BYTE 			0x89

#define LEGACY_MODE_SEGMENT_FLAGS     			0xC


extern void load_kernel_segments(void);


/* Task State Segment */
struct tss {
	uint32_t   link;
	uint32_t   res1;

	uint32_t   esp0[2];

	uint32_t   ss0;
	uint32_t   res2;

	uint32_t   esp1[2];

	uint32_t   ss1;
	uint32_t   res3;

	uint32_t   esp2[2];

	uint32_t   ss2;
	uint32_t   res4;

	uint32_t   cr3[2];

	uint32_t   eip[2];

	uint32_t   eflags[2];

	uint32_t   eax[2];
	uint32_t   ecx[2];
	uint32_t   edx[2];
	uint32_t   ebx[2];
	uint32_t   esp[2];
	uint32_t   ebp[2];
	uint32_t   esi[2];
	uint32_t   edi[2];

	uint32_t   es;
	uint32_t   res5;
	
	uint32_t   cs;
	uint32_t   res6;

	uint32_t   ss;
	uint32_t   res7;

	uint32_t   ds;
	uint32_t   res8;

	uint32_t   fs;
	uint32_t   res9;

	uint32_t   gs;
	uint32_t   res10;

	uint32_t   ldt;
	uint32_t   res11;

	uint32_t   trap;
	uint32_t   iomap;
	
	uint32_t   ssp[2];
};

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
};

static struct tss tss;
static struct gdt gdt;

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

void gdt_init(void)
{
	encode_segment_descriptor(gdt.null_descriptor, 0x0, 0x0, 0x0, 0x0);
	encode_segment_descriptor(gdt.kernel_mode_code_segment, FLAT_MODEL_BASE, FLAT_MODEL_LIMIT, KERNEL_MODE_CODE_SEGMENT_ACCESS_BYTE, LEGACY_MODE_SEGMENT_FLAGS);
	encode_segment_descriptor(gdt.kernel_mode_data_segment, FLAT_MODEL_BASE, FLAT_MODEL_LIMIT, KERNEL_MODE_DATA_SEGMENT_ACCESS_BYTE, LEGACY_MODE_SEGMENT_FLAGS);
	encode_segment_descriptor(gdt.user_mode_code_segment, FLAT_MODEL_BASE, FLAT_MODEL_LIMIT, USER_MODE_CODE_SEGMENT_ACCESS_BYTE, LEGACY_MODE_SEGMENT_FLAGS);
	encode_segment_descriptor(gdt.user_mode_data_segment, FLAT_MODEL_BASE, FLAT_MODEL_LIMIT, USER_MODE_DATA_SEGMENT_ACCESS_BYTE, LEGACY_MODE_SEGMENT_FLAGS);
	encode_segment_descriptor(gdt.task_state_segment, (uint32_t) &tss, sizeof(struct tss), TASK_STATE_SEGMENT_ACCESS_BYTE, 0x0);

	gdtd[2] = (uint16_t) (((uint32_t) &gdt >> 16) & 0xFFFF);
	gdtd[1] = (uint16_t) ((uint32_t) &gdt & 0xFFFF);
	gdtd[0] = (uint16_t) sizeof(struct gdt);

	asm volatile("lgdt [%0]" : : "r" (gdtd));
	
	load_kernel_segments();
	//enable_fast_system_calls();
}

