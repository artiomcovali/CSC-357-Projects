/* Converts characters from UNIX to DOS.
 * CSC 357, Assignment 1
 * Given code, Spring '25 */

#include <stdio.h>
#include "chars.h"

int main(void) {
    char next, str[3];

    while (scanf("%c", &next) != EOF) {
        int n = utod(next, str, '\0');
        int i;
        for (i = 0; i < n; i++) {
            putchar((unsigned char)str[i]);
        }
    }

    return 0;
}
