#include <ulib.h>

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5) {
    char* addr1 = mmap(NULL, 22, PROT_READ, 0);
    if ((long)addr1 < 0) {
        printf("1. Test failed\n");
        return -1;
    }
    pmap(1);

    addr1 = mmap(addr1 + 8192, 4097, PROT_READ, 0);
    if ((long)addr1 < 0) {
        printf("2. Test failed\n");
        return -1;
    }
    pmap(1);

    addr1 = mmap(NULL, 1680, PROT_READ | PROT_WRITE, 0);
    if ((long)addr1 < 0) {
        printf("3. Test failed\n");
        return -1;
    }
    pmap(1);

    return 0;
}