#include <stdlib.h>
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
#include "cstm.h"
#include "queue.h"
#include <unistd.h>

/*
NOTES
DO NOT FORGET TO DETACH MEMORY AND SEMAPHORES!!!
possibly all operations on semaphores should use undo flag in case of termination

*/


struct Queue* qctl;
int *qarr;

typedef unsigned short int ushort;
union semun  {
    int val;
    struct semid_ds *buf;
    ushort *array;
};

void intsig(int signum){
    //detach shmem
    if(shmdt(qarr)<0){perror("shmdt qarr");}
    if(shmdt(qctl)<0){perror("shmdt qctl");}

    printf("\nreceived SIGINT\n");
    exit(EXIT_SUCCESS);
}



long dec(char *num){
    return strtol(num, NULL, 10);
}


int main(int argc, char* argv[]){
    
    //the queue should be now set up by barber, attach to it
    //first the controller to obtain max_clients for proper array attaching, should be in queue controller
    int qctlkey = shmget(KEY_QCNTL, sizeof(struct Queue), 0644); if(qctlkey<0){perror("shmget qctl"); exit(EXIT_FAILURE);};
    qctl = (struct Queue*)shmat(qctlkey, (void *)0, 0); if(qctl==(struct Queue*)(-1)){perror("shmat qctl"); exit(EXIT_FAILURE);}


    int qarrkey = shmget(KEY_QUEUE, qctl->capacity * sizeof(int), 0644); if(qarrkey<0){perror("shmget qarr"); exit(EXIT_FAILURE);};
    qarr = (int*)shmat(qarrkey, (void *)0, 0); if(qarr==(int*)(-1)){perror("shmat qarr"); exit(EXIT_FAILURE);}

    int semid = semget(KEY_SEM, 0, 0666); if(semid < 0){perror("semget"); exit(EXIT_FAILURE);}
    //semphores:
    //0:waiting room (can be accessed?) def 1
    //1:client (ready?) (or signal) def 0
    //2:barber (ready?) (or signal) def 0



    //while(1){
        struct sembuf params[1];
        //decrease waiting room semaphore - customer entered, need to lock queue to get him from there (wait if blocked)
        printf("entering waiting room\n");
        params[0].sem_flg = 0; params[0].sem_num = 0; params[0].sem_op = -1;
        if (semop(semid, params, 1) == -1){perror("semop");} //no exit here?

        //check queue->capacity - queue->size to get free seats, if > 0
        if(qctl->capacity - qctl->size > 0){
            //enqueue yourself (free seats number automatically decreases)
            printf("sitting in waiting room\n");
            enqueue(qctl, qarr, getpid());

            //increase client semaphore - ready for cut (client's currently in waiting room)
            printf("ready for cut\n");
            params[0].sem_flg = 0; params[0].sem_num = 1; params[0].sem_op = 1;
            if (semop(semid, params, 1) == -1){perror("semop");} //no exit here?

            //increase waiting room semaphore - lock no longer needed
            printf("unlocking waiting room\n");
            params[0].sem_flg = 0; params[0].sem_num = 0; params[0].sem_op = 1;
            if (semop(semid, params, 1) == -1){perror("semop");} //no exit here? 

            //decrease barber semaphore - wait for barber to get client
            printf("waiting for barber\n");
            params[0].sem_flg = 0; params[0].sem_num = 2; params[0].sem_op = -1;
            if (semop(semid, params, 1) == -1){perror("semop");} //no exit here?
            //(Have hair cut here.)
            printf("having haircut\n");

        }else{ //no free seats, client leaves
            printf("no free seats\n");
            
            // increase waiting room semaphore - client leaves
            printf("leaving waiting room\n");
            params[0].sem_flg = 0; params[0].sem_num = 0; params[0].sem_op = 1;
            if (semop(semid, params, 1) == -1){perror("semop");} //no exit here?

            //(Leave without a haircut.)
        }
    //}
    intsig(0); //debug only
}