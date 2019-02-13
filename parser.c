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




int main(int argc, char* argv[]){
    static int ARGCNT = 10; //max arguments
    static int ARGSIZE = 20; //max char length of single argument
    static int debug = 0; //if 1 will output additional info
    static int PRGCNT = 10; //max piped progs
    
    if(argc != 4){
        printf("missing parameters\n");
        exit(EXIT_FAILURE);
    }


    int status; //for child process
    FILE * fp;
    char * line = NULL; //nullterminated string, may contain newline 
    size_t len = 0; //option for automatic allocation by getline
    ssize_t read; //length of str

    //open file here or die trying
    fp = fopen(argv[1], "r");
    if (fp == NULL){
        printf("failed to open file\n");
        exit(EXIT_FAILURE);
    }

    
    //get the first line and process them
    read = getline(&line, &len, fp);
    
    while(read != -1){
        int isPiped = 0; //in case of pipe will be set to 1, required for different array freeing
        //go line by line, syntax: program arg0 arg1 ... lub program arg0+ | program ...
        char *newline = strchr( line, '\n' );  //remove newline
        if(newline){
            *newline = 0;
            printf((debug) ? "found nl\n" : ""); //dbg
        }

        char* pPosition = strchr(line, '|'); //if pPosition != NULL, the line containts pipes
        if(pPosition != NULL){
            //I assume that there wont be more than 10 piped programs, can be adjusted
            //draft: create 3dim array, first field addressing consecutive pipe programs, rest behaving like without pipes

            //not really optimal to allocate all the time but no time to make it better now
            char *** args = calloc(PRGCNT, sizeof(char**));
            for(int i=0; i<PRGCNT; i++){
                char ** subargs = calloc(ARGCNT, sizeof(char*));
                for(int j=0; j<ARGCNT; j++){
                    subargs[j]=calloc(ARGSIZE, sizeof(char));
                }
                args[i]=subargs;
            }
            printf((debug) ? "allocated w'pipe\n" : ""); //dbg


            char ** temparr = calloc(PRGCNT, sizeof(char*));
            for(int i=0; i<PRGCNT; i++){
                temparr[i]=calloc(ARGSIZE*ARGCNT, sizeof(char));
            }
            char *token = NULL;
            int progcount=0;
            token = strtok(line, "|");
            while(token != NULL) {
                strcpy(temparr[progcount], token);
                progcount++;
                if(debug){printf("parsed token: %s\n", token);} //dbg
                token = strtok(NULL, "|");
            }
            temparr[progcount]=NULL;
            printf((debug) ? "processed line\n" : ""); //dbg

            token=NULL;
            for(int i=0; i<progcount; i++){
                token = strtok(temparr[i], " ");
                int itemcount = 0;
                while(token != NULL) {
                    strcpy(args[i][itemcount], token);
                    itemcount++;
                    if(debug){printf("parsed token: %s\n", token);} //dbg
                    token = strtok(NULL, " ");
                }
                args[i][itemcount]=NULL;
            }
            printf((debug) ? "processed args\n" : ""); //dbg
            for(int i=0; i<PRGCNT; i++){
                free(temparr[i]);
            }
            free(temparr);
            //now we should have args[programindex][argindex]

            

            //will need chaining
            //could be array of children and single parent, but that would make piping hard
            //other way: make every child: fork (if var<progcount) -> create pipe and connect properly ->when all done pipe everything
            
            //fd[0] - output, fd[1] input
            int fd[2];
            int childid=0;
            pipe(fd);
            pid_t child = fork();
            if(!child){
                close(fd[1]); //subchilds will accept output but wont need input
                while(childid<progcount && !child && !child2){
                    childid++;
                    int fd2[2];
                    pipe(fd2);
                    pid_t child2 = fork();
                    if(!child2){
                        close(fd[1])
                        dup2(fd[0], fd2[1]);
                    }
                }
                execvp(args[progcount][0], args[progcount][0]);
            }
            //execvp(args[0], args);

            




            //free everything
            for(int i=0; i<PRGCNT; i++){
                for(int j=0; j<ARGCNT; j++){
                    free(args[i][j]);
                }
                free(args[i]);
            }
            free(args);
        }else{

            //store current line in array or smth <- done by getline 
            //below: allocating 2d array of chars, which is actually 1dim array of strings (program and arguments)
            char ** args = calloc(ARGCNT, sizeof(char*));
            for(int i=0; i<ARGCNT; i++){
                args[i]=calloc(ARGSIZE, sizeof(char));
            }
            printf((debug) ? "allocated\n" : ""); //dbg
            
            //IMPORTANT: START FROM 0, FIRST SHOULD BE PROGRAM NAME, STORE SEPARATELY
            //below: iterate over every element in line after separating by whitespace, copy tokens to array
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
                    //child code
                    //execl("/bin/ls", "ls", "-l", (char *)0); //use the one checking path, execvp i think
                    execvp(args[0], args);

                    exit(0);
                }else{
                    //parent code
                    //wait for children to finish
                    while(waitpid(child, &status, WNOHANG) == 0){
                        sleep(1);
                    }
                    
                    //if something went wrong, stop parsing
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
        }

        read = getline(&line, &len, fp);
    }
    
    fclose(fp);
    if (line){
        free(line);
    }
    printf("\n");
    exit(EXIT_SUCCESS);
}