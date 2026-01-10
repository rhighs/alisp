PROG:=alisp
CC:=gcc

all:
	$(CC) mpc.c main.c -o $(PROG) -g -Wall -Wextra -std=c99 -ledit
