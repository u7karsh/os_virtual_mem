
#include <xinu.h>
#define PAGE_SIZE 4096
#define TEST1
#define TEST2
#define TEST3
#define TEST4
#define TEST5
#define TEST6
#define TEST7
#define TEST8

sid32 semTest;
pid32 mainPid;
int err[4];

void init_err_arr(){
    int i;
    for(i=0;i<4;i++) err[i]=0;
}

int if_error(){
    int i,error=0;
    for(i=0;i<4;i++){
      if (1==err[i]){
         error=1;
	 break;  
      }
    }
    return error;
}


void test1(int numPages, int pnum){
    int error = 0;
    unsigned char *ptr1 = NULL;
    ptr1 = (unsigned char*)vmalloc(numPages * PAGE_SIZE);
    int i=0,j;

    for( j = 0; j < 101; j++ ){
       // write data
       for(i =0; i<numPages; ++i){
          ptr1[i*PAGE_SIZE]     = pnum;
          ptr1[i*PAGE_SIZE + 1] = i & 0xFF;
          ptr1[i*PAGE_SIZE + 2] = (i >> 8) & 0xFF;
          ptr1[i*PAGE_SIZE + 3] = (i >> 16) & 0xFF;
          ptr1[i*PAGE_SIZE + 4] = (i >> 24) & 0xFF;
       }

       if( rand() % 100 == 0 ){
          yield();
       } else{
          sleepms(1+(rand()%256));
       }

       // read data
       unsigned char c = 0;
       unsigned char d = 0;
       unsigned char e = 0;
       unsigned char f = 0;
       unsigned char g = 0;
       int rec;
       i=0;
       for(i=0; i<numPages; ++i){
          c =  ptr1[i*PAGE_SIZE];
          d =  ptr1[i*PAGE_SIZE+1];
          e =  ptr1[i*PAGE_SIZE+2];
          f =  ptr1[i*PAGE_SIZE+3];
          g =  ptr1[i*PAGE_SIZE+4];
          rec = d | e << 8 | f << 16 | g << 24;
          if( c != pnum || rec != i ){
             printf("MISMATCH: %d %d %d %d %d %d %d %d\n", pnum, i, c, d, e, f, g, rec);
             error = 1;
             break;
          }
       }

       if( rand() % 100 == 0 ){
          yield();
       } else{
          sleepms(1+(rand()%256));
       }
    }
    if (i!=numPages) error=1;

    //vfree for test case 1, 2, 3 and 4
    //vfree((char*)ptr1, numPages*PAGE_SIZE);
    err[pnum]=error;
    //kprintf("\nAX\n");
}

/*
Test1: // An extreme case to exhaust FFS space.
// This is the basic test that verifies if the basic framework is working
 *
 * */
void test1_run(void){
    //By default, FFS is 2048 page, so use 2048 to exhaust FFS area
    int error;
    init_err_arr();

    pid32 p1 = vcreate(test1, 2000, 2048, 50, "test1", 2, 2048, 0);
    resume(p1);

    receive();
    error=if_error();
    if(error){
        kprintf("\nCase1 FAIL\n");
    }else{
        kprintf("\nCase1 PASS\n");
    }
}


/*
 *Test2: // An extreme case to exhaust Disk Space
 * */
void test2_run(void){
    int error;
    init_err_arr();

    pid32 p1 = vcreate(test1, 2000, MAX_HEAP_SIZE, 50, "test2", 2, 4096, 0);
    resume(p1);

    receive();
    error=if_error();
    if(error){
        kprintf("\nCase2 FAIL\n");
    }else{
        kprintf("\nCase2 PASS\n");
    }
}

/*
 *Test 3:// Multiple processes FFS basic case,
 * this test will create 4 processes that exhaust FFS space together
 * */
void test3_run(void){
    int error;
    init_err_arr();

    pid32 p1 = vcreate(test1, 2000, 512, 10, "P1", 2, 512, 0);
    pid32 p2 = vcreate(test1, 2000, 512, 10, "P2", 2, 512, 1);
    pid32 p3 = vcreate(test1, 2000, 512, 10, "P3", 2, 512, 2);
    pid32 p4 = vcreate(test1, 2000, 512, 10, "P4", 2, 512, 3);
    resume(p1);
    resume(p2);
    resume(p3);
    resume(p4);

    receive();
    receive();
    receive();
    receive();

    error=if_error();
    if(error){
        kprintf("\nCase3 FAIL\n");
    }else{
        kprintf("\nCase3 PASS\n");
    }
}

/*
 * Test 4:// Multiple processes Disk basic case,
 * this test will create 4 processes that exhaust Disk space together
 * */
void test4_run(void){
    int error;
    init_err_arr();

    pid32 p1 = vcreate(test1, 2000, 1024, 10, "P1", 2, 1024, 0);
    pid32 p2 = vcreate(test1, 2000, 1024, 10, "P2", 2, 1024, 1);
    pid32 p3 = vcreate(test1, 2000, 1024, 10, "P3", 2, 1024, 2);
    pid32 p4 = vcreate(test1, 2000, 1024, 10, "P4", 2, 1024, 3);
    resume(p1);
    resume(p2);
    resume(p3);
    resume(p4);

    receive();
    receive();
    receive();
    receive();

    error=if_error();
    if(error){
        kprintf("\nCase4 FAIL\n");
    }else{
        kprintf("\nCase4 PASS\n");
    }
}

/*
 * Test 5:// Test if vfree() is working when FFS is exhausted.
 * */
void test5_run(void){
    int error;
    init_err_arr();
    pid32 p1 = vcreate(test1, 2000, 2048, 10, "P1", 2, 2048, 0);
    resume(p1);
    // wait for the first process to be finished
    receive();

    pid32 p2 = vcreate(test1, 2000, 2048, 10, "P2", 2, 2048, 0);
    resume(p2);
    // wait for the second process to be finished
    receive();

    error=if_error();
    if(error){
        kprintf("\nCase5 FAIL\n");
    }else{
        kprintf("\nCase5 PASS\n");
    }
}

/*Test 6: // The same as Test 5, but this time we exhaust Disk Space, so just use 4096 as the heapsize.*/
void test6_run(void){
    int error;
    init_err_arr();
    pid32 p1 = vcreate(test1, 2000, 4096, 10, "P1", 2, 4096, 0);
    resume(p1);
    // wait for the first process to be finished
    receive();

    pid32 p2 = vcreate(test1, 2000, 4096, 10, "P2", 2, 4096, 0);
    resume(p2);
    // wait for the second process to be finished
    receive();

    error=if_error();
    if(error){
        kprintf("\nCase6 FAIL\n");
    }else{
        kprintf("\nCase6 PASS\n");
    }
}

//A simple test case that use part FFS
void test7_run(void){
    //By default, FFS is 2048 page, so use 1024 
    int error;
    init_err_arr();

    pid32 p1 = vcreate(test1, 2000, 1024, 50, "test9", 2, 1000, 0);
    resume(p1);

    receive();
    error=if_error();
    if(error){
        kprintf("\nCase9 FAIL\n");
    }else{
        kprintf("\nCase9 PASS\n");
    }
}

//A simple test case that use all FFS, but part Disk
void test8_run(void){
    //By default, FFS is 2048 page, and Disk is 4096, so use 3072 to exhaust FFS area but give Disk area a relief 
    int error;
    init_err_arr();

    pid32 p1 = vcreate(test1, 2000, 3072, 50, "test8", 2, 3000, 0);
    resume(p1);

    receive();
    error=if_error();
    if(error){
        kprintf("\nCase10 FAIL\n");
    }else{
        kprintf("\nCase10 PASS\n");
    }
}

process	main(void)
{
    mainPid = currpid;
#ifdef TEST1
    kprintf(".........run TEST1......\n");
    test1_run();
#endif
#ifdef TEST2
    kprintf(".........run TEST2......\n");
    test2_run();
#endif
#ifdef TEST3
    kprintf(".........run TEST3......\n");
    test3_run();
#endif
#ifdef TEST4
    kprintf(".........run TEST4......\n");
    test4_run();
#endif
#ifdef TEST5
    kprintf(".........run TEST5......\n");
    test5_run();
#endif
#ifdef TEST6
    kprintf(".........run TEST6......\n");
    test6_run();
#endif
#ifdef TEST7
    kprintf(".........run TEST7......\n");
    test7_run();
#endif
#ifdef TEST8
    kprintf(".........run TEST8......\n");
    test8_run();
#endif
    kprintf("\nAll tests are done!\n");
    return OK;
}


