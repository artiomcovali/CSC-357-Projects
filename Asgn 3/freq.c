#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "dict.h"

typedef struct {
    char *word;
    int count;
} WordCount;

static int cmp_wordcount(const void *a, const void *b) {
    const WordCount *wa = (const WordCount *)a;
    const WordCount *wb = (const WordCount *)b;

    if (wa->count != wb->count) {
        return wb->count - wa->count;
    }
    return strcmp(wa->word, wb->word);
}

int main(int argc, char **argv) {
    FILE *fp;
    Dict *dct;
    char *buf;
    int cap, len;
    int c;

    if (argc < 2) {
        printf("%s: Too few arguments\n", argv[0]);
        return 1;
    }
    if (argc > 2) {
        printf("%s: Too many arguments\n", argv[0]);
        return 1;
    }

    fp = fopen(argv[1], "r");
    if (fp == NULL) {
        printf("%s: %s\n", argv[0], strerror(errno));
        return 1;
    }

    dct = dctcreate();
    if (dct == NULL) {
        fclose(fp);
        return 1;
    }

    cap = 32;
    len = 0;
    buf = (char *)malloc((size_t)cap);
    if (buf == NULL) {
        fclose(fp);
        dctdestroy(dct);
        return 1;
    }

    while ((c = fgetc(fp)) != EOF) {
        if (isspace((unsigned char)c)) {
            if (len > 0) {
                int i, w;
                int *cnt;

                w = 0;
                for (i = 0; i < len; i++) {
                    unsigned char ch = (unsigned char)buf[i];
                    if (isalpha(ch)) {
                        buf[w++] = (char)tolower(ch);
                    }
                }
                buf[w] = '\0';

                if (w > 0) {
                    cnt = (int *)dctget(dct, buf);
                    if (cnt == NULL) {
                        cnt = (int *)malloc(sizeof(int));
                        if (cnt != NULL) {
                            *cnt = 1;
                            dctinsert(dct, buf, cnt);
                        }
                    } else {
                        (*cnt)++;
                    }
                }

                len = 0;
            }
        } else {
            if (len + 1 >= cap) {
                int newcap = cap * 2;
                char *nbuf = (char *)realloc(buf, (size_t)newcap);
                if (nbuf == NULL) {
                    free(buf);
                    fclose(fp);

                    {
                        char **keys = dctkeys(dct);
                        int i;
                        if (keys != NULL) {
                            for (i = 0; i < dct->size; i++) {
                                free(dctget(dct, keys[i]));
                            }
                            free(keys);
                        }
                    }

                    dctdestroy(dct);
                    return 1;
                }
                buf = nbuf;
                cap = newcap;
            }
            buf[len++] = (char)c;
        }
    }

    if (len > 0) {
        int i, w;
        int *cnt;

        w = 0;
        for (i = 0; i < len; i++) {
            unsigned char ch = (unsigned char)buf[i];
            if (isalpha(ch)) {
                buf[w++] = (char)tolower(ch);
            }
        }
        buf[w] = '\0';

        if (w > 0) {
            cnt = (int *)dctget(dct, buf);
            if (cnt == NULL) {
                cnt = (int *)malloc(sizeof(int));
                if (cnt != NULL) {
                    *cnt = 1;
                    dctinsert(dct, buf, cnt);
                }
            } else {
                (*cnt)++;
            }
        }
    }

    free(buf);
    fclose(fp);

    if (dct->size > 0) {
        char **keys = dctkeys(dct);
        WordCount *arr;
        int i, n, limit;

        n = dct->size;
        arr = (WordCount *)malloc((size_t)n * sizeof(WordCount));
        if (arr != NULL && keys != NULL) {
            for (i = 0; i < n; i++) {
                int *cnt = (int *)dctget(dct, keys[i]);
                arr[i].word = keys[i];
                arr[i].count = (cnt == NULL) ? 0 : *cnt;
            }

            qsort(arr, (size_t)n, sizeof(WordCount), cmp_wordcount);

            limit = (n < 10) ? n : 10;
            for (i = 0; i < limit; i++) {
                printf("%s (%d)\n", arr[i].word, arr[i].count);
            }
        }

        free(arr);
        free(keys);
    }

    {
        char **keys = dctkeys(dct);
        int i;
        if (keys != NULL) {
            for (i = 0; i < dct->size; i++) {
                free(dctget(dct, keys[i]));
            }
            free(keys);
        }
    }

    dctdestroy(dct);
    return 0;
}
