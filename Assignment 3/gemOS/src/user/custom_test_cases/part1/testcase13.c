#include<ulib.h>


int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
    int page = 4096;
    char* addr1 = mmap(NULL, 4096, PROT_EXEC, 0); // wrong protection
    if((long)addr1 != -1) {
        printf("1. TEST CASE FAILED\n");
        return -1;
    }
    pmap(1);

    char* addr2 = mmap(NULL, 4096, PROT_EXEC, -MAP_FIXED); // wrong flag
    if((long)addr2 != -1) {
        printf("2. TEST CASE FAILED\n");
        return -1;
    }
    pmap(1);

    char* addr3 = mmap(NULL, 4100, PROT_READ|PROT_WRITE, 0); // success, 2 pages
    if((long)addr3 < 0) {
        printf("1.TEST CASE FAILED\n");
        return 1;
    }
    pmap(1);

    int munmap1 = munmap(addr1, 100);
    if(munmap1 != -1) {
        printf("2. TEST CASE FAILED\n");
        return -1;
    }
    pmap(1);

    munmap1 = munmap(addr2, 4096);
    if(munmap1 != -1) {
        printf("3. TEST CASE FAILED\n");
        return -1;
    }
    pmap(1);
    
    return 0;
}