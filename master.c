#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <sys/resource.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>


#define MAX_BUF 1024
char* argv[2];
int fd;

void intsig(int signum){
    close(fd);
    unlink(argv[1]);
    printf("received SIGINT\n");
    exit(EXIT_SUCCESS);
}


int main(int argc, char* argv[]){
    if(argc != 2){
        printf("missing path\n");
        exit(EXIT_FAILURE);
    }
    signal(SIGINT, intsig);

    //int mkfifo(const char *pathname, mode_t mode); //mode: w/r
    if(mkfifo(argv[1], 0666)<0){
        perror("mkfifo: ");
        exit(EXIT_FAILURE);
    }

    char buf[MAX_BUF];
    fd = open(argv[1], O_RDONLY);
    if(fd < 0){
        perror("Open: ");
        exit(EXIT_FAILURE);
    }

    while(1){
        read(fd, buf, MAX_BUF);
        printf("pipe: %s\n", buf);
    }

    close(fd);
    unlink(argv[1]);
    exit(EXIT_SUCCESS);
}