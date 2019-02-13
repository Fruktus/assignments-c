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
int main(int argc, char* argv[]){
   /* char name[]="/Users/Antek/Desktop/Notatki";
    char path[]="/Users/Antek/Desktop/Notatki";
    DIR* dirp = opendir(path);
    if(dirp==NULL){
        printf("isnull\n");
    }
struct dirent* dp = readdir(dirp);
    while (dp!=NULL) {
        
        printf("%d %d %d %s\n", dp->d_type, dp->d_reclen, dp->d_ino, dp->d_name);
        dp = readdir(dirp);
    }
     closedir(dirp);
     */
    /*

    printf("Information for %s\n",argv[1]);
    printf("---------------------------\n");
    printf("File Size: \t\t%lld bytes\n",fileStat.st_size);
    printf("Number of Links: \t%hu\n",fileStat.st_nlink);
    printf("File inode: \t\t%llu\n",fileStat.st_ino);
    printf("Modification time: \t\t%s\n",ctime(&fileStat.st_mtime));
 
    printf("File Permissions: \t");
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
    printf("\n\n");
 
    printf("The file %s a symbolic link\n", (S_ISLNK(fileStat.st_mode)) ? "is" : "is not");

    */

    if(argc != 4){
        printf("wrong params\n");
        exit(0);
    }
    char* path = argv[1];
    char* oper = argv[2];
    char* date = argv[3];
    char name_buf[1024];
    
    DIR* dirp = opendir(path);
    if(dirp==NULL){
        printf("isnull\n");
    }

    struct dirent* dp = readdir(dirp);
    while (dp!=NULL) {
        struct stat fileStat;
        sprintf(name_buf, "%s/%s", argv[1], dp->d_name);

        if(stat(name_buf, &fileStat) < 0){
            printf("error\n");
            return 1;
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
        }

        dp = readdir(dirp);
    }
    closedir(dirp);

    //when comparing dates use it like comparator(compared, parameter, operation), if == 1 print
}