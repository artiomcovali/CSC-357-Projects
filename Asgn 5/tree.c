#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define INDENT_WIDTH 4

struct options {
    bool show_all;
    bool long_format;
    int max_depth;
};

static void usage(void) {
    fprintf(stderr, "usage: ./tree [-a] [-d N] [-l] [PATH]\n");
}

static void print_path_error(const char *path, int err) {
    printf("%s: %s\n", path, strerror(err));
}

static char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *copy = malloc(n);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, s, n);
    return copy;
}

static int name_cmp(const void *a, const void *b) {
    const char *const *sa = (const char *const *)a;
    const char *const *sb = (const char *const *)b;
    return strcmp(*sa, *sb);
}

static void mode_to_string(mode_t mode, char out[11]) {
    if (S_ISLNK(mode)) {
        memcpy(out, "lrwxrwxrwx", 10);
        out[10] = '\0';
        return;
    }

    out[0] = S_ISDIR(mode) ? 'd' :
             S_ISCHR(mode) ? 'c' :
             S_ISBLK(mode) ? 'b' :
             S_ISFIFO(mode) ? 'p' :
             S_ISSOCK(mode) ? 's' : '-';
    out[1] = (mode & S_IRUSR) ? 'r' : '-';
    out[2] = (mode & S_IWUSR) ? 'w' : '-';
    out[3] = (mode & S_IXUSR) ? 'x' : '-';
    out[4] = (mode & S_IRGRP) ? 'r' : '-';
    out[5] = (mode & S_IWGRP) ? 'w' : '-';
    out[6] = (mode & S_IXGRP) ? 'x' : '-';
    out[7] = (mode & S_IROTH) ? 'r' : '-';
    out[8] = (mode & S_IWOTH) ? 'w' : '-';
    out[9] = (mode & S_IXOTH) ? 'x' : '-';
    out[10] = '\0';
}

static void print_link_target(const char *path, off_t size_hint) {
    size_t cap = (size_hint > 0) ? (size_t)size_hint + 1 : 256;
    char *target = malloc(cap);
    if (target == NULL) {
        return;
    }

    while (1) {
        ssize_t len = readlink(path, target, cap);
        if (len < 0) {
            free(target);
            return;
        }

        if ((size_t)len < cap) {
            target[len] = '\0';
            printf(" -> %s", target);
            free(target);
            return;
        }

        if (cap > (SIZE_MAX / 2)) {
            free(target);
            return;
        }

        cap *= 2;
        char *tmp = realloc(target, cap);
        if (tmp == NULL) {
            free(target);
            return;
        }
        target = tmp;
    }
}

static void print_entry(const char *path, const char *name, int depth,
                        const struct stat *sb, const struct options *opt,
                        const char *err_suffix) {
    if (!opt->long_format) {
        if (err_suffix != NULL) {
            printf("%*s%s: %s\n", depth * INDENT_WIDTH, "", name, err_suffix);
        } else {
            printf("%*s%s\n", depth * INDENT_WIDTH, "", name);
        }
        return;
    }

    char mode[11];
    mode_to_string(sb->st_mode, mode);
    printf("%s %*s%s", mode, depth * INDENT_WIDTH, "", name);

    if (S_ISLNK(sb->st_mode)) {
        print_link_target(path, sb->st_size);
    }

    if (err_suffix != NULL) {
        printf(": %s", err_suffix);
    }

    putchar('\n');
}

static int traverse(const char *path, const char *name, int depth,
                    const struct options *opt) {
    struct stat sb;
    if (lstat(path, &sb) != 0) {
        print_path_error(path, errno);
        return 1;
    }

    if (!S_ISDIR(sb.st_mode)) {
        print_entry(path, name, depth, &sb, opt, NULL);
        return 0;
    }

    if (opt->max_depth >= 0 && depth >= opt->max_depth) {
        print_entry(path, name, depth, &sb, opt, NULL);
        return 0;
    }

    DIR *dir = opendir(path);
    if (dir == NULL) {
        print_entry(path, name, depth, &sb, opt, strerror(errno));
        return 1;
    }
    print_entry(path, name, depth, &sb, opt, NULL);

    size_t cap = 16;
    size_t count = 0;
    char **entries = malloc(cap * sizeof(*entries));
    if (entries == NULL) {
        closedir(dir);
        fprintf(stderr, "memory allocation failure\n");
        return 1;
    }

    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }
        if (!opt->show_all && de->d_name[0] == '.') {
            continue;
        }

        if (count == cap) {
            cap *= 2;
            char **tmp = realloc(entries, cap * sizeof(*entries));
            if (tmp == NULL) {
                for (size_t i = 0; i < count; i++) {
                    free(entries[i]);
                }
                free(entries);
                closedir(dir);
                fprintf(stderr, "memory allocation failure\n");
                return 1;
            }
            entries = tmp;
        }

        entries[count] = xstrdup(de->d_name);
        if (entries[count] == NULL) {
            for (size_t i = 0; i < count; i++) {
                free(entries[i]);
            }
            free(entries);
            closedir(dir);
            fprintf(stderr, "memory allocation failure\n");
            return 1;
        }
        count++;
    }

    closedir(dir);
    qsort(entries, count, sizeof(*entries), name_cmp);

    int rc = 0;
    for (size_t i = 0; i < count; i++) {
        size_t need = strlen(path) + strlen(entries[i]) + 2;
        char *child = malloc(need);
        if (child == NULL) {
            fprintf(stderr, "memory allocation failure\n");
            rc = 1;
            free(entries[i]);
            continue;
        }

        if (path[0] != '\0' && path[strlen(path) - 1] == '/') {
            snprintf(child, need, "%s%s", path, entries[i]);
        } else {
            snprintf(child, need, "%s/%s", path, entries[i]);
        }

        if (traverse(child, entries[i], depth + 1, opt) != 0) {
            rc = 1;
        }

        free(child);
        free(entries[i]);
    }

    free(entries);
    return rc;
}

int main(int argc, char *argv[]) {
    struct options opt = {
        .show_all = false,
        .long_format = false,
        .max_depth = -1,
    };

    opterr = 0;

    int ch;
    while ((ch = getopt(argc, argv, "ad:l")) != -1) {
        switch (ch) {
            case 'a':
                opt.show_all = true;
                break;
            case 'd': {
                char *end = NULL;
                errno = 0;
                long val = strtol(optarg, &end, 10);
                if (errno != 0 || end == optarg || *end != '\0' || val < 0 || val > INT_MAX) {
                    usage();
                    return 1;
                }
                opt.max_depth = (int)val;
                break;
            }
            case 'l':
                opt.long_format = true;
                break;
            default:
                usage();
                return 1;
        }
    }

    const char *path = ".";
    if (optind < argc) {
        path = argv[optind];
        optind++;
    }

    if (optind != argc) {
        usage();
        return 1;
    }

    return traverse(path, path, 0, &opt);
}
