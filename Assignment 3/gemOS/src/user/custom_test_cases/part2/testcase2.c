#include <ulib.h>

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5) {
    int PAGE_SIZE = 4096;
    char* mm1 = mmap(NULL, PAGE_SIZE*7, PROT_READ|PROT_WRITE, 0);
    if ((long)mm1 < 0) {
        printf("1. Test case passed.\n");
        return -1;
    }
    pmap(1);
    char* mm2 = (char*)((unsigned long)mm1 + PAGE_SIZE*3);
    mm2[50] = 'X';
    // should result in page fault
    mm2[51] = 'A';
    if ((mm2[50] != 'X') || (mm2[51] != 'A')) {
        return -1;
    }
    pmap(1);
    int val1 = mprotect(mm2, 5000, PROT_READ);
    // number of vm-area = 3;
    pmap(1);
    char read_char = mm2[50];
    if (read_char != 'X') {
        printf("2. Test case failed");
        return -1;
    }
    int val2 = munmap((char*)((unsigned long)mm2 + PAGE_SIZE), 2*PAGE_SIZE);
    pmap(1);
    if (val2 < 0) {
        printf("3. Test case failed\n");
        return -1;
    }
    char* mm3 = (char*)((unsigned long)mm1 + PAGE_SIZE*6);
    mm3[89] = 'Y';
    // should result in page fault
    pmap(1);

    int val3 = mprotect(mm3, PAGE_SIZE, PROT_READ);
    mm3[90] = 'Z';
    // the program should terminate

    return 0;
}