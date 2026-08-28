; Entry point for the Interrupt Service Routines
; This code implements a common function (and macros) to handle all the ISRs
;
; Taken mostly from https://littleosbook.github.io/#creating-a-generic-interrupt-handler

%macro no_error_isr 1
global interrupt_handler_%1
interrupt_handler_%1:
	push    dword 0  	; push 0 as error code
	push    dword %1 	; push the interrupt vector id
	jmp     isr_entry	; jump to the common handler
%endmacro

%macro error_isr 1
global interrupt_handler_%1
interrupt_handler_%1:
	push    dword %1 	; push the interrupt vector id
	jmp     isr_entry	; jump to the common handler
%endmacro

extern isr_handler

isr_entry:				; isr entry point
	pushad				; save the registers
	cli					; disable interrupts
	call isr_handler	; call the C function
	popad				; restore the registers
	add esp, 8			; restore the esp
	iret				; return to the code that got interrupted


; Interrupt Vectors

; CPU exceptions
no_error_isr 0
no_error_isr 1
no_error_isr 2
no_error_isr 3
no_error_isr 4
no_error_isr 5
no_error_isr 6
no_error_isr 7
error_isr    8
no_error_isr 9
error_isr    10
error_isr    11
error_isr    12
error_isr    13
error_isr    14
no_error_isr 15
no_error_isr 16
error_isr    17
no_error_isr 18
no_error_isr 19
no_error_isr 20
no_error_isr 21
no_error_isr 22
no_error_isr 23
no_error_isr 24
no_error_isr 25
no_error_isr 26
no_error_isr 27
no_error_isr 28
no_error_isr 29
no_error_isr 30
no_error_isr 31

; PIC IRQs
no_error_isr 32
no_error_isr 33
no_error_isr 34
no_error_isr 35
no_error_isr 36
no_error_isr 37
no_error_isr 38
no_error_isr 39
no_error_isr 40
no_error_isr 41
no_error_isr 42
no_error_isr 43
no_error_isr 44
no_error_isr 45
no_error_isr 46
no_error_isr 47

; Kernel IRQs
no_error_isr 128	; syscall
