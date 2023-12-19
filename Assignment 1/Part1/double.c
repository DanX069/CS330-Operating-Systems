#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Unable to execute\n");
        exit(EXIT_FAILURE);
    }
    
    unsigned long num = atoi(argv[argc-1]);
    unsigned long result = 2 * num;

    if (argc > 2) {
        char buffer[20];
        sprintf(argv[argc-1], "%lu", result);
        argv++;
        if (execv(argv[0], argv) == -1) {
            printf("Unable to execute\n");
            exit(EXIT_FAILURE);
        }
    } 
    printf("%lu\n", result);
    exit(result);
}