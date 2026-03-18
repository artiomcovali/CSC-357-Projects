/* Defines functions for converting characters between DOS and UNIX.
 * CSC 357, Assignment 1
 * Reference sol'n, Spring '25. */

#include "chars.h"

/* istext: Checks whether or not a character is plain text.
 * NOTE: Do not alter this function. It returns 1 if "c" has an ASCII code in
 *       the ranges [0x08, 0x0D] or [0x20, 0x7E], and 0 otherwise. */
int istext(char c) {
    return (c >= 0x08 && c <= 0x0D) || (c >= 0x20 && c <= 0x7E);
}

/* dtou: Converts a character from DOS to UNIX.
 * TODO: Implement this function. It should return the number of characters
 *       placed into "str": generally nothing if "next" is a carriage return,
 *       a newline if "next" is a newline following a carriage return, and
 *       "next" otherwise. Use "dflt" instead if "next" is not plain text, and
 *       do nothing if neither is plain text. See also the given unit tests. */
int dtou(char next, char str[], char dflt) {
    static int pending_cr = 0; 
    int out = 0;
    char c;

    str[0] = '\0';

    if (istext(next)) {
        c = next;
    } else if (istext(dflt)) {
        c = dflt;
    } else {
        return 0;
    }

    if (pending_cr) {
        if (c == '\n') {
            str[out++] = '\n';
            str[out] = '\0';
            pending_cr = 0;
            return out;
        }

        str[out++] = '\r';
        pending_cr = 0;
    }

    if (c == '\r') {
        pending_cr = 1;
        str[out] = '\0';
        return out;
    }

    str[out++] = c;
    str[out] = '\0';
    return out;
}

/* utod: Converts a character from UNIX to DOS.
 * TODO: Implement this function. It should return the number of characters
 *       placed into "str": generally a carriage return followed by a newline
 *       if "next" is a newline, and "next" otherwise. Use "dflt" instead if
 *       "next" is not plain text, and do nothing if neither is plain text. See
 *       also the given unit tests. */
int utod(char next, char str[], char dflt) {
    static int pending_cr = 0; 
    int out = 0;
    char c;

    str[0] = '\0';

    if (istext(next)) {
        c = next;
    } else if (istext(dflt)) {
        c = dflt;
    } else {
        return 0;
    }

    if (pending_cr) {
        if (c == '\n') {
            str[out++] = '\r';
            str[out++] = '\n';
            str[out] = '\0';
            pending_cr = 0;
            return out;
        }

        str[out++] = '\r';
        pending_cr = 0;
    }

    if (c == '\r') {
        pending_cr = 1;
        str[out] = '\0';
        return out;
    }

    if (c == '\n') {
        str[out++] = '\r';
        str[out++] = '\n';
        str[out] = '\0';
        return out;
    }

    str[out++] = c;
    str[out] = '\0';
    return out;
}
