#include<ulib.h>


int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
    char* addr1 = mmap(NULL, 4096, PROT_EXEC, 0);
    if((long)addr1 != -1) {
        printf("1. TEST CASE FAILED\n");
        return -1;
    }
    pmap(1);

    addr1 = mmap(NULL, 4096, PROT_EXEC, -MAP_FIXED);
    if((long)addr1 != -1) {
        printf("2. TEST CASE FAILED\n");
        return -1;
    }
    pmap(1);

    addr1 = mmap(NULL, 4100, PROT_READ|PROT_WRITE, 0);
    if((long)addr1 < 0) {
        printf("1.TEST CASE FAILED\n");
        return 1;
    }
    // Vm_Area count should be 1
    // Expected output will have address printed. In your case address printed might be different.
    // But See the printed address, (i.e) the start and the end address of the dumped vm area is page aligned irrespective of the length provided.
    pmap(1);

    int munmap1 = munmap(addr1, 100);
    if(munmap1 < 0) {
        printf("2. TEST CASE FAILED\n");
        return -1;
    }
    pmap(1);
    
    return 0;
}