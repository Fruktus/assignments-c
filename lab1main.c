#include "lab1.h"
#include <stdio.h>
#include <string.h>
#include <time.h>


int main(){
    /*
    char **arr=create_arr(10);
    if(arr==NULL){
        printf("%d: couldn't allocate array\n",__LINE__ -2);
    }else{
        printf("%d: allocated array\n",__LINE__ -4);
    }

    remove_arr(&arr, 10);
    if(arr!=NULL){
        printf("%d: couldn't free array\n",__LINE__ -2);
    }else{
        printf("%d: freed array\n",__LINE__ -4);
    }

    arr=create_arr(10);
    if(arr==NULL){
        printf("%d: couldn't allocate array\n",__LINE__ -2);
    }else{
        printf("%d: allocated array\n",__LINE__ -4);
    }

    fill_arr(arr, 5); 
    if(*arr==NULL){
        printf("%d: couldn't allocate block\n",__LINE__ -2);
    }else{
        printf("%d: allocated block\n",__LINE__ -4);
    }
    //arr[2][3]='c';

    if(arr[5]==NULL){ 
        printf("%d: block free\n",__LINE__ -1);
    }else{
        printf("%d: block allocated\n",__LINE__ -3);
    }

    add_block(arr, 5);
    char *testptr=arr[5];
    if(arr[5]==NULL){
        printf("%d: couldn't allocate block\n",__LINE__ -2);
    }else{
        printf("%d: allocated block\n",__LINE__ -4);
    }

    add_block(arr, 5);
    if(testptr==arr[5]){
        printf("%d: didn't re-add block\n",__LINE__ -2);
    }else{
        printf("%d: re-added block\n",__LINE__ -4);
    }

    remove_block(arr, 5);
    if(arr[5]==NULL){
        printf("%d: removed block\n",__LINE__ - 2);
    }else{
        printf("%d: couldn't remove block\n",__LINE__ - 4);
    }

    remove_arr(&arr, 10);
    */
    clock_t start, mid, end;
    double cpu_time_used;
    char **arr=NULL;
    int exit=0;
    int size=0;
    while(exit==0){
        int param;
        char command[10];
        printf(">");
        scanf("%s %d",command,&param);
        if(!strncmp(command,"create",6)){
            if(arr==NULL){
                start = clock();
                arr=create_arr(param);
                size=param;
                end = clock();
                printf("created\n");
                cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
                printf("time: %f\n", cpu_time_used);
            }else{
                printf("Table already exists, recreating\n");
                start = clock();
                remove_arr(&arr, size);
                mid = clock();
                arr=create_arr(param);
                end = clock();
                size=param;
                printf("created\n");
                cpu_time_used = ((double) (mid - start)) / CLOCKS_PER_SEC;
                printf("time deleting: %f\n", cpu_time_used);
                cpu_time_used = ((double) (end - mid)) / CLOCKS_PER_SEC;
                printf("time creating: %f\n", cpu_time_used);
                cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
                printf("time total: %f\n", cpu_time_used);
            }
        }else if(!strncmp(command,"search",6)){
            start = clock();
            char *result=find_block(arr, param, size);
            end = clock();
            int i=0;
            int res=0;
            while(result[i]!=0){
                res += result[i];
            }
            printf("najblizsza suma: %d\n", res);
            cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
            printf("time: %f\n", cpu_time_used);
        }else if(!strncmp(command,"add",3)){
            start = clock();
            fill_arr(arr, param);
            end = clock();
            printf("dodano\n");
            cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
            printf("time: %f\n", cpu_time_used);
        }else if(!strncmp(command, "exit",4)){
            printf("quitting...\n");
            exit=1;
        }else if(!strncmp(command,"remove",6)){
            start = clock();
            for(int i=0; i<param; i++){
                remove_block(arr, i);
            }
            end = clock();
            cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
            printf("time: %f\n", cpu_time_used);
            printf("usunieto\n");
        }else{
            printf("bledne polecenie");
        }
    } 
}
/*
allowed commands
create_table rozmiar - stworzenie tablicy o rozmiarze "rozmiar" i blokach o rozmiarach "rozmiar bloku" (nie zmienialny) 
* search_element wartość - wyszukanie elementu o wartości ASCII zbliżonej do pozycji "wartosć" 
* remove number - usuń "number" bloków (liczac od poczatku)
* add  number - dodaj "number" bloków (liczac od poczatku, nie nadpisujac nowych)

*/




//gcc lab1main.c lab1.o -o lab1main
//gcc  test.c mylib.o
//gcc  test.c -L/home/newhall/lib -lmylib