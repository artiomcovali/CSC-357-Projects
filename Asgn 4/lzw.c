#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MAX_CODE 4095
#define CODE_BITS 12
#define ASCII_MAX 0x7F
#define INIT_DICT_SIZE (ASCII_MAX + 1)

typedef struct {
    unsigned char *data;
    size_t len;
} entry_t;

static entry_t *dict = NULL;
static int dict_capacity = 0;
static int next_code = 0;

static int ensure_dict_capacity(int cap) {
    if (cap <= dict_capacity) return 0;
    entry_t *newd = realloc(dict, sizeof(entry_t) * cap);
    if (!newd) return -1;
    for (int i = dict_capacity; i < cap; ++i) {
        newd[i].data = NULL;
        newd[i].len = 0;
    }
    dict = newd;
    dict_capacity = cap;
    return 0;
}

static int dict_add(const unsigned char *s, size_t len) {
    if (next_code > MAX_CODE) return -1;
    if (ensure_dict_capacity(next_code + 1) != 0) return -1;
    unsigned char *copy = malloc(len);
    if (!copy) return -1;
    memcpy(copy, s, len);
    dict[next_code].data = copy;
    dict[next_code].len = len;
    next_code++;
    return next_code - 1;
}

static void dict_free(void) {
    if (!dict) return;
    for (int i = 0; i < dict_capacity; ++i) {
        free(dict[i].data);
        dict[i].data = NULL;
        dict[i].len = 0;
    }
    free(dict);
    dict = NULL;
    dict_capacity = 0;
    next_code = 0;
}

static int dict_find(const unsigned char *s, size_t len) {
    for (int i = 0; i < next_code; ++i) {
        if (dict[i].len == len && memcmp(dict[i].data, s, len) == 0) {
            return i;
        }
    }
    return -1;
}

static int dict_init_ascii(void) {
    dict_free();
    if (ensure_dict_capacity(MAX_CODE + 1) != 0) return -1;
    next_code = 0;
    for (int c = 0; c <= ASCII_MAX; ++c) {
        unsigned char *p = malloc(1);
        if (!p) return -1;
        p[0] = (unsigned char)c;
        dict[next_code].data = p;
        dict[next_code].len = 1;
        next_code++;
    }
    return 0;
}

typedef struct {
    unsigned int buf;
    int bits;
    FILE *out;
} bit_writer_t;

static void bw_init(bit_writer_t *bw, FILE *out) {
    bw->buf = 0;
    bw->bits = 0;
    bw->out = out;
}

static int bw_write_code(bit_writer_t *bw, unsigned int code) {
    bw->buf = (bw->buf << CODE_BITS) | (code & ((1u << CODE_BITS) - 1));
    bw->bits += CODE_BITS;

    while (bw->bits >= 8) {
        int shift = bw->bits - 8;
        unsigned int byte = (bw->buf >> shift) & 0xFFu;
        if (fputc((int)byte, bw->out) == EOF) return -1;
        bw->bits -= 8;
        bw->buf &= ((1u << bw->bits) - 1);
    }
    return 0;
}

static int bw_flush(bit_writer_t *bw) {
    if (bw->bits > 0) {
        unsigned int byte = (bw->buf << (8 - bw->bits)) & 0xFFu;
        if (fputc((int)byte, bw->out) == EOF) return -1;
        bw->bits = 0;
        bw->buf = 0;
    }
    return 0;
}

typedef struct {
    unsigned int buf;
    int bits;
    FILE *in;
} bit_reader_t;

static void br_init(bit_reader_t *br, FILE *in) {
    br->buf = 0;
    br->bits = 0;
    br->in = in;
}

static int br_read_code(bit_reader_t *br, unsigned int *code) {
    while (br->bits < CODE_BITS) {
        int ch = fgetc(br->in);
        if (ch == EOF) {
            if (br->bits == 0) return 0;
            return 0;
        }
        br->buf = (br->buf << 8) | (unsigned int)(unsigned char)ch;
        br->bits += 8;
    }
    int shift = br->bits - CODE_BITS;
    unsigned int val = (br->buf >> shift) & ((1u << CODE_BITS) - 1);
    br->bits -= CODE_BITS;
    br->buf &= ((1u << br->bits) - 1);
    *code = val;
    return 1;
}

static int encode_file(FILE *in, FILE *out) {
    if (dict_init_ascii() != 0) return -1;

    bit_writer_t bw;
    bw_init(&bw, out);

    unsigned char c;
    size_t r;
    unsigned char *curr = NULL;
    size_t curr_len = 0;

    r = fread(&c, 1, 1, in);
    if (r == 0) {
        bw_flush(&bw);
        return 0;
    }

    curr = malloc(1);
    if (!curr) return -1;
    curr[0] = c;
    curr_len = 1;

    while (1) {
        unsigned char nextc;
        r = fread(&nextc, 1, 1, in);
        if (r == 0) {
            int code = dict_find(curr, curr_len);
            bw_write_code(&bw, (unsigned int)code);
            break;
        }

        unsigned char *cand = malloc(curr_len + 1);
        memcpy(cand, curr, curr_len);
        cand[curr_len] = nextc;

        int cand_code = dict_find(cand, curr_len + 1);
        if (cand_code >= 0) {
            free(curr);
            curr = cand;
            curr_len++;
        } else {
            if (next_code <= MAX_CODE) dict_add(cand, curr_len + 1);
            free(cand);
            int code = dict_find(curr, curr_len);
            bw_write_code(&bw, (unsigned int)code);
            free(curr);
            curr = malloc(1);
            curr[0] = nextc;
            curr_len = 1;
        }
    }

    bw_flush(&bw);
    free(curr);
    return 0;
}

static int decode_file(FILE *in, FILE *out) {
    if (dict_init_ascii() != 0) return -1;

    bit_reader_t br;
    br_init(&br, in);

    unsigned int code;
    if (br_read_code(&br, &code) <= 0) return 0;

    fwrite(dict[code].data, 1, dict[code].len, out);

    unsigned char *prev = malloc(dict[code].len);
    size_t prev_len = dict[code].len;
    memcpy(prev, dict[code].data, prev_len);

    while (br_read_code(&br, &code) > 0) {
        unsigned char *entry;
        size_t entry_len;

        if ((int)code < next_code) {
            entry_len = dict[code].len;
            entry = malloc(entry_len);
            memcpy(entry, dict[code].data, entry_len);
        } else {
            entry_len = prev_len + 1;
            entry = malloc(entry_len);
            memcpy(entry, prev, prev_len);
            entry[entry_len - 1] = prev[0];
        }

        fwrite(entry, 1, entry_len, out);

        if (next_code <= MAX_CODE) {
            unsigned char *newstr = malloc(prev_len + 1);
            memcpy(newstr, prev, prev_len);
            newstr[prev_len] = entry[0];
            dict[next_code].data = newstr;
            dict[next_code].len = prev_len + 1;
            next_code++;
        }

        free(prev);
        prev = entry;
        prev_len = entry_len;
    }

    free(prev);
    return 0;
}

static void usage(void) {
    fprintf(stderr, "usage: ./lzw (-c | -x) SOURCE DESTINATION\n");
}

int main(int argc, char **argv) {
    if (argc != 4) {
        usage();
        return EXIT_FAILURE;
    }

    int encode = strcmp(argv[1], "-c") == 0;
    int decode = strcmp(argv[1], "-x") == 0;

    if (!encode && !decode) {
        usage();
        return EXIT_FAILURE;
    }

    FILE *in = fopen(argv[2], "rb");
    if (!in) {
        perror(argv[0]);
        return EXIT_FAILURE;
    }

    FILE *out = fopen(argv[3], "wb");
    if (!out) {
        perror(argv[0]);
        fclose(in);
        return EXIT_FAILURE;
    }

    int rv = encode ? encode_file(in, out) : decode_file(in, out);

    fclose(in);
    fclose(out);
    dict_free();

    return rv == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
