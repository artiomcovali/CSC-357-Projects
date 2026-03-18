/* Defines functions for implmenting a dictionary.
 * CSC 357, Assignment 3
 * Given code, Winter '24 */

#include <stdlib.h>
#include <string.h>
#include "dict.h"

/* dcthash: Hashes a string key.
 * NOTE: This is certainly not the best known string hashing function, but it
 *       is reasonably performant and easy to predict when testing. */
static unsigned long int dcthash(char *key) {
    unsigned long int code = 0, i;

    for (i = 0; i < 8 && key[i] != '\0'; i++) {
        code = key[i] + 31 * code;
    }

    return code;
}

/* dctcreate: Creates a new empty dictionary.
 * TODO: Implement this function. It should return a pointer to a newly
 *       dynamically allocated dictionary with an empty backing array. */
Dict *dctcreate() {
    Dict *dct = (Dict *)malloc(sizeof(Dict));
    if (dct == NULL) return NULL;

    dct->cap = 5;
    dct->size = 0;
    dct->arr = (Node **)calloc((size_t)dct->cap, sizeof(Node *));
    if (dct->arr == NULL) {
        free(dct);
        return NULL;
    }

    return dct;
}

/* dctdestroy: Destroys an existing dictionary.
 * TODO: Implement this function. It should deallocate a dictionary, its
 *       backing array, and all of its nodes. */
void dctdestroy(Dict *dct) {
    int i;
    if (dct == NULL) return;

    if (dct->arr != NULL) {
        for (i = 0; i < dct->cap; i++) {
            Node *cur = dct->arr[i];
            while (cur != NULL) {
                Node *next = cur->next;
                free(cur->key);
                free(cur);
                cur = next;
            }
        }
        free(dct->arr);
    }

    free(dct);
}

/* dctget: Gets the value to which a key is mapped.
 * TODO: Implement this function. It should return the value to which "key" is
 *       mapped, or NULL if it does not exist. */
void *dctget(Dict *dct, char *key) {
    Node *cur;
    unsigned long int h;
    int idx;

    if (dct == NULL || key == NULL || dct->arr == NULL || dct->cap <= 0) return NULL;

    h = dcthash(key);
    idx = (int)(h % (unsigned long int)dct->cap);

    cur = dct->arr[idx];
    while (cur != NULL) {
        if (strcmp(cur->key, key) == 0) return cur->val;
        cur = cur->next;
    }
    return NULL;
}

/* dctinsert: Inserts a key, overwriting any existing value.
 * TODO: Implement this function. It should insert "key" mapped to "val".
 * NOTE: Depending on how you use this dictionary, it may be useful to insert a
 *       dynamically allocated copy of "key", rather than "key" itself. Either
 *       implementation is acceptable, as long as there are no memory leaks. */
void dctinsert(Dict *dct, char *key, void *val) {
    unsigned long int h;
    int idx;
    Node *cur;

    if (dct == NULL || key == NULL) return;

    h = dcthash(key);
    idx = (int)(h % (unsigned long int)dct->cap);

    cur = dct->arr[idx];
    while (cur != NULL) {
        if (strcmp(cur->key, key) == 0) {
            cur->val = val;
            return;
        }
        cur = cur->next;
    }

    if (dct->size + 1 > dct->cap) {
        int newcap = 2 * dct->cap + 1;
        Node **newarr = (Node **)calloc((size_t)newcap, sizeof(Node *));
        int i;

        if (newarr != NULL) {
            for (i = 0; i < dct->cap; i++) {
                Node *ncur = dct->arr[i];
                while (ncur != NULL) {
                    Node *next = ncur->next;
                    unsigned long int nh = dcthash(ncur->key);
                    int nidx = (int)(nh % (unsigned long int)newcap);

                    ncur->next = newarr[nidx];
                    newarr[nidx] = ncur;

                    ncur = next;
                }
            }
            free(dct->arr);
            dct->arr = newarr;
            dct->cap = newcap;

            h = dcthash(key);
            idx = (int)(h % (unsigned long int)dct->cap);
        }
    }

    cur = (Node *)malloc(sizeof(Node));
    if (cur == NULL) return;

    {
        size_t n = strlen(key) + 1;
        cur->key = (char *)malloc(n);
        if (cur->key == NULL) {
            free(cur);
            return;
        }
        memcpy(cur->key, key, n);
    }

    cur->val = val;

    cur->next = dct->arr[idx];
    dct->arr[idx] = cur;
    dct->size++;
}

/* dctremove: Removes a key and the value to which it is mapped.
 * TODO: Implement this function. It should remove "key" and return the value
 *       to which it was mapped, or NULL if it did not exist. */
void *dctremove(Dict *dct, char *key) {
    unsigned long int h;
    int idx;
    Node *cur, *prev;

    if (dct == NULL || key == NULL || dct->arr == NULL || dct->cap <= 0) return NULL;

    h = dcthash(key);
    idx = (int)(h % (unsigned long int)dct->cap);

    prev = NULL;
    cur = dct->arr[idx];
    while (cur != NULL) {
        if (strcmp(cur->key, key) == 0) {
            void *out = cur->val;
            if (prev == NULL) dct->arr[idx] = cur->next;
            else prev->next = cur->next;

            free(cur->key);
            free(cur);
            dct->size--;
            return out;
        }
        prev = cur;
        cur = cur->next;
    }
    return NULL;
}

/* dctkeys: Enumerates all of the keys in a dictionary.
 * TODO: Implement this function. It should return a dynamically allocated array
 *       of pointers to the keys, in no particular order, or NULL if empty. */
char **dctkeys(Dict *dct) {
    char **keys;
    int i, k;

    if (dct == NULL || dct->size == 0) return NULL;

    keys = (char **)malloc((size_t)dct->size * sizeof(char *));
    if (keys == NULL) return NULL;

    k = 0;
    for (i = 0; i < dct->cap; i++) {
        Node *cur = dct->arr[i];
        while (cur != NULL) {
            keys[k++] = cur->key;
            cur = cur->next;
        }
    }

    return keys;
}

