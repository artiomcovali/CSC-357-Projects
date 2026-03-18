all: gossip

gossip: gossip.c
	gcc -Wall -Wextra -std=gnu11 -pedantic -o gossip gossip.c

debug: gossip.c
	gcc -Wall -Wextra -std=gnu11 -pedantic -g -o gossip gossip.c

clean:
	rm -f gossip
