#include <ulib.h>

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5) {
    int pages = 4096;

    char* mm1 = mmap(NULL, 4000, PROT_READ, 0);
    if ((long)mm1 < 0) {
        printf("1. test case failed\n");
        return -1;
    }
    char* mm2 = mmap(NULL, 7000, PROT_READ|PROT_WRITE, 0);
    if ((long)mm2 < 0) {
        printf("2. test case failed\n");
        return -1;
    }
    char* mm3 = mmap(NULL, 8000, PROT_READ, 0);
    if ((long)mm3 < 0) {
        printf("3. test case failed\n");
        return -1;
    }
    pmap(1);

    int result = mprotect((void*)mm1, 4*pages, PROT_READ);

    if (result < 0) {
        printf("4. test case failed\n");
        return -1;
    }
    pmap(1);

    return 0;
}