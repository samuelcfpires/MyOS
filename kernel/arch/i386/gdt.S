; Auxiliary functions for gdt.c
;
; References:
;	- https://wiki.osdev.org/Getting_to_Ring_3
; 	- https://wiki.osdev.org/SYSENTER
;	- https://www.felixcloutier.com/x86/sysenter
;	- https://www.felixcloutier.com/x86/sysexit
;
; @author Samuel Pires

global load_kernel_segments
global flush_tss
global enable_fast_system_calls


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



; paging stuff is missing, otherwise both routines are working

jump_to_usermode:
	; set registers to the users's data segment
	; (<index to GDT entry> * <entry size>) | <ring level>
	mov ax, (4 * 8) | 3
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	; set up the stack frame iret expects
	; user CS and SS are set on iret according to passed selectors
	mov eax, esp
	push (4 * 8) | 3 	; data selector
	push eax 			; current esp
	pushf 				; eflags
	push (3 * 8) | 3	; code selector
	;push user_func 		; instruction address to return to

	xchg bx, bx
	iret

jump_to_usermode_fast:
	mov ax, (4 * 8) | 3
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	; set user CS and SS segments (applied on sysexit)
	; IA32_SYSENTER_CS (0x174): Contains ring 0 code segment (CS)
	; 32-bit mode: ring 3 code = CS + 16; ring 3 data = CS + 24
	xor edx, edx
	mov eax, 0x8
	mov ecx, 0x174
	wrmsr

	;mov edx, user_func	; to be loaded into EIP
	mov ecx, esp		; to be loaded into ESP

	xchg bx, bx
	sysexit
