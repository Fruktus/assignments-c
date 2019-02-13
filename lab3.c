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

/* DEV INFO
right now properly (it seems) allocates arrays and parses tokens.
now needs error handling and actually running commands


*/


int main(int argc, char* argv[]){
    static int ARGCNT = 10;
    static int ARGSIZE = 20;
    static int debug = 0; //if 1 will output additional info
    
    if(argc != 4){
        printf("missing parameters\n");
        exit(EXIT_FAILURE);
    }

    struct rlimit limmem={strtol(argv[2], NULL, 10), strtol(argv[2], NULL, 10)};
    struct rlimit limcpu={strtol(argv[3], NULL, 10), strtol(argv[3], NULL, 10)};

    int status;
    FILE * fp;
    char * line = NULL; //nullterminated string, may contain newline
    size_t len = 0; //option for allocation by getline
    ssize_t read; //legth of str

    //open file here or die trying
    fp = fopen(argv[1], "r");

    if (fp == NULL){
        printf("failed to open file\n");
        exit(EXIT_FAILURE);
    }

    
    read = getline(&line, &len, fp);
    
    while(read != -1){
        //go line by line, syntax: program arg0 arg1 ...
        char *newline = strchr( line, '\n' );  //remove newline
        if(newline){
            *newline = 0;
            printf((debug) ? "found nl\n" : ""); //dbg
        }

        //store current line in array or smth <- done by getline 
        char ** args = calloc(ARGCNT, sizeof(char*));
        for(int i=0; i<ARGCNT; i++){
            args[i]=calloc(ARGSIZE, sizeof(char));
        }
        printf((debug) ? "allocated\n" : ""); //dbg
        
        //IMPORTANT: START FROM 0, FIRST SHOULD BE PROGRAM NAME, STORE SEPARATELY
        char *token = NULL;
        int itemcount=0;
        token = strtok(line, " ");
        while(token != NULL) {
            strcpy(args[itemcount], token);
            itemcount++;
            if(debug){printf("parsed token: %s\n", token);} //dbg
            token = strtok(NULL, " ");
        }
        args[itemcount]=NULL;
        printf((debug) ? "processed file\n" : ""); //dbg

        if(itemcount > 1){
            pid_t child = fork();
            if(!child){
                //set limits here
                if((setrlimit(RLIMIT_DATA, &limmem))==-1){exit(EXIT_FAILURE);}
                if((setrlimit(RLIMIT_CPU, &limcpu))==-1){exit(EXIT_FAILURE);}

                //execl("/bin/ls", "ls", "-l", (char *)0); //use the one checking path, execvp i think
                execvp(args[0], args);

                //test program for limits
                /*
                int x=1;
                int y=1;
                int z;
                while(1){
                    z=x+y;
                    x=y+z;
                    y=z+x;
                }
                */

                //if error, handle it
                //no idea how to hunt for it tho    
                exit(0);
            }else{
                struct rusage rstats;
                while(waitpid(child, &status, WNOHANG) == 0){
                    sleep(1);
                }
                if((getrusage(RUSAGE_CHILDREN, &rstats)) != -1){
                    printf("sys: %d usr: %d mem: %ld\n", rstats.ru_utime.tv_usec, rstats.ru_stime.tv_usec, rstats.ru_idrss);
                }
                if(!WIFEXITED(status)){
                    //print info
                    for(int i=0; i<ARGCNT; i++){
                        free(args[i]);
                    }
                    free(args);
                    fclose(fp);
                    if (line){
                        free(line);
                    }
                    exit(EXIT_FAILURE);
                }
            }
        }
        for(int i=0; i<ARGCNT; i++){
        free(args[i]);
        }
        free(args);
        read = getline(&line, &len, fp);
    }
    
    fclose(fp);
    if (line){
        free(line);
    }
    printf("\n");
    exit(EXIT_SUCCESS);
}