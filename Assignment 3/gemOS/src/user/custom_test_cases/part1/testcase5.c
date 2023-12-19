#include <ulib.h>

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5) {
    int page = 4096;
    char* addr1 = mmap(NULL, 22, PROT_READ, 0);
    if ((long)addr1 < 0) {
        printf("1. Test failed\n");
        return -1;
    }
    pmap(1);

    char* addr2 = mmap(addr1 + 2*page, 4097, PROT_READ, MAP_FIXED);
    if ((long)addr2 < 0) {
        printf("2. Test failed\n");
        return -1;
    }
    pmap(1);

    addr1 = mmap(addr2 + 2*page, 1680, PROT_READ, 0);
    if ((long)addr1 < 0) {
        printf("3. Test failed\n");
        return -1;
    }
    pmap(1);

    return 0;
}