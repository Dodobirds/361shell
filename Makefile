# choose your compiler
CC=gcc
#CC=gcc -Wall -Wextra -pedantic

mshell: mshell.o builtin.o lib.a 
	$(CC) $^ -o $@

mshell.o: shell.c 
	$(CC) -c $< -o $@

builtin.o: builtin.c  
	$(CC) -c $< -o $@

lib.a: utils.o
	ar rcs $@ $^

utils.o: utils.c utils.h
	$(CC) -c $< -o $@

clean:
	rm -rf *.o mshell *.a
