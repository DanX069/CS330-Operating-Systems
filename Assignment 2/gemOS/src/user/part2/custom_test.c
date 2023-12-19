#include <ulib.h>

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5) {
    int strace_fd = create_trace_buffer(O_RDWR);
    if (strace_fd != 3) {
        printf("create trace buffer failed\n");
        return -1;
    }
    int rdwr_fd = create_trace_buffer(O_RDWR);
    if(rdwr_fd != 4){
        printf("create trace buffer failed for read and write\n");
        return -1;
    }

    u64 strace_buff[4096];
	u64 strace_buff_2[4096];
    int read_buff[4096];
    int write_buff[4096];

    for(int i = 0, j = 0; i< 4096; i++){
        j = i % 26;
        write_buff[i] = 'A' + j;
    }

    int ret = strace(25, 0);
    if (ret < 0) {
        printf("1. Test case failed\n");
        return -1;
    }
    ret = strace(2, 0);
    if (ret < 0) {
        printf("2. Test case failed\n");
        return -1;
    }

    ret = strace(25, 0);
    if (ret != -1) {
        printf("3. Test case failed\n");
        return -1;
    }

    ret = strace(32, 0);
    if (ret != -1) {
        printf("4. Test case failed\n");
        return -1;
    }

    ret = strace(24, 0);
    if (ret < 0) {
        printf("5. Test case failed\n");
        return -1;
    }

    start_strace(strace_fd, FILTERED_TRACING);

    int write_ret = write(rdwr_fd, write_buff, 10);

    if (write_ret != 10) {
        printf("6. Test case failed\n");
        return -1;
    }

    int read_ret = read(rdwr_fd, write_buff, 10);
    if (read_ret != 10) {
        printf("7. Test case failed\n");
        return -1;
    }

    sleep(5);

    strace(40, 0);
    strace(11, 0);

    getpid();
    get_stats();
    end_strace();


    int strace_ret = read_strace(strace_fd, strace_buff, 4);
    if (strace_ret != 96) {
        printf("8. Test case failed\n");
        return -1;
    }

    strace_ret = read_strace(strace_fd, strace_buff + 12, 3);
    if (strace_ret != 8) {
        printf("9. Test case failed\n");
        return -1;
    }

    if (strace_buff[0] != 25) {
        printf("10. Test case failed\n");
        return -1;
    }

    if (strace_buff[4] != 24) {
        printf("11. Test case failed\n");;
        return -1;
    }

    if (strace_buff[8] != 40) {
        printf("12. Test case failed\n");
        return -1;
    }

    if (strace_buff[11] != 2) {
        printf("13. Test case failed\n");
        return -1;
    }

    if (strace_buff[12] != 11) {
        printf("14. Test case failed\n");
        return -1;
    }



    printf("Test case passed\n");
    close(strace_fd);
    close(rdwr_fd);
}