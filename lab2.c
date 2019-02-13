#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/stat.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include <sys/times.h>


typedef struct {
    unsigned len;
    char content[];
 }st_record;


st_record* create_record(unsigned n) {
    st_record* s = malloc(sizeof(st_record)+n*sizeof(char));
    if (!s) { perror("malloc create_record"); exit(EXIT_FAILURE); };
    s->len = n;
    //for (unsigned i=0; i<n; i++) s->Employe_Names[i] = NULL;
    return s;
 }

void fill_record(st_record *s){
    arc4random_buf(s->content, s->len);
}

void write_records(char* path, int n, int size){
    int fd = open(path, O_RDWR|O_CREAT, 0666);
    for(int i=0; i<n; i++){
        st_record* s=create_record(size);
        fill_record(s);
        write(fd, s->content, size);
    }
    close(fd);
}

void fcopy(char* source, char* target, int n, int bsize){
    char *buff=malloc(n*bsize*sizeof(char));
    FILE *fd = fopen(source, "r");
    if (fd == NULL) {
        perror("Failed: ");
        return;
    }
    FILE *ft = fopen(target, "w");
    if (ft == NULL) {
        perror("Failed: ");
        return;
    }
    fread(buff, bsize*sizeof(char), n, fd);
    fwrite(buff, bsize*sizeof(char), n, ft);
    fclose(fd);
    fclose(ft);
    free(buff);
    /*
    //or by block
    for(int i=0; i<n; i++){
        char* buff=malloc(bsize*sizeof(char));
        fread(buff, bsize, 1, fd);
        fwrite();
    }
    fclose(fd);
    */
}

void copy(char* source, char* target, int n, int bsize){
    int fs = open(source, O_RDONLY);
    if (fs < 0) {
        perror("Failed: ");
        return;
    }
    int ft = open(target, O_RDWR|O_CREAT, 0666);
    if (ft < 0) {
        perror("Failed: ");
        return;
    }
    char *buff=malloc(n*bsize*sizeof(char)); 
    if (read(fs, buff, n*bsize*sizeof(char)) < 0) {
        perror("Failed: ");
        return;
    }
    if (write(ft, buff, n*bsize*sizeof(char)) < 0) {
        perror("Failed: ");
        return;
    }
    close(fs);
    close(ft);
    free(buff);
}

void fsort(char* source, int n, int bsize){
    FILE *fd = fopen(source, "r+");
    if (fd == NULL) {
        perror("Failed: ");
        return;
    }
    char* rec1 = malloc(bsize*sizeof(char));
    char* rec2 = malloc(bsize*sizeof(char));
    int sorted=1;
    int begin=0;
    while(sorted!=0){
        int min=CHAR_MAX;
        sorted=0;
        fseek(fd, begin*bsize*sizeof(char), SEEK_SET);
        for(int i=begin; i<n; i++){
            fread(rec1, bsize*sizeof(char), 1, fd);
            if(rec1[0] < min){
                strncpy(rec2, rec1, bsize);
                sorted=i;
            }
        }
        if(sorted!=0){
            //swap file[begin] with file[sorted] 
            //set position to begin FIRST
            fseek(fd, begin*bsize*sizeof(char), SEEK_SET);
            fread(rec1, bsize*sizeof(char), 1, fd);
            //reset position to begin
            fseek(fd, begin*bsize*sizeof(char), SEEK_SET);
            fwrite(rec2, bsize*sizeof(char), 1, fd);
            //set position to sorted
            fseek(fd, sorted*bsize*sizeof(char), SEEK_SET);
            fwrite(rec1, bsize*sizeof(char), 1, fd);
        }
        begin++;
    }

    fclose(fd);
    free(rec1);
    free(rec2);
}

void sort(char* source, int n, int bsize){
    int fd = open(source, O_RDWR);
    if (fd < 0) {
        perror("Failed: ");
        return;
    }

    char* rec1 = malloc(bsize*sizeof(char));
    char* rec2 = malloc(bsize*sizeof(char));
    int sorted=1;
    int begin=0;
    while(sorted!=0){
        int min=CHAR_MAX;
        sorted=0;
        lseek(fd, begin*bsize*sizeof(char), SEEK_SET);
        for(int i=begin; i<n; i++){
            read(fd, rec1, bsize*sizeof(char));
            if(rec1[0] < min){
                strncpy(rec2, rec1, bsize);
                sorted=i;
            }
        }
        if(sorted!=0){
            //swap file[begin] with file[sorted] 
            //set position to begin FIRST
            lseek(fd, begin*bsize*sizeof(char), SEEK_SET);
            read(fd, rec1, bsize*sizeof(char));
            //reset position to begin
            lseek(fd, begin*bsize*sizeof(char), SEEK_SET);
            write(fd, rec2, bsize*sizeof(char));
            //set position to sorted
            lseek(fd, sorted*bsize*sizeof(char), SEEK_SET);
            write(fd, rec1, bsize*sizeof(char));
        }
        begin++;
    }

    close(fd);
    free(rec1);
    free(rec2);
}

int main(int argc, char * argv[]){
    /*
    st_record* s = create_record(10);
    fill_record(s);
    for(int i=0; i<10; i++){
        printf("%d ",s->content[i]);
    }
    printf("\n");
    free(s);
    char path[]="/Users/Antek/Desktop/out";
    char path2[]="/Users/Antek/Desktop/outcpfs";
    char path3[]="/Users/Antek/Desktop/outcps";
    
    write_records(path, 5, 10);
    fcopy(path, path2, 5, 10);
    fsort(path2, 5, 10);
    copy(path, path3, 5, 10);
    sort(path3, 5, 10);
    //first part works
    */

    //interpret cli parameters here
    clock_t start, end;
    struct tms t_start, t_end;
    double cpu_time_used;
    //printf("%s %d %d\n", argv[2], (int)strtol(argv[3], NULL, 10), (int)strtol(argv[4], NULL, 10)); //dbg

    if(strncmp(argv[1], "generate", 10)==0){
        if(argc!=5){
            printf("wrong arguments. requires 3 arguments.\n");
        }else{
            //printf("%s %d %d\n", argv[2], (int)argv[3], (int)argv[4]); //dbg
            start = clock();
            times(&t_start);
            write_records(argv[2], (int)strtol(argv[3], NULL, 10), (int)strtol(argv[4], NULL, 10));
            times(&t_end);
            end = clock();
            cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
            printf("created.\ntime: %f\n", cpu_time_used);
            printf("usertime: %lu cputime: %jd\n", t_end.tms_cutime - t_start.tms_cutime, t_end.tms_stime - t_start.tms_stime);
        }
    }else if(strncmp(argv[1], "sort", 4)==0){
        //./program sort dane 100 512 sys
        if(argc==6){
            if(strncmp(argv[5], "sys", 3)==0){
                start = clock();
                times(&t_start);
                sort(argv[2], (int)strtol(argv[3], NULL, 10), (int)strtol(argv[4], NULL, 10));
                end = clock();
                times(&t_end);
                cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
                printf("sorted.\ntime: %f\n", cpu_time_used);
                printf("usertime: %lu cputime: %jd\n", t_end.tms_cutime - t_start.tms_cutime, t_end.tms_stime - t_start.tms_stime);
            }else if(strncmp(argv[5], "lib", 3)==0){
                start = clock();
                times(&t_start);
                fsort(argv[2], (int)strtol(argv[3], NULL, 10), (int)strtol(argv[4], NULL, 10));
                end = clock();
                times(&t_end);
                cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
                printf("fsorted.\ntime: %f\n", cpu_time_used);
                printf("usertime: %lu cputime: %jd\n", t_end.tms_cutime - t_start.tms_cutime, t_end.tms_stime - t_start.tms_stime);
            }else{
                printf("invalid argument: %s\n", argv[6]);
            }
        }else{
            printf("wrong arguments. requires 4 arguments.\n");
        }
    }else if(strncmp(argv[1], "copy", 4)==0){
        //./program copy plik1 plik2 100 512 sys
        if(argc==7){
            if(strncmp(argv[6], "sys", 3)==0){
                start = clock();
                times(&t_start);
                copy(argv[2], argv[3], (int)strtol(argv[4], NULL, 10), (int)strtol(argv[5], NULL, 10));
                end = clock();
                times(&t_end);
                cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
                printf("copied.\ntime: %f\n", cpu_time_used);
                printf("usertime: %lu cputime: %jd\n", t_end.tms_cutime - t_start.tms_cutime, t_end.tms_stime - t_start.tms_stime);
            }else if(strncmp(argv[6], "lib", 3)==0){
                start = clock();
                times(&t_start);
                fcopy(argv[2], argv[3], (int)strtol(argv[4], NULL, 10), (int)strtol(argv[5], NULL, 10));
                end = clock();
                times(&t_end);
                cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
                printf("fcopied.\ntime: %f\n", cpu_time_used);
                printf("usertime: %lu cputime: %jd\n", t_end.tms_cutime - t_start.tms_cutime, t_end.tms_stime - t_start.tms_stime);
            }else{
                printf("invalid argument: %s\n", argv[6]);
            }
        }else{
            printf("wrong arguments. requires 4 arguments.\n");
        }
    }else{
        printf("wrong command\n");
    }
}
//stuf to do:
//generate - write_records(sciezka, ilosc, dlugosc)
//fsort / sort - sort/fsort(srcpath, #record, size)
//fcopy / copy - fcopy/copy(srcpath, trgpath, #record, sizeofrecords)