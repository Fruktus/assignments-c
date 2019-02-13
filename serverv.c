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
#include <sys/ipc.h>
#include "cstmsg.h"

#define MAX_CLIENTS 20

/*
 *
 *  11 maja praktyczne kolokwium
 *  3 zadania, znalezc blad, napisac jakas funkcje, bedzie mozna uzywac manuala
 *
 */

key_t clients[MAX_CLIENTS];
int client_count = 0;

struct message {
    long mtype;
    char mtext[2048];
};

void intsig(int signum){
    printf("\nreceived SIGINT\n");
    struct message *response = malloc(sizeof(response));
    response->mtype=MSG_END;
    for(int i=0; i<client_count; i++){
        msgsnd(clients[i], response, sizeof(response->mtext), 0);
    }
    //zamknij kolejke
    if(msgctl(msgget(SRV_KEY, 0660), IPC_RMID, NULL) < 0){
        perror("msgctl");
    }
    exit(EXIT_SUCCESS);
}

long dec(char *num){
    return strtol(num, NULL, 10);
}

void inplace_reverse(char * str)
{
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

    // IPC_EXCL użyta z IPC_CREAT powoduje błąd EEXIST tworzy nowa ale nie jesli juz jest
    //przy ipc private zawsze tworzy nowa
    int msqid = msgget(SRV_KEY, IPC_CREAT | IPC_EXCL | 0660);
    if (msqid < 0) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    
    //key_t key = ftok("/tmp", 'S');

    

    //po otrzymaniu od klienta klucza, otworz kolejke i nadaj klientowi id i odeslij kolejka klienta
    //klucz przyjdzie wiadomoscia, oznaczyc typ komunikatu
    printf("server start\n");
    while(1){
        //read msg
        struct message *msg = malloc(sizeof(struct message));
        int btread = msgrcv(msqid, msg, sizeof(struct message), 0, 0);
        if(btread < 0){
            perror("msgrcv");
        }
        if(msg != NULL){
            printf("received message: %ld\n", msg->mtype); //dbg
            switch(msg->mtype){
                case MSG_END:{
                        struct message *response = malloc(sizeof(response));
                        response->mtype=MSG_END;
                        for(int i=0; i<client_count; i++){
                            msgsnd(clients[i], response, sizeof(response->mtext), 0);
                        }
                        free(response);
                        free(msg);
                        printf("exiting\n");
                        //zamknij kolejke
                        if(msgctl(msqid, IPC_RMID, NULL) < 0){
                            perror("msgctl");
                        }
                        exit(EXIT_SUCCESS);
                    }
                break;

                case MSG_KEY:
                    if(client_count < MAX_CLIENTS){
                        //save client queue id
                        printf("received key: %s\n", msg->mtext);
                        clients[client_count]=dec(msg->mtext);

                        struct message *response = malloc(sizeof(struct message));
                        response->mtype=msg->mtype;

                        char id[4];
                        //generate string id for client
                        id[0] = client_count/100 + '0';
                        id[1] = client_count/10%10 + '0';
                        id[2] = client_count%10 +'0';
                        id[3] = '\0';

                        printf("reply id: %s\n", id);
                        sprintf(response->mtext, "%s", id);
                        if(msgsnd(clients[client_count], response, sizeof(response->mtext), 0) < 0){
                            perror("msgsnd");
                        }
                        free(response);
                        client_count++;
                    }else{
                        printf("exceeded maximum clients\n");
                    }
                break;

                case MSG_MIRROR:
                    {
                        struct message *response = malloc(sizeof(struct message));
                        response->mtype=msg->mtype;
                        //for the sake of simplicity I'll use the first three digits to represent clientid
                        char id[4];
                        id[3]='\0';
                        strncpy(id, msg->mtext, 3);
                        int clientid = dec(id);
                        printf("cid: %ld\n", dec(id));
                        //need to rework reverse
                        sprintf(response->mtext, "%s", msg->mtext+3);
                        inplace_reverse(response->mtext);
                        printf("sending mirror\n");
                        if(msgsnd(clients[clientid], response, sizeof(response->mtext), 0) < 0){
                            perror("msgsnd");
                        }
                        free(response);
                    }
                break;

                case MSG_CALC:
                    
                break;

                case MSG_TIME:{
                    struct message *response = malloc(sizeof(struct message));    
                    time_t current_time;
                    char* c_time_string;

                    current_time = time(NULL);
                    c_time_string = ctime(&current_time);
                    char *newline = strchr( c_time_string, '\n' );  //remove newline
                    if(newline){
                        *newline = 0;
                    }

                    response->mtype=msg->mtype;
                    printf("time: %s\n", c_time_string);
                    sprintf(response->mtext, "%s", c_time_string);
                    char id[4];
                    id[3] = '\0';
                    strncpy(id, msg->mtext, 3);
                    int clientid = dec(id);
                    if(msgsnd(clients[clientid], response, sizeof(response->mtext), 0) < 0){
                        perror("msgsnd");
                    }
                    free(response);
                }
                break;
            }
        }
        free(msg);
        sleep(1);
    }


    //dodawaj zlecenia do kolejki czy tam sciaaj, whatevs, maja zawierac zlecenie i info komu odeslac



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

    mozna zrobic ze serwer wtedy wszystkim podpietym klientom odesle END zeby sie pozamykaly
    */
   

    //usun swoja kolejke
    exit(EXIT_SUCCESS);
}