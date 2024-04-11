main: ./main.c ./malloc.c
	$(CC) -g -o $@ $^
