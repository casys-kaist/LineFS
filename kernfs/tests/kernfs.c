#include <stdio.h>
#include "kernfs_interface.h"

int main(void)
{
	printf("initialize file system\n");

#ifdef __x86_64__
	init_fs();
	shutdown_fs();
#endif

#ifdef __aarch64__
	init_nic_fs();
	shutdown_nic_fs();
#endif
}
