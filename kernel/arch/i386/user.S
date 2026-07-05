
global user_func


user_func:
	mov eax, 2
	int 0x80
	mov eax, 1
	int 0x80
