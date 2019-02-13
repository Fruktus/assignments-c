#include <stdlib.h>
#include <stdio.h>
#include "limits.h"
#include "lab1.h"
#define MAX_SIZE 100

//https://www.cs.swarthmore.edu/~newhall/unixhelp/howto_C_libraries.html
//gcc -o lab1.o -c lab1.c

char **create_arr(int n){
	return (char**)calloc(n, sizeof(char*));
}

void remove_arr(char*** n, int size){
	int i=0;
	while(i < size){
		if((*n)[i]!=NULL){
			free((*n)[i]);
			(*n)[i]=NULL;
		}
		i++;
	}
	free(*n);
	*n=NULL;
}

void add_block(char **a, int m){
	if(a[m]==NULL){
		a[m]=(char*)calloc(MAX_SIZE,sizeof(char));
	}
}

void remove_block(char **a, int n){
	if(a[n]!=NULL){
		free(a[n]);
		a[n]=NULL;
	}
}

void fill_arr(char **a, int m){ //wypelnia a o dl m blokami o dlugosci 100*char
	for(int i=0; i<m; i++)
	{
		a[i]=(char*)calloc(MAX_SIZE,sizeof(char));
	}
}

char *find_block(char **a, int n, int m){
	char *res=NULL;
	int current_dist=INT_MAX;
	int i=0;
	while(i<m){
		if(a[i]!=NULL){
			int sum=0;
			int j=0;
			while(j<MAX_SIZE && a[j]!=NULL){
				sum += (int)a[j]; 
				if(abs(sum) < current_dist){
					res = a[i];
				}
				j++;
			}
		}
		i++;
	}
	return res;
}
//value = *(pointer + n); 
