CC = gcc
FLAGS = -Wall -Wextra -Wpedantic

main: *.c
	$(CC) $(FLAGS) *.c
