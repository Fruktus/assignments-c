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

//
// This one's finished
//



int flag = 0;
int term = 0;

void stopsig(int signum){
    if(!flag){
        printf("\nOczekuję na CTRL+Z - kontynuacja albo CTR+C - zakonczenie programu\n");
        flag = 1;
    }else{
        flag = 0;
    }
}

void intsig(int signum){
    printf("Odebrano sygnał SIGINT\n");
    term = 1;
}

int main(int argc, char* argv[]){
    time_t rawtime;
    struct tm * timeinfo;

    
    signal(SIGTSTP, stopsig);
    signal(SIGINT, intsig);

    printf("PID: %d\n", getpid());
    while(1){
        pid_t child = fork();
        int status;

        if(!child){
            execlp("date", "date", NULL);
        }else{
            while(waitpid(child, &status, WNOHANG) == 0){
                if(flag){
                    kill(child, SIGTERM);
                }
                if(term){
                    kill(child, SIGTERM);
                    exit(EXIT_SUCCESS);
                }
                sleep(1);
            }
        }
    }
    
}