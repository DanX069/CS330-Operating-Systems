#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>

unsigned long calculate_dir_size(const char *path, int write_pipe);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Unable to execute\n");
        exit(1);
    }

    int pipefd[2];
    pipe(pipefd);

    unsigned long size = calculate_dir_size(argv[1], pipefd[1]);
    
    printf("%lu\n", size);
    
    return 0;
}

unsigned long calculate_dir_size(const char *path, int write_pipe) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    unsigned long total_size = 0;

    if ((dir = opendir(path)) == NULL) {
        printf("Unable to execute\n");
        exit(1);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (lstat(full_path, &statbuf) == -1) {
            printf("Unable to execute\n");
            exit(1);
        }

        if (S_ISLNK(statbuf.st_mode)) {
            total_size += calculate_dir_size(full_path, write_pipe);
        } 
        else if (S_ISDIR(statbuf.st_mode)) {
            int pipefd[2];
            pipe(pipefd);
            
            pid_t pid = fork();
            if (pid == 0) {  // Child
                close(pipefd[0]);
                unsigned long size = calculate_dir_size(full_path, pipefd[1]);
                write(pipefd[1], &size, sizeof(size));
                close(pipefd[1]);
                exit(0);
            } else {  // Parent
                unsigned long sub_size = 0;
                close(pipefd[1]);
                read(pipefd[0], &sub_size, sizeof(sub_size));
                close(pipefd[0]);
                //wait(NULL); // Wait for the child to exit
                total_size += sub_size;
            }
        } 
        else {
            total_size += statbuf.st_size;
        }
    }

    closedir(dir);

    if (stat(path, &statbuf) == -1) {
        printf("Unable to execute\n");
        exit(1);
    }

    total_size += statbuf.st_size;
    write(write_pipe, &total_size, sizeof(total_size));
    return total_size;
}