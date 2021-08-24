CC=gcc

shell: shell.c
	$(CC) shell.c -o shell -Wall -Werror -g

valgrind: shell
	 valgrind --leak-check=full --track-origins=yes -s shell
clean:
	rm -f shell
