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
#include <sys/msg.h>
#include "cstmsg.h"


key_t key;

void intsig(int signum){
    printf("\nreceived SIGINT\n");
    if(msgctl(msgget(key, 0660), IPC_RMID, NULL) < 0){
        perror("msgctl");
    }
    exit(EXIT_SUCCESS);
}

struct message {
    long mtype;
    char mtext[2048];
};

long dec(char *num){
    return strtol(num, NULL, 10);
}


int main(int argc, char* argv[]){
    signal(SIGINT, intsig);

    //stworz kolejke, wyslij jej klucz serwerowi, po otrzymaniu id przejdz do petli
    srand(time(NULL));
    int rnd = rand();
    key = ftok("/tmp", rnd);

    int msqid = msgget(key, IPC_CREAT | IPC_EXCL | 0600);
    if (msqid < 0) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }
    printf("id kolejki: %d\n", msqid);

    //petla do przetwarzania polecen \/ \/ 

    /*
    Usługa lustra (MIRROR):
    Klient wysyła ciąg znaków. Serwer odsyła ten sam ciąg z powrotem w odwrotnej kolejności. Klient po odebraniu wysyła go na standardowe wyjście.
    Usługa kalkulatora (CALC):
    Klient wysyła proste działanie arytmetyczne (+ - * /) Serwer oblicza i odsyła wynik. Klient po odebraniu wysyła go na standardowe wyjście. Działania można przesyłać w postaci np CALC 2+3, CALC 3*4 albo ADD 1 2, MUL 3 4, SUB 5 4, DIV 4 3 - wariant do wyboru.
    Usługa czasu (TIME):
    Po odebraniu takiego zlecenia serwer wysyła do klienta datę i godzinę w postaci łańcucha znaków. Klient po odebraniu informacji wysyła ją na standardowe wyjście.
    Nakaz zakończenia (END):
    Po odebraniu takiego zlecenia serwer zakończy działanie, jak tylko opróżni się kolejka zleceń (zostaną wykonane wszystkie pozostałe zlecenia).
    Klient nie czeka na odpowiedź.
    */
    struct message *msg = NULL;
    char clientid[4];

    msg = malloc(sizeof(struct message));
    msg->mtype = 2;
            

    int srvid = msgget(SRV_KEY, 0660);
    if(srvid<0){
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    char keybuf[9];
    sprintf (keybuf, "%d", msqid);
    sprintf(msg->mtext, "%d", msqid);

    printf("sending key\n");
    if(msgsnd(srvid, msg, sizeof(msg->mtext), 0) < 0){
        perror("msgsnd");
    }
    free(msg);
    
    
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
            //buf: X string..., X - operation to do (1..5), 0 - do in loop (?)

            switch(buf[0] - '0'){
                case 0:{
                    //loop?
                }
                break;

                case 1:{
                    //send MSG_END
                    struct message *reply = malloc(sizeof(struct message));
                    reply->mtype = MSG_END;

                    printf("sending END\n");
                    if(msgsnd(srvid, reply, sizeof(reply->mtext), 0) < 0){
                        perror("msgsnd");
                    }
                    free(reply);
                    if(msgctl(msgget(key, 0660), IPC_RMID, NULL) < 0){
                        perror("msgctl");
                    }
                    exit(EXIT_SUCCESS);
                }
                break;

                //no case 2, sending key is automatic

                case 3:{ 
                    //mirror
                    struct message *reply = malloc(sizeof(struct message));
                    sprintf(reply->mtext, "%s", buf);
                    reply->mtype=MSG_MIRROR;

                    //remember to parse buffer first!
                    //also, to add clientid
                    //strcpy(reply->mtext, clientid);
                    //strcpy(reply->mtext + 3, buf+2);
                    char tmp[1028];
                    sprintf(tmp, "%s", clientid);
                    strcat(tmp, buf+2);
                    char *newline = strchr( tmp, '\n' );  //remove newline
                    if(newline){
                        *newline = 0;
                    }
                    strcpy(reply->mtext, tmp);
                    printf("sending: %s\n", reply->mtext);
                    if(msgsnd(srvid, reply, sizeof(reply->mtext), 0) < 0){
                        perror("msgsnd");
                    }
                    free(reply);
                }
                break;

                case 4:{
                    //calc

                }
                break;

                case 5:{
                    //time
                    struct message *reply = malloc(sizeof(struct message));
                    reply->mtype=MSG_TIME;
                    sprintf(reply->mtext, "%s", clientid); 

                    printf("sending time\n");
                    if(msgsnd(srvid, reply, sizeof(reply->mtext), 0) < 0){
                        perror("msgsnd");
                    }
                    free(reply);
                }
                break;
            }
            

        }else{
            printf("No user input within five seconds.\n");
        }        

        //messaging

        msg = malloc(sizeof(struct message));
        int btread = msgrcv(msqid, msg, sizeof(struct message), 0, IPC_NOWAIT);
        if(btread < 0){
            perror("msgrcv");
            free(msg);
            msg = NULL;
        }
        if(msg != NULL){
            printf("odebrano wiadomosc: %ld\n", msg->mtype);
            switch(msg->mtype){
                case MSG_END:
                    if(msgctl(msqid, IPC_RMID, NULL) < 0){
                        perror("msgctl");
                    }
                    exit(EXIT_SUCCESS);
                break;

                case MSG_KEY:{
                    sprintf(clientid, "%s", msg->mtext);
                    printf("id: %s\n", clientid);
                }
                break;

                case MSG_MIRROR:{
                    printf("response: %s\n", msg->mtext);
                }
                break;

                case MSG_CALC:
                    printf("response: %s\n", msg->mtext);
                break;

                case MSG_TIME:
                    printf("response: %s\n", msg->mtext);
                break;
            }
            free(msg);
        }
    }



    //zamknij kolejke
    if(msgctl(msgget(key, 0660), IPC_RMID, NULL) < 0){
        perror("msgctl");
    }
    exit(EXIT_SUCCESS);
}