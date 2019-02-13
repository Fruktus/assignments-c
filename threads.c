#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/resource.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/sem.h> 
#include <sys/ipc.h>
#include <sys/shm.h>
#include <math.h>
#include <pthread.h>

#ifndef max
	#define max( a, b ) ( ((a) > (b)) ? (a) : (b) )
#endif

//source: https://ugurkoltuk.wordpress.com/2010/03/04/an-extreme-simple-pgm-io-api/
//i had to rewrite most of it though because as it was it couldnt properly read file
typedef struct _PGMData {
    int row;
    int col;
    int max_gray;
    int **matrix;
} PGMData;

typedef struct _FilterData {
    int size;
    double **matrix;
} FilterData;


PGMData* src;
FilterData* flt;
PGMData* trg;
int tcnt;

long dec(char *num){
    return strtol(num, NULL, 10);
}

int **allocate_dynamic_matrix(int row, int col){
    int **ret_val;
    int i;
 
    ret_val = (int **)malloc(sizeof(int *) * row);
    if (ret_val == NULL) {
        perror("memory allocation failure");
        exit(EXIT_FAILURE);
    }
 
    for (i = 0; i < row; ++i) {
        ret_val[i] = (int *)malloc(sizeof(int) * col);
        if (ret_val[i] == NULL) {
            perror("memory allocation failure");
            exit(EXIT_FAILURE);
        }
    }
 
    return ret_val;
}

double **allocate_dynamic_matrixf(int size){
    double **ret_val;
    int i;
    int row = size;
    int col = size;
 
    ret_val = (double **)malloc(sizeof(double *) * row);
    if (ret_val == NULL) {
        perror("memory allocation failure");
        exit(EXIT_FAILURE);
    }
 
    for (i = 0; i < row; ++i) {
        ret_val[i] = (double *)malloc(sizeof(double) * col);
        if (ret_val[i] == NULL) {
            perror("memory allocation failure");
            exit(EXIT_FAILURE);
        }
    }
 
    return ret_val;
}
 
void deallocate_dynamic_matrix(int **matrix, int row){
    int i;
 
    for (i = 0; i < row; ++i) {
        free(matrix[i]);
    }
    free(matrix);
}

void deallocate_dynamic_matrixf(double **matrix, int size){
    int i;
    int row = size;
 
    for (i = 0; i < row; ++i) {
        free(matrix[i]);
    }
    free(matrix);
}

int isspace(int c){
    if(c == ' ' || c == 12 || c == 9){ //(blanks, TABs, CRs, LFs)
        return 1;
    }
    return 0;
}

void SkipComments(FILE *fp){
    int ch;
    char line[100];
    while ((ch = fgetc(fp)) != EOF && isspace(ch)) {
        ;
    }
 
    if (ch == '#') {
        fgets(line, sizeof(line), fp);
        SkipComments(fp);
    } else {
        fseek(fp, -1, SEEK_CUR);
    }
} 

/*for reading:*/
PGMData* readPGM(const char *file_name, PGMData *data){
    FILE *pgmFile;
    char version[3];
    int i, j;
    pgmFile = fopen(file_name, "rb");
    if (pgmFile == NULL) {
        perror("cannot open file to read");
        exit(EXIT_FAILURE);
    }
    fgets(version, sizeof(version), pgmFile);
    if (strncmp(version, "P5", 3) == 0) {
        fprintf(stderr, "Wrong file type!\n");
        exit(EXIT_FAILURE);
    }
    SkipComments(pgmFile); //(blanks, TABs, CRs, LFs)
    fscanf(pgmFile, "%d", &data->col);
    SkipComments(pgmFile);
    fscanf(pgmFile, "%d", &data->row);
    SkipComments(pgmFile);
    fscanf(pgmFile, "%d", &data->max_gray);
 
    data->matrix = allocate_dynamic_matrix(data->row, data->col);
    if (data->max_gray > 255) {
        printf("not supported\n");
        exit(EXIT_FAILURE);
    }
    else {
        for (i = 0; i < data->row; ++i) {
            for (j = 0; j < data->col; ++j) {
                int num;
                fscanf(pgmFile, "%d", &num);
                data->matrix[i][j] = num;
                //printf("%d ", data->matrix[i][j]); //debug
            }
            //printf("\n");
        }
    }
    fclose(pgmFile);
    return data;
}

FilterData* readFLT(const char *file_name, FilterData *data){
    FILE *pgmFile;
    int i, j;
    pgmFile = fopen(file_name, "rb");
    if (pgmFile == NULL) {
        perror("cannot open file to read");
        exit(EXIT_FAILURE);
    }
    SkipComments(pgmFile); //(blanks, TABs, CRs, LFs)
    fscanf(pgmFile, "%d", &data->size);
    SkipComments(pgmFile);

 
    data->matrix = allocate_dynamic_matrixf(data->size);
    for (i = 0; i < data->size; ++i) {
        for (j = 0; j < data->size; ++j) {
            double num;
            fscanf(pgmFile, "%lf", &num);
            data->matrix[i][j] = num;
            printf("%lf ", data->matrix[i][j]); //dbg
        }
        printf("\n"); //dbg
    }
    fclose(pgmFile);
    return data;
}
 
/*and for writing*/
void writePGM(const char *filename, const PGMData *data){
    FILE *pgmFile;
    int i, j;
 
    pgmFile = fopen(filename, "wb");
    if (pgmFile == NULL) {
        perror("cannot open file to write");
        exit(EXIT_FAILURE);
    }
 
    fprintf(pgmFile, "P2 \n");
    fprintf(pgmFile, "%d %d\n", data->col, data->row);
    fprintf(pgmFile, "%d\n", data->max_gray);

    if (data->max_gray > 255) {
        for (i = 0; i < data->row; ++i) {
            for (j = 0; j < data->col; ++j) {
                printf("not supported\n");
                exit(EXIT_FAILURE);
            }
 
        }
    }
    else {
        for (i = 0; i < data->row; ++i) {
            for (j = 0; j < data->col; ++j) {
                int x = data->matrix[i][j];
                if(x <= 9){
                    fputc(x + '0', pgmFile);
                    printf("%c ", x + '0');
                }else if(x <= 99){
                    fputc(x/10 + '0', pgmFile);
                    fputc(x%10 + '0', pgmFile);
                    printf("%c%c ", x/10 + '0', x%10 + '0');
                }else{
                    fputc(x/100 + '0', pgmFile);
                    fputc(x/10%10 + '0', pgmFile);
                    fputc(x%10 + '0', pgmFile);
                    printf("%c%c%c ", x/100 + '0', x/10%10 + '0', x%10 + '0');
                }
                fputc(' ', pgmFile);
            }
            fputc('\n', pgmFile);
            printf("\n");
        }
    }
 
    fclose(pgmFile);
    deallocate_dynamic_matrix(data->matrix, data->row);
}

int calcPx(int x, int y, int c, int **intarr, double **dblarr){
    double sum = 0;
    for(int i=0; i<c; i++){
        for(int j=0; j<c; j++){
            int a = max(1, x-ceil(c/2)+i);
            int b = max(1, y-ceil(c/2)+j);
            //printf("a:%d b:%d\n", a, b);
            sum += intarr[a][b];
            sum *= dblarr[i][j];
        }
    }
    return round(sum);
}

void* threadFunc(void *param){
    int *id = (int*)param;
    //printf("id:%d\n", *id);
    int block = floor(flt->size/tcnt);
    //printf("bl:%d\n", block);
    for(int i=block*(*id); i<block+(*id)*block; i++){
        for(int j=0; j<flt->size; j++){
            //printf("i:%d j:%d\n", i, j);
            trg->matrix[i][j] = calcPx(i, j, flt->size, src->matrix, flt->matrix);
            //printf("trg:%d\n", trg->matrix[i][j]);
        }
    }
    return NULL;
}

/*
ARGV:
    liczbę wątków,
    nazwę pliku z wejściowym obrazem,
    nazwę pliku z definicją filtru,
    nazwę pliku wynikowego.
*/
int main(int argc, char* argv[]){
    if(argc!=5){printf("missing params\n");exit(EXIT_FAILURE);}
    tcnt = dec(argv[1]);
    const char *srcp = argv[2];
    const char *fltp = argv[3];
    const char *trgp = argv[4];
    pthread_t *tid = malloc(tcnt*sizeof(pthread_t ));
    int *ids = malloc(tcnt*sizeof(int));

    src = malloc(sizeof(PGMData));
    src = readPGM(srcp, src); //load  source image

    flt = malloc(sizeof(FilterData));
    flt = readFLT(fltp, flt); 

    trg = malloc(sizeof(PGMData));
    trg->col=flt->size; trg->row=flt->size;
    trg->max_gray=src->max_gray;
    trg->matrix = allocate_dynamic_matrix(flt->size, flt->size);
    
    printf("col:%d row:%d\n", src->col, src->row);
    //split into threads, each should do columns/tcnt columns (or rows)
    //prepare function to calculate field for each thread
    for(int i=0; i<tcnt; i++){
        ids[i]=i;
        pthread_create(&tid[i], NULL, threadFunc, &ids[i]);
    }

    for(int i=0; i<tcnt; i++){
        pthread_join(tid[i], NULL);
    }

    //join loop?

    writePGM(trgp, trg); //will autoclose
    
    deallocate_dynamic_matrix(src->matrix, src->row); //won't write this one
    deallocate_dynamic_matrixf(flt->matrix, flt->size); //won't write this one
    free(src);
    free(flt);
    free(trg);
    free(ids);
    free(tid);

    exit(EXIT_SUCCESS);
}