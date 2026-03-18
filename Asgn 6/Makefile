
all: batch

batch: batch.c
	gcc -Wall -Wextra -std=gnu11 -pedantic -O2 -o batch batch.c

debug: batch.c
	gcc -Wall -Wextra -std=gnu11 -pedantic -g -o batch batch.c

clean:
	rm -f batch
