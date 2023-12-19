#include <ulib.h>

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
    int pages = 4096;
    char * mm1 = mmap(NULL, pages*6, PROT_READ, 0);
    if((long)mm1 < 0)
    {
        // Testcase failed.
        printf("Test case failed \n");
        return 1;
    }

    // Read all the pages
    for(int i = 0; i < 5; i++){
                char temp;
                char* page_read = mm1 +(i*pages);
                temp = page_read[0];
    }

    // Page faults should be 5
    pmap(1);

    // Unmap a few mid pages
    int result = munmap(mm1 + (pages*2), pages);
    if(result < 0)
    {
        printf("Test case failed \n");
        return 1;
    }
    result = munmap(mm1 + (pages*4), pages);
    if(result < 0)
    {
        printf("Test case failed \n");
        return 1;
    }

    // Page faults should be 2
    pmap(1);

    // Changing the protection should give access to write
    result  = mprotect((void *)mm1, pages*6, PROT_READ|PROT_WRITE);
    if(result <0)
    {
        printf("Test case failed \n");
        return 0;
    }

    // read-write access changed
    pmap(1);

    // Write to all the pages
    char* page_write = mm1;
    page_write[0] = 'A';
    page_write = mm1 + pages;
    page_write[0] = 'A';
    page_write = mm1 + 3*pages;
    page_write[0] = 'A';
    page_write = mm1 + 5*pages;
    page_write[0] = 'A';

    // page faults should increase to 6
    pmap(1);

    // should fail now:
    page_write = mm1 + 2*pages;
    page_write[0] = 'A';

    printf("Test case failed\n");
  return 0;
}