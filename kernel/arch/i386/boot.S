; Code for booting the OS
;
; Refer to:
; https://wiki.osdev.org/Higher_Half_bare_bones
;
; @author Samuel Pires

MULTIBOOT_PAGE_ALIGN   equ 1<<0
MULTIBOOT_MEMORY_INFO  equ 1<<1

; Declare constants for the multiboot header.
FLAGS 	 equ	MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO
MAGIC 	 equ 	0x1BADB002
CHECKSUM equ	-(MAGIC + FLAGS)


; Declare a header as in the Multiboot Standard.
section .multiboot.data
align 4
dd MAGIC
dd FLAGS
dd CHECKSUM


; Allocate the initial stack.
section .bootstrap_stack write nobits
align 16
kernel_end_of_stack:
	resb 16384
kernel_stack_bottom:


; Preallocate page directories and tables.
section .bss
align 4096
kernel_page_directory:
	resb 4096
kernel_page_tables:
	resb 1048576						; 256 * 4KB page tables to hold 1GB of memory


; The kernel entry point.
section .multiboot.text exec
extern _kernel_offset
extern _kernel_start_physical
extern _kernel_end_physical
extern _kernel_data_start_physical
extern low_mem_num_pages
global _start

PAGE_TABLE_SIZE  equ 4096
PAGE_TABLE_RANGE equ 4194304

KERNEL_SPACE_PAGE_FRAMES equ 262144	; 1GB

_start:
	; Save the multiboot magic number and info struct
	mov esp, kernel_stack_bottom
	sub esp, _kernel_offset
	add ebx, _kernel_offset
	mov [esp - 4], eax
	mov [esp - 8], ebx

	; Get and save number of low memory page table entries
	mov eax, low_mem_num_pages
	sub eax, _kernel_offset
	mov eax, [eax]
	mov [esp - 12], eax

	mov eax, kernel_page_tables
	sub eax, _kernel_offset
	mov ebx, 0
	mov ecx, [esp - 12]					; number of low memory page table entries
	mov edi, 0x1
	mov esi, 0x3
	mov ebp, esi						; mark first MB as writable

; Map the low memory page tables
.loop.pt:
	cmp ebx, _kernel_start_physical
	cmovae ebp, edi						; mark text and rodata as non-writable
	cmp ebx, _kernel_data_start_physical
	cmovae ebp, esi						; mark remaining memory as writable
	mov edx, ebx
	or edx, ebp
	mov [eax], edx
	add eax, 4
	add ebx, PAGE_TABLE_SIZE
	loop .loop.pt

.loop.pt.zero.start:
	mov ecx, KERNEL_SPACE_PAGE_FRAMES
	sub ecx, [esp - 12]					; index of the first high memory page table entry

; Zero the remaining (high memory) page tables
.loop.pt.zero:
	mov dword [eax], 0
	add eax, 4
	loop .loop.pt.zero

; Identity map the kernel in the page directory
.loop.pd.id.start:
	mov eax, kernel_page_directory
	sub eax, _kernel_offset
	mov ebx, kernel_page_tables
	sub ebx, _kernel_offset
	mov ecx, 0

.loop.pd.id:
	mov edx, ebx
	or edx, 0x3
	mov [eax], edx
	add eax, 4
	add ebx, PAGE_TABLE_SIZE
	add ecx, PAGE_TABLE_RANGE
	cmp ecx, _kernel_end_physical
	jb .loop.pd.id

; Zero the entries between identity and virtual kernel
.loop.pd.zero:
	mov dword [eax], 0
	add eax, 4
	add ecx, PAGE_TABLE_RANGE
	cmp ecx, _kernel_offset
	jb .loop.pd.zero

; Map low memory
.loop.pd.lm.start:
	mov ebx, kernel_page_tables
	sub ebx, _kernel_offset
	or ebx, 0x3
	mov ecx, 256						; add all 256 page tables to page directory

.loop.pd.lm:
	mov [eax], ebx
	add eax, 4
	add ebx, PAGE_TABLE_SIZE
	loop .loop.pd.lm

.end:
	mov eax, kernel_page_directory
	sub eax, _kernel_offset
	mov cr3, eax						; load page directory
	mov eax, cr4
	or eax, 0x00000090					; enable 4MB pages and global-page support
	mov cr4, eax
	mov eax, cr0
	or eax, 0x80010000					; enable paging and write protect
	mov cr0, eax
	lea eax, [_higher_half_start]
	jmp eax								; jump to higher half with an absolute jump


section .text
extern _init
extern kmain

_higher_half_start:
	mov eax, kernel_page_directory
	mov ecx, 0

; Unmap identity mapping
.unmap:
	mov dword [eax], 0
	add eax, 4
	add ecx, PAGE_TABLE_RANGE
	cmp ecx, _kernel_end_physical
	jb .unmap

	; Invalidate TLB
	mov eax, cr3
	mov cr3, eax

.start:
	mov esp, kernel_stack_bottom
	mov ebp, esp
	sub esp, 8							; mind the saved multiboot magic number and info struct

	call _init							; call the global constructors.

	call kmain

.loop:
	hlt
	jmp .loop
