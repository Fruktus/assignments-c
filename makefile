CC=gcc
CFLAGS=-O1
DEPS = lab1.h
OBJ = lab1.o lab1main.o
PATH = /Users/Antek/Desktop/Notatki/sysopy/lab1

all: Dlab1main Slab1main Llab1main

lab1.so:
	gcc -shared -o $@ lab1.o  -lm $(CFLAGS)

lab1.a:
	ar rcs $@ lab1.o

%.o: %.c $(DEPS)
		$(CC) -c -o $@ $< $(CFLAGS)

Dlab1main: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)

Slab1main: $(OBJ)
	cc -o $@ lab1main.c lab1.a $(CFLAGS)

Llab1main: $(OBJ)
	gcc  lab1main.c -o $@ -L$(PATH) -llab1 $(CFLAGS)