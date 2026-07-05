/**
 * Code for handling system calls.
 * 
 * @author Samuel Pires
 */

#include <kernel/syscall.h>
#include <kernel/arch/i386/system.h>

#include <stdio.h>


void myexit()
{
	printf("Exited program\n");
	STOP;
}

void syscall_handler(int syscall_num)
{
	switch(syscall_num) {
		case(1): myexit(); break;
		case(2): printf("syscall test\n"); break;
		default: break;
	}
}
