CC = gcc
FLAGS = -Wall -Werror

main: main.c
	$(CC) $(FLAGS) main.c