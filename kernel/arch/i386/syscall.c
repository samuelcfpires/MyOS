/**
 * Code for handling system calls.
 * 
 * @author Samuel Pires
 */

#include <kernel/syscall.h>
#include <kernel/arch/i386/system.h>

#include <stdio.h>


void syscall_handler(int syscall_num)
{
	switch (syscall_num) {
	default:
		printf("Unhandled syscall: %d\n", syscall_num);
		STOP;
		break;
	}
}
