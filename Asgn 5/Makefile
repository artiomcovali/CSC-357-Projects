all: tree

tree: tree.c
	gcc -std=c11 -Wall -Wextra -Werror -pedantic tree.c -o tree

debug: tree.c
	gcc -std=c11 -Wall -Wextra -Werror -pedantic -g3 -O0 -fsanitize=address,undefined tree.c -o tree

clean:
	rm -f tree
