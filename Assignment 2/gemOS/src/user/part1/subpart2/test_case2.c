#include<ulib.h>

int main (u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5) {
        printf("----------------- Part 1 Subpart 2 Testcase 2 ---------------\n");
        int fd = create_trace_buffer(O_RDWR);
	// char *buff = mmap(0, 10, PROT_WRITE, MAP_POPULATE);
    char buff[10];
	if((long)buff == -1)
        {
		printf("Testcase unable to progress\n");
		return -1;
        }

	for(int i = 0; i< 10; i++){
		buff[i] = 'A' + i;
	}

	int ret = write(fd, buff, 10);
	if(ret != 10){
		printf("1.Test case failed\n");
		return -1;
	}

	printf("Test case passed\n");

	return 0;

}