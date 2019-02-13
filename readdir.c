#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>


int comparator(unsigned long long a, unsigned long long b, char mode){
    if(strncmp(&mode, ">", 1)){
        if(a > b){
            return 1;
        }
        return 0;
    }else if(strncmp(&mode, "=", 1)){
        if(a == b){
            return 1;
        }
        return 0;
    }else{
        if(a < b){
            return 1;
        }
        return 0;
    }
}

void aux_traverse(char* path, char* oper, char* date);

void traverse(char* path, char* oper, char* date){
    char name_buf[1024];

    DIR* dirp = opendir(path);
    if(dirp==NULL){
        printf("isnull\n");
    }

    struct dirent* dp = readdir(dirp);
    while (dp!=NULL) {
        struct stat fileStat;
        sprintf(name_buf, "%s/%s", path, dp->d_name);

        //dbg


        if(lstat(name_buf, &fileStat) < 0){
            printf("error\n");
            return;
        } 
        if(!S_ISLNK(fileStat.st_mode) && comparator(fileStat.st_mtime,strtol(date, NULL, 10),oper[0])==1){
            printf( (S_ISDIR(fileStat.st_mode)) ? "d" : "-");
            printf( (fileStat.st_mode & S_IRUSR) ? "r" : "-");
            printf( (fileStat.st_mode & S_IWUSR) ? "w" : "-");
            printf( (fileStat.st_mode & S_IXUSR) ? "x" : "-");
            printf( (fileStat.st_mode & S_IRGRP) ? "r" : "-");
            printf( (fileStat.st_mode & S_IWGRP) ? "w" : "-");
            printf( (fileStat.st_mode & S_IXGRP) ? "x" : "-");
            printf( (fileStat.st_mode & S_IROTH) ? "r" : "-");
            printf( (fileStat.st_mode & S_IWOTH) ? "w" : "-");
            printf( (fileStat.st_mode & S_IXOTH) ? "x" : "-");
            printf(" %s \t\t%lld bytes Last Modification: \t\t%s\n", name_buf, fileStat.st_size, ctime(&fileStat.st_mtime));
            
            if(S_ISDIR(fileStat.st_mode) && strncmp(dp->d_name,".",1)){
                pid_t child_pid = vfork();
                //printf("process: %d, child: %d\n", (int)getpid(), (int)child_pid);
                if((int)child_pid==0){
                    traverse(name_buf, oper, date); //call recurrently
                    exit(0);
                }
            }     
        }
        
        dp = readdir(dirp);
    }
    closedir(dirp);
}


int main(int argc, char* argv[]){
    if(argc != 4){
        printf("wrong params\n");
        exit(0);
    }
    char* path = argv[1];
    char* oper = argv[2];
    char* date = argv[3];
    traverse(path, oper, date);
    
    

    //when comparin dates use it like comparator(compared, parameter, operation), if == 1 print
}