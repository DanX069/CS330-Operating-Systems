#include<ulib.h>


int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
    // check splitting of node in case of munmap()
    int page = 4096;
    char* addr1 = mmap(NULL, 4096 * 3, PROT_READ, 0);
    if((long)addr1 == -1) {
        printf("1. TEST CASE FAILED\n");
        return -1;
    }
    pmap(1);

    int munmap1 = munmap(addr1 + 4096, 100);
    if(munmap1 == -1) {
        printf("2. TEST CASE FAILED\n");
        return -1;
    }
    pmap(1);
    
    return 0;
}