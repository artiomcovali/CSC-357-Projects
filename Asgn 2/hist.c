#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ROWS 1024
#define MAX_LINE 2048

static void mismatched_error(void);
static void quotes_error(void);
static int count_cells(const char *line);
static long parse_cell(const char *start, const char *end);
static long floor_div(long a, long d);
static long ceil_div(long a, long d);
static long round_down_10(long x);
static long round_up_10(long x);
static int round_up_5(int x);

int main(void) {
    char line[MAX_LINE];
    long data[MAX_ROWS];
    int rows;
    int cols;
    int i;
    long minv, maxv;
    long start, end;
    int bins;
    int *counts;
    int maxc, height;

    rows = 0;

    if (!fgets(line, sizeof(line), stdin)) return 0;
    if (strchr(line, '\"') != NULL || strchr(line, '\'') != NULL) quotes_error();

    cols = count_cells(line);

    while (fgets(line, sizeof(line), stdin)) {
        const char *p;
        const char *cell;
        long sum;

        if (strchr(line, '\"') != NULL || strchr(line, '\'') != NULL) quotes_error();
        if (count_cells(line) != cols) mismatched_error();

        sum = 0;
        p = line;
        cell = p;

        while (1) {
            if (*p == ',' || *p == '\n' || *p == '\0') {
                sum += parse_cell(cell, p);
                if (*p == '\0' || *p == '\n') break;
                p++;
                cell = p;
            } else {
                p++;
            }
        }

        if (rows < MAX_ROWS) {
            data[rows++] = sum;
        } else {
            break;
        }
    }

    if (rows <= 0) return 0;

    minv = data[0];
    maxv = data[0];
    for (i = 1; i < rows; i++) {
        if (data[i] < minv) minv = data[i];
        if (data[i] > maxv) maxv = data[i];
    }

    start = round_down_10(minv);
    end   = round_up_10(maxv);

    bins = (int)((end - start) / 2L) + 1;
    if (bins < 1) bins = 1;

    counts = (int *)calloc((size_t)bins, sizeof(int));
    if (counts == NULL) return 0;

    for (i = 0; i < rows; i++) {
        long x, u;
        int idx;

        x = data[i];
        u = (x % 2L == 0L) ? x : (x + 1L);
        idx = (int)((u - start) / 2L);

        if (idx < 0) idx = 0;
        if (idx >= bins) idx = bins - 1;

        counts[idx]++;
    }

    maxc = 0;
    for (i = 0; i < bins; i++) {
        if (counts[i] > maxc) maxc = counts[i];
    }
    height = round_up_5(maxc);

    {
        int y;
        for (y = height + 1; y > 0; y--) {
            int col;

            if (y % 5 == 0) {
                printf("%3d T", y);
            } else {
                printf("    |");
            }

            putchar(' ');

            for (col = 0; col < bins; col++) {
                putchar(counts[col] >= y ? '#' : ' ');
            }

            putchar(' ');

            if (y % 5 == 0) {
                printf("T %d\n", y);
            } else {
                printf("|\n");
            }
        }
    }

    printf("    +-");
    for (i = 0; i < bins; i++) {
        long u;
        u = start + 2L * (long)i;
        putchar((u % 10L == 0L) ? '|' : '-');
    }
    printf("-+\n");

    {
        int axis_len;
        char axisbuf[MAX_LINE];
        int j;

        axis_len = 2 + bins;
        if (axis_len >= MAX_LINE) axis_len = MAX_LINE - 1;

        for (j = 0; j < axis_len; j++) axisbuf[j] = ' ';
        axisbuf[axis_len] = '\0';

        for (i = 0; i < bins; i++) {
            long u;
            u = start + 2L * (long)i;
            if (u % 10L == 0L) {
                char num[32];
                int len;
                int pos;
                int t;

                sprintf(num, "%ld", u);
                len = (int)strlen(num);

                pos = (2 + i) - (len - 1);
                if (pos < 0) pos = 0;
                if (pos + len <= axis_len) {
                    for (t = 0; t < len; t++) axisbuf[pos + t] = num[t];
                }
            }
        }

        j = axis_len - 1;
        while (j >= 0 && axisbuf[j] == ' ') j--;
        axisbuf[j + 1] = '\0';

        printf("    ");
        puts(axisbuf);
    }

    free(counts);
    return 0;
}

static void mismatched_error(void) {
    printf("./hist: Mismatched cells\n");
    exit(1);
}

static void quotes_error(void) {
    printf("./hist: Unsupported quotes\n");
    exit(2);
}

static int count_cells(const char *line) {
    int count;
    const char *p;

    count = 1;
    p = line;
    while (*p && *p != '\n') {
        if (*p == ',') count++;
        p++;
    }
    return count;
}

static long parse_cell(const char *start, const char *end) {
    char buf[MAX_LINE];
    int i;
    char *tail;
    long val;

    i = 0;
    while (start < end) {
        if (*start != ' ' && *start != '\t') {
            if (i >= MAX_LINE - 1) return 0;
            buf[i++] = *start;
        }
        start++;
    }
    buf[i] = '\0';

    if (buf[0] == '\0') return 0;

    val = strtol(buf, &tail, 10);
    if (tail == buf) return 0;
    if (*tail != '\0') return 0;

    return val;
}

/* math helper funcs */

static long floor_div(long a, long d) {
    long q;
    q = a / d;
    if (a < 0 && (a % d) != 0) q--;
    return q;
}

static long ceil_div(long a, long d) {
    long q;
    q = a / d;
    if (a > 0 && (a % d) != 0) q++;
    return q;
}

static long round_down_10(long x) {
    return floor_div(x, 10L) * 10L;
}

static long round_up_10(long x) {
    return ceil_div(x, 10L) * 10L;
}

static int round_up_5(int x) {
    if (x <= 0) return 0;
    return (x % 5 == 0) ? x : (x + (5 - (x % 5)));
}
