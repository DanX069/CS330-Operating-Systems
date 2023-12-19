#include <ulib.h>

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5) {
    int pages = 4096;
    char* mm1 = mmap((void*)(0x180204000), 4000, PROT_READ, MAP_FIXED);
    if ((long)mm1 < 0) {
        printf("1. Test case failed\n");
        return -1;
    }
    pmap(1);
    char* mm2 = mmap((void*)((long)mm1 + 2*pages), 34, PROT_READ|PROT_WRITE, 0);
    if ((long)mm2 < 0) {
        printf("2. Test case failed\n");
        return -1;
    }
    pmap(1);
    int val = munmap(mm1 - pages, mm2 + 3*pages - mm1);
    if (val < 0) {
        printf("3. Test case failed\n");
        return -1;
    }
    pmap(1);
    return 0;
}