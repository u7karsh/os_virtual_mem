/*  main.c  - main */

#include <xinu.h>

void test(){
   char *arr = vmalloc(2048*PAGE_SIZE);
   printf("test1 %d\n", arr[0]);
   arr[0] = 1;
   printf("test2 %d\n", arr[0]);
}

process	main(void)
{
   resume(vcreate(test, 4096, 2048, 19, "shell", 0));
   sleep(10);
	return OK;
    
}
