#pragma once

#define SYSCALL_RESTART_SYSCALL	0
#define SYSCALL_EXIT 			1
#define SYSCALL_FORK 			2
#define SYSCALL_READ 			3
#define SYSCALL_WRITE 			4
#define SYSCALL_OPEN 			5
#define SYSCALL_CLOSE 			6
#define SYSCALL_WAITPID 		7
#define SYSCALL_CREAT 			8
#define SYSCALL_LINK 			9
#define SYSCALL_UNLINK 			10
#define SYSCALL_EXECVE 			11
#define SYSCALL_CHDIR 			12
#define SYSCALL_TIME 			13
#define SYSCALL_MKNOD 			14
#define SYSCALL_CHMOD 			15
#define SYSCALL_LCHOWN 			16


void syscall_entry(void);
void syscall_handler(int syscall_num);
