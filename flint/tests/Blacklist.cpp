// DO NOT COMPILE ME!!!

#include <cstdio>
#include <cstring>

int main() {
	
	volatile int i = 0;

	while (i == 0) {
		break;
	}

	asm volatile (
		"movl $4, %%eax;"
		"movl $1, %%ebx;"
		"movl %0, %%ecx;"
		"movl %1, %%edx;");

	char *ptr, delim = ' ';
	strtok(ptr, &delim);

	int *a = NULL;
	int *b = nullptr;

	i++;

	return 0;
};