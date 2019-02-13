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




int term = 0;
int reply = 0;
int killprog = 0;

void usr1sig(int signum){
    reply = 1;
}

void usr2sig(int signum){
    term = 1;
}

void intsig(int signum){
    killprog = 1;
}


int main(int argc, char* argv[]){
    time_t rawtime;
    struct tm * timeinfo;

    if(argc!=3){
        printf("wrong arguments\n");
        exit(EXIT_FAILURE);
    }    
    

    printf("PID: %d\n", getpid()); //dbg/clarity
    
    signal(SIGUSR1, usr1sig);
    signal(SIGUSR2, usr2sig);

    pid_t child = fork();
    int status;
    if(child<0){exit(EXIT_FAILURE);}

    //child
    if(!child){
        if(strtol(argv[2], NULL, 10) != 3){
            while(1){
                if(reply){
                    reply = 0;
                    kill(getppid(), SIGUSR1);
                }
                if(term){
                    kill(getppid(), SIGUSR2);
                    exit(EXIT_SUCCESS);
                }
            }
        }else{
            //DISABLED FOR NOW, MACOSX DOES NOT SUPPORT REALTIME SIGNALS
            //signal(SIGRTMIN+1, usr1sig);
            //signal(SIGRTMIN+2, usr2sig);
            while(1){
                if(reply){
                    reply = 0;
                    kill(getppid(), SIGUSR1);
                }
                if(term){
                    kill(getppid(), SIGUSR2);
                    exit(EXIT_SUCCESS);
                }
            }
        }
    //parent
    }else{
        signal(SIGINT, intsig);

        printf("CPID: %d\n",child);
        int replies = 0;
        switch(strtol(argv[2], NULL, 10)){
            case 1:
                for(int i=0; i<strtol(argv[1], NULL, 10); i++){
                    kill(child, SIGUSR1);
                    printf("wyslano\n");
                    if(reply == 1){ 
                        printf("odebrano\n");
                        reply=0;
                        replies++;
                    }else{
                        printf("no response\n");
                    }
                    if(killprog == 1){
                        printf("odebrano SIGINT\n");
                        kill(child, SIGUSR2);
                        exit(EXIT_SUCCESS);
                    }
                    sleep(1);
                }
                kill(child, SIGUSR2);
                break;
            case 2:
                for(int i=0; i<strtol(argv[1], NULL, 10); i++){
                    kill(child, SIGUSR1);
                    printf("wyslano\n"); //dbg
                    while(reply==0 && waitpid(child, &status, WNOHANG) == 0 && term == 0){
                        sleep(1);
                    }
                    if(waitpid(child, &status, WNOHANG) != 0){ 
                        printf("child ded\n");
                        break;
                    }else{
                        printf("odebrano\n"); //dbg
                        replies++;
                    }
                    if(killprog == 1){
                        printf("odebrano SIGINT\n");
                        kill(child, SIGUSR2);
                        exit(EXIT_SUCCESS);
                    }
                    reply=0;
                }
                kill(child, SIGUSR2);
                break;
            case 3:
            //NOTE: MACOSX DOES NOT SUPPORT REALTIME SIGNALS
            /*
                for(int i=0; i<strtol(argv[1], NULL, 10); i++){
                    kill(child, SIGRTMIN+1);
                    printf("wyslano\n");
                    if(reply == 1){ 
                        printf("odebrano\n");
                        reply=0;
                        replies++;
                    }else{
                        printf("no response\n");
                    }
                    if(killprog == 1){
                        printf("odebrano SIGINT\n");
                        kill(child, SIGRTMIN+2);
                        exit(EXIT_SUCCESS);
                    }
                    sleep(1);
                }
                kill(child, SIGRTMIN+2);
                */
               break;
            default: 
                printf("niepoprawny tryb\n");
                kill(child, SIGUSR2);
                exit(EXIT_FAILURE);
        }
        printf("wyslano: %ld\n", strtol(argv[1], NULL, 10));
        printf("odebrane: %d\n", replies);    
    }
}