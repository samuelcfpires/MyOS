; Auxiliary functions for gdt.c
;
; @author Samuel Pires


extern user_func

global load_kernel_segments
global flush_tss
global enable_fast_system_calls
global jump_to_user_func


load_kernel_segments:
	mov ax, 0x10

	; set registers to the kernel's data segment
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov fs, ax
	mov gs, ax

	; use long jump to set the code segment register to the kernel's code segment
	jmp 0x08:flush_cs

flush_cs:
	; cs is now 0x08
	ret

flush_tss:
	mov ax, (5 * 8) | 0
	ltr ax
	ret


jump_to_user_func:
	call flush_tss

	; set registers to the users's data segment
	mov ax, (4 * 8) | 3
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	xor edx, edx
	mov eax, (3 * 8) | 3
	mov ecx, 0x174
	wrmsr

	mov edx, user_func
	mov ecx, esp

	xchg bx, bx
	sysexit
