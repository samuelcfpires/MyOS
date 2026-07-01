/**
 * Code for Memory Management.
 * 
 * @author Samuel Pires
*/

#include <kernel/mm/mm.h>
#include <kernel/utils.h>
#include <kernel/system.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>


#define MEM_MAP_MAX_LENGTH	((1 << 30) / (4 * (1 << 10)))	/* 1GB / 4KB page frames */


extern char _kernel_end_physical;

page_t* mem_map;
size_t mem_map_length;

/* A variable must be defined for boot sequence */
const unsigned long low_mem_num_pages = HIGH_MEM_PFN;


extern uintptr_t bmap_init(uintptr_t mem_start, uintptr_t mem_end);
extern void bmap_exclude(uintptr_t start_addr, uintptr_t end_addr);
extern void* bmap_alloc(size_t num_pages);
extern void* bmap_alloc_lower(size_t num_pages, uintptr_t addr_lower);
extern void* bmap_alloc_upper(size_t num_pages, uintptr_t addr_upper);
extern void* bmap_alloc_range(size_t num_pages, uintptr_t addr_lower, uintptr_t addr_upper);
extern void bmap_free(void* page_addr, size_t num_pages);
extern void bmap_print(void);


static struct kmem_cache_s* page_cache;


static uintptr_t mem_map_init(uintptr_t mem_start, uintptr_t mem_end);
static uintptr_t detect_mem_end(multiboot_info_t* mbi);
static void detect_mem_holes(multiboot_info_t* mbi, uintptr_t mem_start);
static bool mmm_exceeds_max_mem(multiboot_memory_map_t* mmm);


/* Global Functions */

void mm_init(multiboot_info_t* mbi)
{
	/* Check if there's a valid memory map */
    if (!(mbi->flags & MULTIBOOT_INFO_MEM_MAP)) {
        PANIC("Invalid memory map given by GRUB bootloader");
	}

	uintptr_t mem_end = detect_mem_end(mbi);

	/* Initialize mem_map */
	uintptr_t mem_start = mem_map_init((uintptr_t) &_kernel_end_physical, mem_end);

	/* Initialize bitmap */	
	mem_start = bmap_init(mem_start, mem_end);

	/* Detect and exclude memory holes */
	detect_mem_holes(mbi, mem_start);

	/* Initialize the slab allocator */
	kmem_cache_init();

	/* Create a page cache */
	page_cache = kmem_cache_create("page_cache", sizeof(page_t), mem_zero_cache_constructor, NULL);
}


void* alloc_pages(size_t num_pages, unsigned char flags)
{
	void* page_addr;

	/* Try to allocate a page from high memory first if specified */
	if (flags & PA_HIGHMEM)
	{
		page_addr = bmap_alloc_lower(num_pages, HIGH_MEM_START);
		if (page_addr != NULL)
			return page_addr;
	}

	/* Otherwise, allocate from the kernel address space */
	page_addr = bmap_alloc_upper(num_pages, HIGH_MEM_START);
	if (page_addr == NULL)
		PANIC("out of memory");

	return page_addr;
}


void free_pages(void* page_addr, size_t num_pages)
{
	bmap_free(page_addr, num_pages);
}



/* Helper Functions */


static uintptr_t mem_map_init(uintptr_t mem_start, uintptr_t mem_end)
{
	mem_start = ALIGN_UP(mem_start, sizeof(page_t));
	mem_map = (page_t*) P2V(mem_start);
	mem_map_length = MIN(mem_end / PAGE_SIZE, MEM_MAP_MAX_LENGTH);

	for (size_t i = 0; i < mem_map_length; i++)
		mem_map[i] = (page_t) {0};

	return mem_start + mem_map_length * sizeof(page_t);
}

/**
 * Detects and returns the end of the memory.
 * 
 * @param mbi the multiboot info struct
 * 
 * @return the end of memory address
*/
static uintptr_t detect_mem_end(multiboot_info_t* mbi)
{
	multiboot_memory_map_t* last_available_mmm = NULL;

	for (unsigned int i = 0; i < mbi->mmap_length; i += sizeof(multiboot_memory_map_t)) {
        multiboot_memory_map_t* mmm = (multiboot_memory_map_t*) P2V(mbi->mmap_addr + i);

		if (mmm->type == MULTIBOOT_MEMORY_AVAILABLE)
			last_available_mmm = mmm;
	}

	if (last_available_mmm == NULL) {
		PANIC("No usable memory found in multiboot memory map");
	}

	if (mmm_exceeds_max_mem(last_available_mmm))
		return 0xFFFFFFFF;

	return last_available_mmm->addr_low + last_available_mmm->len_low;
}


/**
 * Detects and excludes memory holes in the already initialized bitmap.
 * 
 * @param mbi the multiboot info struct
 * @param mem_start the starting address of available memory
*/
static void detect_mem_holes(multiboot_info_t* mbi, uintptr_t mem_start)
{
	uintptr_t curr_addr = mem_start;

	for (unsigned int i = 0; i < mbi->mmap_length; i += sizeof(multiboot_memory_map_t)) {
        multiboot_memory_map_t* mmm = (multiboot_memory_map_t*) P2V(mbi->mmap_addr + i);

		if (mmm->type == MULTIBOOT_MEMORY_AVAILABLE)
		{
			/* Ignore memory above 4GB */
			if (mmm_exceeds_max_mem(mmm))
				break;

			if (mmm->addr_low > curr_addr)
				bmap_exclude(curr_addr, mmm->addr_low);

			if (mmm->addr_low + mmm->len_low > curr_addr)
				curr_addr = mmm->addr_low + mmm->len_low;
		}
	}
}


/**
 * Returns true if the given multiboot memory map entry exceeds max memory.
 * 
 * @param mmm the multiboot memory map entry
 * 
 * @return true if the given multiboot memory map entry exceeds max memory, false otherwise
*/
static bool mmm_exceeds_max_mem(multiboot_memory_map_t* mmm)
{
	return mmm->addr_high > 0 || mmm->len_high > 0 || UNSIGNED_SUM_OVERFLOWS(mmm->addr_low, mmm->len_low);
}
