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


// A structure to represent a queue
struct Queue
    {
        int front, rear, size;
        unsigned capacity;
        int* array;
    };
    
    // function to create a queue of given capacity. 
    // It initializes size of queue as 0
    struct Queue* createQueue(unsigned capacity)
    {
        struct Queue* queue = (struct Queue*) malloc(sizeof(struct Queue));
        queue->capacity = capacity;
        queue->front = queue->size = 0; 
        queue->rear = capacity - 1;  // This is important, see the enqueue
        queue->array = (int*) malloc(queue->capacity * sizeof(int));
        return queue;
    }
    
    // Queue is full when size becomes equal to the capacity 
    int isFull(struct Queue* queue)
    {  return (queue->size == queue->capacity);  }
    
    // Queue is empty when size is 0
    int isEmpty(struct Queue* queue)
    {  return (queue->size == 0); }
    
    // Function to add an item to the queue.  
    // It changes rear and size
    void enqueue(struct Queue* queue, int item)
    {
        if (isFull(queue))
            return;
        queue->rear = (queue->rear + 1)%queue->capacity;
        queue->array[queue->rear] = item;
        queue->size = queue->size + 1;
        printf("%d enqueued to queue\n", item);
    }
    
    // Function to remove an item from queue. 
    // It changes front and size
    int dequeue(struct Queue* queue)
    {
        if (isEmpty(queue))
            return INT_MIN;
        int item = queue->array[queue->front];
        queue->front = (queue->front + 1)%queue->capacity;
        queue->size = queue->size - 1;
        return item;
    }
    
    // Function to get front of queue
    int front(struct Queue* queue)
    {
        if (isEmpty(queue))
            return INT_MIN;
        return queue->array[queue->front];
    }
    
    // Function to get rear of queue
    int rear(struct Queue* queue)
    {
        if (isEmpty(queue))
            return INT_MIN;
        return queue->array[queue->rear];
    }
//end queue implementation (borrowed from https://www.geeksforgeeks.org/queue-set-1introduction-and-array-implementation/)


int run = 0;
int term = 0;
int replies = 0;
struct Queue *queue;

void hc_sigusr1(int signum){
    run = 1;
}

void hc_sigusr2(int signum){
    term = 1;
}

void hp_sigint(int signum){
    term = 1;
}

void hp_sigusr1(int signum, siginfo_t *sip, void *notused){
    replies++;
    //mark process which asked with siginfo
    enqueue(queue, sip->si_pid);
}

int dec(char* a){
    return strtol(a, NULL, 10);
}

int kidsrunning(int *children, int *childstatus, int size){
    int status;
    int result = 0;
    for(int i=0; i < size; i++){
        if(waitpid(children[i], &status, WNOHANG) == 0){
            result++;
        }else{
            childstatus[i]=WEXITSTATUS(status);
        }
    }
    return result;
}


int main(int argc, char* argv[]){
    if(argc != 3){
        printf("niepoprawna ilosc argumentow\n");
        exit(EXIT_FAILURE);
    }
    if(dec(argv[1]) < dec(argv[2])){
        printf("less children than requests\n");
        exit(EXIT_FAILURE);
    }


    queue = createQueue(20);

    int* children = malloc(dec(argv[1]) * sizeof(int));
    int* childstatus = malloc(dec(argv[1]) * sizeof(int));
    pid_t child;
    for(int i=0; i<dec(argv[1]); i++){
        child = fork();
        if(child<0){
            printf("error forking!\n");
            exit(EXIT_FAILURE);
        }
        if(child){
            printf("cpid: %d\n", child);
        }
        if(!child){ break; } //so we won't have n! kids running around
        children[i]=child;
    }

    if(!child){
        //child code

        struct sigaction usr1;
        usr1.sa_handler = hc_sigusr1;
        sigemptyset(&usr1.sa_mask);
        usr1.sa_flags = 0;
        sigaction(SIGUSR1, &usr1, NULL);

        struct sigaction usr2;
        usr2.sa_handler = hc_sigusr2;
        sigemptyset(&usr2.sa_mask);
        usr2.sa_flags = 0;
        sigaction(SIGUSR2, &usr2, NULL);

        sleep(1); //wait for parent to finish forking
        srandom(time(NULL));
        int timesleep = rand() % 11;
        sleep(timesleep);
        kill(getppid(), SIGUSR1); //request run permission

        while(!run && !term){ //wait for permission or termination
            sleep(1);
            kill(getppid(), SIGUSR1); //request run permission (repeated in case of miss)
        }
        if(term){
            exit(EXIT_SUCCESS);
        }
        //send back rtmin signal
        exit(timesleep);
    }else{
        //parent code

        struct sigaction usr1;
        usr1.sa_sigaction = hp_sigusr1;
        sigemptyset(&usr1.sa_mask);
        usr1.sa_flags = SA_SIGINFO;
        sigaction(SIGUSR1, &usr1, NULL);

        struct sigaction sint;
        sint.sa_handler = hp_sigint;
        sigemptyset(&sint.sa_mask);
        sint.sa_flags = 0;
        sigaction(SIGINT, &sint, NULL);

        //wait for enough requests
        while(replies < dec(argv[2]) && !term){
            sleep(1);
        }

        //now respond to every process which asked
        //while still children to accept and queue not empty
        while(kidsrunning(children, childstatus, dec(argv[1])) > 0 && !term){
            if(!isEmpty(queue)){
                int kid = dequeue(queue);
                kill(kid, SIGUSR1);
            }else{
                sleep(1);
            }
        }
        if(!term){
            for(int i=0; i<dec(argv[1]); i++){
                printf("CPID: %d status: %d\n",children[i], childstatus[i]);
            }
        }else{
            for(int i=0; i<dec(argv[1]); i++){
                kill(children[i], SIGUSR2);
            }
            free(children);
            printf("odebrano sigint\n");
            exit(EXIT_SUCCESS);
        }
    }
    free(children);
}