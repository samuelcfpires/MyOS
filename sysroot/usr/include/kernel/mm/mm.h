#pragma once

#include <kernel/multiboot.h>
#include <kernel/ds/list.h>
#include <kernel/mm/slab.h>

/* Must be defined: PAGE_SIZE */
#ifdef __i386__
#include <kernel/arch/i386/paging.h>
#endif

#include <stdint.h>
#include <stddef.h>

extern char _kernel_offset;

#define KERNEL_OFFSET 		((uintptr_t) &_kernel_offset)

#define P2V(p)				((p) + KERNEL_OFFSET)
#define V2P(v)				((v) - KERNEL_OFFSET)


typedef struct page_s
{
	list_t list;

	union {
		struct {
			void* cache;
			void* slab;
		};
	};
} page_t;


#define HIGH_MEM_START		(896 * (1 << 20))	/* 896MB */
#define HIGH_MEM_PFN		(HIGH_MEM_START / PAGE_SIZE)

extern page_t* mem_map;
extern size_t mem_map_length;

#define phys_to_page(p)		((page_t*) (mem_map + ((uintptr_t)(p)) / PAGE_SIZE))
#define virt_to_page(v)		((page_t*) (phys_to_page(V2P((uintptr_t)(v)))))


/* Page allocation flags */
#define PA_HIGHMEM	(1 << 0)

#define PA_KERNEL	0


/**
 * Initializes the memory manager.
 * 
 * @param mbi the multiboot info structure
*/
void mm_init(multiboot_info_t* mbi);


/* Page allocation functions */

/**
 * Allocates a contiguous number of pages.
 * 
 * @param num_pages the number of contiguous pages to allocate
 * 
 * @return the address of the first page
*/
void* alloc_pages(size_t num_pages, unsigned char flags);

/**
 * Frees a contiguous number of pages.
 * 
 * @param page_addr the address of the first page
 * @param num_pages the number of contiguous
*/
void free_pages(void* page_addr, size_t num_pages);

#define alloc_page(flags) alloc_pages(1, flags)

#define free_page(page_addr) free_pages(page_addr, 1)



/* General memory allocation functions */

#define kmalloc kmem_alloc
#define kfree 	kmem_free
