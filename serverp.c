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
#include <mqueue.h>
#include "cstmsg.h"

#define MAX_CLIENTS 20

/*
 *
 *  11 maja praktyczne kolokwium
 *  3 zadania, znalezc blad, napisac jakas funkcje, bedzie mozna uzywac manuala
 *
 */

mqd_t mq;
mqd_t clients[MAX_CLIENTS];
int client_count = 0;


void intsig(int signum){
    printf("\nreceived SIGINT\n");
    //struct message *response = malloc(sizeof(response));
    //response->mtype=MSG_END;
    //for(int i=0; i<client_count; i++){
    //    msgsnd(clients[i], response, sizeof(response->mtext), 0);
    //}
    //zamknij kolejke
    //if(msgctl(msgget(SRV_KEY, 0660), IPC_RMID, NULL) < 0){
    //    perror("msgctl");
    //}
    if(mq_close(mq) < 0){
        perror("mqclose");
    }
    if(mq_unlink(QUEUE_NAME) < 0){
        perror("mqunlink");
    }
    exit(EXIT_SUCCESS);
}

long dec(char *num){
    return strtol(num, NULL, 10);
}

void inplace_reverse(char * str){
  if (str)
  {
    char * end = str + strlen(str) - 1;

    // swap the values in the two given variables
    // XXX: fails when a and b refer to same memory location
    #define XOR_SWAP(a,b) do\
    {\
      a ^= b;\
      b ^= a;\
      a ^= b;\
    } while(0)

    // walk inwards from both ends of the string, 
    // swapping until we get to the middle
    while (str < end)
    {
      XOR_SWAP(*str, *end);
      str++;
      end--;
    }
    #undef XOR_SWAP
  }
}//src: https://stackoverflow.com/questions/784417/reversing-a-string-in-c?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa








int main(int argc, char* argv[]){
    signal(SIGINT, intsig);


    
    struct mq_attr attr;
    char buffer[MAX_BUF];

    attr.mq_flags = 0;
    attr.mq_maxmsg = 20;
    attr.mq_msgsize = MAX_SIZE;
    attr.mq_curmsgs = 0;

    mq = mq_open(QUEUE_NAME, O_CREAT | O_RDONLY, 0644, &attr);
    if(mq < 0){
        perror("mqueue");
        exit(EXIT_FAILURE);
    }

    printf("server start\n");
    while(1){
        //read msg
        ssize_t bytes_read;
        bytes_read = mq_receive(mq, buffer, MAX_SIZE, NULL);
        //CHECK(bytes_read >= 0);
        buffer[bytes_read] = '\0';
        printf("Received: %s\n", buffer);

        if(bytes_read > 0){
            printf("received message: %ld\n", dec(buffer[0])); //dbg
            //buffer: XCCC....    -  x-polecenie, ccc-id clienta
            switch(dec(buffer[0])){
                case MSG_END:{
                        char *response = malloc(MAX_BUF * sizeof(char));
                        response[0] = MSG_END + '0'; //remember, char array!
                        for(int i=0; i<client_count; i++){
                            //msgsnd(clients[i], response, sizeof(response->mtext), 0);
                            if(mq_send(clients[i], response, MAX_BUF, 0) < 0){
                                perror("mqsend");
                            }
                            if(mq_close(clients[i]) < 0){
                                perror("mqclose");
                            }

                        }
                        free(response);
                        printf("exiting\n");
                        //zamknij kolejke
                        if(mq_close(mq) < 0){
                            perror("mqclose");
                        }
                        if(mq_unlink(QUEUE_NAME) < 0){
                            perror("mqunlink");
                        }
                    }
                break;

                case MSG_KEY:
                    if(client_count < MAX_CLIENTS){
                        //save client queue id
                        printf("received key: %s\n", buffer+1);
                        clients[client_count] = mq_open(dec(buffer+1), O_WRONLY, 0644, &attr);

                        char *response = malloc(MAX_BUF * sizeof(char));
                        response[0] = MSG_KEY + '0';

                        char id[4];
                        //generate string id for client
                        id[0] = client_count/100 + '0';
                        id[1] = client_count/10%10 + '0';
                        id[2] = client_count%10 +'0';
                        id[3] = '\0';

                        printf("reply id: %s\n", id);
                        sprintf(response+1, "%s", id);
                        if(mq_send(clients[client_count], response, MAX_BUF, 0) < 0){
                                perror("mqsend");
                        }
                        free(response);
                        client_count++;
                    }else{
                        printf("exceeded maximum clients\n");
                    }
                break;

                case MSG_MIRROR:
                    {
                        char *response = malloc(MAX_BUF * sizeof(char));
                        response[0] = MSG_MIRROR + '0';
                        //for the sake of simplicity I'll use the first three digits to represent clientid
                        char id[4];
                        id[3]='\0';
                        strncpy(id, buffer+1, 3);
                        int clientid = dec(id);
                        printf("cid: %ld\n", dec(id));
                        //need to rework reverse
                        sprintf(response+1, "%s", buffer+4);
                        inplace_reverse(response+1);
                        printf("sending mirror\n");

                        if(mq_send(clients[clientid], response, MAX_BUF, 0) < 0){
                                perror("mqsend");
                        }
                        free(response);
                    }
                break;

                case MSG_CALC:
                    
                break;

                case MSG_TIME:{
                    char *response = malloc(MAX_BUF * sizeof(char));   
                    time_t current_time;
                    char* c_time_string;

                    current_time = time(NULL);
                    c_time_string = ctime(&current_time);
                    char *newline = strchr( c_time_string, '\n' );  //remove newline
                    if(newline){
                        *newline = 0;
                    }

                    response[0] = MSG_TIME ++ '0';
                    printf("time: %s\n", c_time_string);
                    sprintf(response+1, "%s", c_time_string);
                    char id[4];
                    id[3] = '\0';
                    strncpy(id, buffer+1, 3);
                    int clientid = dec(id);
                    if(mq_send(clients[clientid], response, MAX_BUF, 0) < 0){
                                perror("mqsend");
                    }
                    free(response);
                }
                break;
            }
        }
        sleep(1);
    }

   
    if(mq_close(mq) < 0){
        perror("mqclose");
    }
    if(mq_unlink(QUEUE_NAME) < 0){
        perror("mqunlink");
    }
    //usun swoja kolejke
    exit(EXIT_SUCCESS);
}
