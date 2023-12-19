#include<ulib.h>

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
  int pages = 4096;

  char * mm1 = mmap(NULL, pages*6, PROT_READ|PROT_WRITE, 0);
  if((long)mm1 < 0)
  {
     // Testcase failed.
     printf("1. Test case failed \n");
     return 1;
  }

  char * mm2 = (char *)((unsigned long)mm1 + pages* 3);
  //should result in page fault
  mm2[0] = 'X'; 

  // vm_area count should be 1 and Page fault should be 1
  pmap(0);

  if(mm2[0] != 'X')
  {
    // Testcase failed
    printf("2. Test case failed \n");
    return 0;
  }
  mm2[1] = 'D';
  // vm_area count and Page fault count both should still be 1
  pmap(0);

  if(mm2[1] != 'D')
  {
    // Testcase failed
    printf("3. Test case failed \n");
    return 0;
  }

  mm2 = (char *)((unsigned long)mm1 + pages*5);
  mm2[0] = '2';
  // should result in page fault
  pmap(0);
  if(mm2[0] != '2')
  {
    // Testcase failed
    printf("4. Test case failed \n");
    return 0;
  }

  mm2 = (char *)((unsigned long)mm1 + pages*4);

  int val1 = munmap(mm2, 67);

  if (val1 < 0) {
    printf("5. Test case failed\n");
    return 0;
  }

  pmap(0);

  mm2[5000] = 'F';
  // no error
  if(mm2[5000] != 'F')
  {
    // Testcase failed
    printf("6. Test case failed \n");
    return 0;
  }
  pmap(0);

  mm2[0] = 'X';


  printf("7. Test case failed\n");

 return 0;
}
