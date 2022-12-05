CC = gcc
FLAGS = -Wall -Wextra -Wpedantic

main: main.c
	$(CC) $(FLAGS) main.c
