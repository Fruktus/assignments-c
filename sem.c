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
#include <semaphore.h>
#include "queue.h"

/*

producer checks the conditional if freeID is empty, if not tries to get the mutex on queue
if acquired lock takes index from queue automatically changing indexes and stuff
if did not needs to go back to beginning (because the conditional checked before might have changed) (or can it? need to check)

after succesfully filling array at given index monitors conditional at takenID to see if its full, if not similarly, tries to get the mutex
if got lock adds processed index to queue
if did not repeats checking monitor

NOTE: Monitors are mutex-safe, no need to manually check again

similarly for consumer

NOTES
check how conditionals work
processes could try getting mutexes before checking full/empty but they would need to poll for it

IMPORTANT
after finishing the file there should be some way to tell producers and consumers to stop after finishing with what theyre left with, ex
any producer waiting to do something should stop
if consumers still have something in queue, they should continue until empty, but then instead of waiting also stop
and then return control to main
PROTIP for above - if implemented using global flag, remember to make it volatile

reading file also needs mutex, since it moves the pointer needed for calculating size of string which requires two vars, 
every thread needs them to be updated for that

every thread need to cleanup its mutex after cancel
there might be problems if some threadd won't give back the index it was working on

in case of time cancelling the threads wont have time to finish what they were doing so they can just get cancelled,
but they needd to release muxes

if encountering read from file error, the threads might not update finished in time and try to read anyway
*/
volatile int finished = 0;
volatile long long lastpos = 0;
Queue *freeId;
Queue *takenId;
char **bufferArr;
FILE* file;
int *tid;

pthread_t *threadsArr;
// pthread_mutex_t freeIdMux;
// pthread_mutex_t takenIdMux;
// pthread_mutex_t fileMux;

// pthread_cond_t freeIdMon;
// pthread_cond_t takenIdMon;
sem_t freeIdSem;
sem_t takenIdSem;
sem_t fileSem;
sem_t freeAmountSem;
sem_t takenAmountSem;




//replace with read from file
//const char *PATH = "/home/fruktus/Desktop/test.txt"; 
int PRCNT = 5;
int CLCNT = 5;
int BUFFSIZE = 10;
char PATH[64];
int LEN;
char MODE = '=';
int VMODE = 0;
int NK;

void loadParam(const char *p){
    FILE *configFile = fopen(p, "r");
    if (configFile == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    fscanf(configFile, "%d %d %d %s %d %c %d %d", &PRCNT, &CLCNT, &BUFFSIZE, PATH, &LEN, &MODE, &VMODE, &NK);
    printf("%s\n", PATH);
    fclose(configFile);
}


void cleanBuffer(char **buf, int size){
    for(int i=0; i<size; i++){
        if(buf[i] != NULL){
            free(buf[i]);
        }
    }
    free(buf);
}

void intsig(int signum){
    //cleanup
    //should kill all of its threads when ill get to them

    free(tid);
    // pthread_mutex_destroy(&freeIdMux);
    // pthread_cond_destroy(&freeIdMon);
    // pthread_mutex_destroy(&takenIdMux);
    // pthread_cond_destroy(&takenIdMon);
    // pthread_mutex_destroy(&fileMux);
    free(threadsArr);
    destroyQueue(freeId);
    destroyQueue(takenId);
    cleanBuffer(bufferArr, BUFFSIZE);
    printf("\nreceived SIGINT\n");
    exit(EXIT_SUCCESS);
}

void fillId(Queue *q){
    for(int i = 0; i< q->capacity; i++){
        enqueue(q, i);
    }
}

long dec(char *num){
    return strtol(num, NULL, 10);
}

int comp(char *s, int l, char m){
    if(m == '>'){
        return strlen(s) > l;
    }
    if(m == '<'){
        return strlen(s) < l;
    }
    if(m == '='){
        return strlen(s) == l;
    }
    return 0; //error?
}

void* producer(void *param){ //add pops and pushes!
    int id = *(int*)param;
    while(!finished){
        //monitor when queue not empty
        if(VMODE)printf("p%d: waiting lock freeId\n",id);
        sem_wait(&freeAmountSem);
        if(VMODE)printf("p%d: lock freeId\n",id); //dbg

        //lock the queue
        sem_wait(&freeIdSem);
        //get index from queue
        int ID = dequeue(freeId);
        if(VMODE)printf("p%d: ID %d\n",id, ID);

        //unlock queue
        sem_post(&freeIdSem);
        if(VMODE)printf("p%d: unlock freeId\n",id);

        //lock file
        if(VMODE)printf("p%d: waiting lock file\n",id);
        sem_wait(&fileSem);
        if(VMODE)printf("p%d: lock file\n",id);//dbg

        //get line, if last finish=1
        char line[1024];
        if(!finished){
            if (fgets(line, sizeof(line), file) == NULL){ //does not strip \n
                finished = 1;
                fclose(file); //no problem with closing, only one uses it at a time

                //unlock file
                sem_post(&fileSem);
                if(VMODE)printf("p%d: unlock file\n",id);
            }else{
                if(VMODE)printf("p%d: line %s",id, line);//dbg

                //update global currentpos
                long long currpos = ftell(file);
                long long size = currpos - lastpos; //local
                lastpos = currpos;
                line[size+1]='\0';

                //unlock file
                sem_post(&fileSem);
                if(VMODE)printf("p%d: unlock file\n",id);

                //malloc for calculated strlen +1
                char *str = malloc((size+1)*sizeof(char));
                //str[size+1]='\0';
                strncpy(str, line, size+1); //im not sure anymore

                //insert returned pointer to buffer at acquired earlier index
                bufferArr[ID] = str;
                
                //lock takenIdQueue
                if(VMODE)printf("p%d: waiting lock takenId\n",id);
                sem_wait(&takenIdSem);
                if(VMODE)printf("p%d: lock takenId\n",id);

                //insert (there should be no way of overflow, since there is only as many inddexes)
                enqueue(takenId, ID);
                sem_post(&takenAmountSem);

                //unlock takenIdQueue
                sem_post(&takenIdSem);
                if(VMODE)printf("p%d: unlock takenId\n",id);
            }
        }else{
            sem_post(&fileSem);
            sem_post(&freeAmountSem);
            sem_post(&takenAmountSem);
        }
    }
    return NULL;
}

void* consumer(void *param){ //add pushes and pops
    int id = *(int*)param;
    while(!finished){
        //monitor when takenId not empty and lock when so
        if(VMODE)printf("c%d: waiting lock takenId\n",id);
        sem_wait(&takenAmountSem);
        if(VMODE)printf("c%d: lock takenId\n",id);

        if(!finished){
            //lock queue
            sem_wait(&takenIdSem);
            //get index
            int ID = dequeue(takenId);
            if(VMODE)printf("c%d: ID %d\n",id, ID);

            //unlock
            sem_post(&takenIdSem);
            if(VMODE)printf("c%d: unlock takenId\n",id);

            //make local copy of pointer, NULL buffer at given index
            char *strptr = bufferArr[ID];
            bufferArr[ID] = NULL;
            if(VMODE)printf("c%d: str %s",id, strptr);

            //lock freeIdQueue
            if(VMODE)printf("c%d: waiting lock freeId\n",id);
            sem_wait(&freeIdSem);
            if(VMODE)printf("c%d: unlock freeId\n",id);

            //insert index
            enqueue(freeId, ID);
            sem_post(&freeAmountSem);

            //unlock
            sem_post(&freeIdSem);
            if(VMODE)printf("c%d: unlock freeId\n",id);

            //do whatever was supposed to be done with the index
            //for dbg just print this
            //printf("%s", strptr);
            if(comp(strptr, LEN, MODE)){
                printf("%s", strptr);
            }

            //free the memory pointed by local var
            free(strptr);
        }else{
            //unlock
            sem_post(&freeAmountSem);
            sem_post(&takenIdSem);
            if(VMODE)printf("c%d: unlock takenId\n",id);
        }
    }

    //here another loop to iterate over anything left in consumers queue?
    //i think that if there was something left, the last active producer would've woken enough threads so no need for that 

    return NULL;
}



int main(int argc, char const *argv[])
{   
    if(argc!=2){printf("missing config file\n");exit(EXIT_FAILURE);}
    loadParam(argv[1]);
    signal(SIGINT, intsig);
    freeId = createQueue(BUFFSIZE);
    takenId = createQueue(BUFFSIZE);
    fillId(freeId); // add indexes to queue
    bufferArr = malloc(BUFFSIZE * sizeof(char*)); //array for strings from file
    tid = malloc((PRCNT+CLCNT) * sizeof(int));

    // pthread_mutex_init(&freeIdMux, NULL);
    // pthread_mutex_init(&takenIdMux, NULL);
    // pthread_mutex_init(&fileMux, NULL);

    // pthread_cond_init(&freeIdMon, NULL);
    // pthread_cond_init(&takenIdMon, NULL);
    sem_init(&freeIdSem, 0, 1); //access
    sem_init(&takenIdSem, 0, 1); //access
    sem_init(&fileSem, 0, 1); //access
    sem_init(&freeAmountSem, 0, BUFFSIZE); //amount 
    sem_init(&takenAmountSem, 0, 0);


    threadsArr = malloc((PRCNT+CLCNT) * sizeof(pthread_t));
    file = fopen(PATH, "r");if(file == NULL){perror("fopen2"); exit(EXIT_FAILURE);} //global to move pointer

    for (int i = 0; i < PRCNT; i++) {
        tid[i]=i;
        pthread_create(&threadsArr[i], NULL, (void*)&producer, &tid[i]);        
    }
    for(int i=PRCNT; i<PRCNT+CLCNT; i++){
        tid[i]=i;
        pthread_create(&threadsArr[i], NULL, (void*)&consumer, &tid[i]);
    }


    for (int i = 0; i < PRCNT+CLCNT; i++) {
        pthread_join(threadsArr[i], NULL);
    }

    
    

    // pthread_mutex_destroy(&freeIdMux);
    // pthread_cond_destroy(&freeIdMon);
    // pthread_mutex_destroy(&takenIdMux);
    // pthread_cond_destroy(&takenIdMon);
    // pthread_mutex_destroy(&fileMux);

    free(tid);
    free(threadsArr);
    destroyQueue(freeId);
    destroyQueue(takenId);
    cleanBuffer(bufferArr, BUFFSIZE);
    exit(EXIT_SUCCESS);
}

