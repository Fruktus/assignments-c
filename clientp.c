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

mqd_t cmq;
mqd_t mq;
char queue_client[sizeof(CLIENT_QUEUE + 3)];


void intsig(int signum){
    printf("\nreceived SIGINT\n");
    //if(msgctl(msgget(key, 0660), IPC_RMID, NULL) < 0){
    //    perror("msgctl");
    //}
    if(mq_close(mq) < 0){
        perror("mqclose");
    }
    if(mq_close(cmq) < 0){
        perror("mqclose");
    }

    //change to clients queue name
    if(mq_unlink(queue_client) < 0){
        perror("mqunlink");
    }
    exit(EXIT_SUCCESS);
}


long dec(char *num){
    return strtol(num, NULL, 10);
}


int main(int argc, char* argv[]){
    signal(SIGINT, intsig);

    //stworz kolejke, wyslij jej klucz serwerowi, po otrzymaniu id przejdz do petli
    srand(time(NULL));
    int rnd = rand()%100;
    strcpy(queue_client, CLIENT_QUEUE);
    strcat(queue_client, rnd + '0');


    struct mq_attr attr;

    attr.mq_flags = 0;
    attr.mq_maxmsg = 20;
    attr.mq_msgsize = MAX_SIZE;
    attr.mq_curmsgs = 0;

    cmq = mq_open(queue_client, O_CREAT | O_RDONLY, 0644, &attr);
    if(cmq < 0){
        perror("mqueue");
        exit(EXIT_FAILURE);
    }
    mq = mq_open(QUEUE_NAME, O_WRONLY, 0644, &attr);
    if(cmq < 0){
        perror("mqueue");
        exit(EXIT_FAILURE);
    }
    printf("id kolejki: %d\n", msqid);


    
    char buffer[MAX_SIZE];

    char clientid[4];

    char *response = malloc(MAX_BUF * sizeof(char));
    response[0] = MSG_KEY + '0';

    char id[4];
    sprintf(response+1, "%s", cmq);
    if(mq_send(mq, msg, MAX_BUF, 0) < 0){
        perror("mqsend");
    }
    free(response);

    while(1){
        //stdin
        fd_set rfds;
        struct timeval tv;
        int retval;
        char buf[1024];

        FD_ZERO(&rfds);
        FD_SET(0, &rfds);

        tv.tv_sec = 5;
        tv.tv_usec = 0;

        retval = select(1, &rfds, NULL, NULL, &tv);

        if(retval == -1){
           perror("select");
        }else if(retval){
            int count = read(0, buf, 1024);
            buf[count]='\0';
            printf("input: %s\n", buf);

            switch(buf[0] - '0'){
                case 1:{
                    //send MSG_END
                
                    char *response = malloc(MAX_BUF * sizeof(char));
                    response[0] = MSG_END + '0'; //remember, char array!
                    if(mq_send(mq, response, MAX_BUF, 0) < 0){
                        perror("mqsend");
                    }
                    if(mq_close(mq) < 0){
                        perror("mqclose");
                    }
                    free(response);
                    printf("exiting\n");
                    //zamknij kolejke
                    if(mq_close(cmq) < 0){
                        perror("mqclose");
                    }
                    if(mq_unlink(queue_client) < 0){
                        perror("mqunlink");
                    }
                    exit(EXIT_SUCCESS);
                }
                break;

                //no case 2, sending key is automatic

                case 3:{ 
                    //mirror
                    char *response = malloc(MAX_BUF * sizeof(char));
                    sprintf(response+1, "%s", buf);
                    

                    char tmp[1028];
                    sprintf(tmp+1, "%s", clientid);
                    strcat(tmp, buf+2);
                    char *newline = strchr( tmp, '\n' );  //remove newline
                    if(newline){
                        *newline = 0;
                    }
                    strcpy(response, tmp);
                    response[0]=MSG_MIRROR + '0';
                    printf("sending: %s\n", reply->mtext);
                    if(mq_send(mq, response, MAX_BUF, 0) < 0){
                        perror("mqsend");
                    }
                    free(response);
                }
                break;

                case 5:{
                    //time
                    char *response = malloc(MAX_BUF * sizeof(char));
                    response[0] = MSG_TIME + '0';
                    sprintf(response+1, "%s", clientid); 

                    printf("sending time\n");
                    if(mq_send(mq, response, MAX_BUF, 0) < 0){
                        perror("mqsend");
                    }
                    free(response);
                }
                break;
            }
            

        }else{
            printf("No user input within five seconds.\n");
        }        

        //messaging

        //msg = malloc(sizeof(struct message));
        int btread = msgrcv(msqid, msg, sizeof(struct message), 0, IPC_NOWAIT);
        ssize_t bytes_read;
        bytes_read = mq_receive(mq, buffer, MAX_SIZE, NULL);
        //CHECK(bytes_read >= 0);
        buffer[bytes_read] = '\0';
        
        if(bytes_read > 0) {
            printf("Received: %s\n", buffer);
            if(buffer[0] == MSG_END + '0'){
                if(mq_close(mq) < 0){
                perror("mqclose");
            }
            if(mq_close(cmq) < 0){
                perror("mqclose");
            }

            //change to clients queue name
            if(mq_unlink(queue_client) < 0){
                perror("mqunlink");
            }
            exit(EXIT_SUCCESS);
            }
        }
        
    }
}
