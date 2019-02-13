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



int dec(char* a){
    return strtol(a, NULL, 10);
}

int main(int argc, char* argv[]){
    if(argc!=3){
        printf("invalid parameters\n");
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));
    int fd = open(argv[1], O_WRONLY);
    char arr[50];
    printf("PID: %d\n", getpid());

    for(int i=0; i<dec(argv[2]); i++){
        FILE *date = popen("date", "r");
        fgets(arr, sizeof(arr), date);
        pclose(date);
        printf("out: %s\n", arr);
        write(fd, arr, sizeof(arr));
        sleep(rand()%3+2);
    }

    close(fd);
}