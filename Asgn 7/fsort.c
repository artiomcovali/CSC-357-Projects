/* Defines functions for sorting arrays in parallel.
 * CSC 357, Assignment 7
 * Given code, Winter '24 */

#include "fsort.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static void merge(void *base, size_t n, size_t width, size_t mid,
                  int (*cmp)(const void *, const void *))
{
    char *arr = base;
    size_t i = 0, j = mid, k = 0;

    char *temp = malloc(n * width);
    if (!temp)
        return;

    while (i < mid && j < n) {
        if (cmp(arr + i * width, arr + j * width) <= 0) {
            memcpy(temp + k * width, arr + i * width, width);
            i++;
        } else {
            memcpy(temp + k * width, arr + j * width, width);
            j++;
        }
        k++;
    }

    while (i < mid) {
        memcpy(temp + k * width, arr + i * width, width);
        i++;
        k++;
    }

    while (j < n) {
        memcpy(temp + k * width, arr + j * width, width);
        j++;
        k++;
    }

    memcpy(arr, temp, n * width);
    free(temp);
}

/* fsort: Sorts an array using a parallelized merge sort.
 * TODO: Implement this function. It should sort the "n" elements of "base",
 *       each of "width" bytes, creating a child process to sort the latter
 *       half of the array if and only if "n > min", and returning 1 if the
 *       requisite child processes could not be created and 0 otherwise. */
int fsort(
 void *base, size_t n, size_t width, size_t min,
 int (*cmp)(const void *, const void *))
{
    if (n <= 1)
        return 0;

    if (n <= min) {
        size_t mid = n / 2;
        fsort(base, mid, width, min, cmp);
        fsort((char *)base + mid * width, n - mid, width, min, cmp);
        merge(base, n, width, mid, cmp);
        return 0;
    }

    size_t mid = n / 2;

    int pipefd[2];
    if (pipe(pipefd) == -1)
        return 1;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }

    if (pid == 0) {
        close(pipefd[0]);

        void *right = (char *)base + mid * width;
        int result = fsort(right, n - mid, width, min, cmp);

        if (result == 0) {
            size_t bytes = (n - mid) * width;
            size_t written = 0;
            while (written < bytes) {
                ssize_t w = write(pipefd[1], (char *)right + written, bytes - written);
                if (w <= 0)
                    break;
                written += (size_t)w;
            }
        }

        close(pipefd[1]);
        _exit(result);
    }

    close(pipefd[1]);

    int left_result = fsort(base, mid, width, min, cmp);

    void *right = (char *)base + mid * width;
    size_t bytes = (n - mid) * width;
    size_t read_total = 0;

    while (read_total < bytes) {
        ssize_t r = read(pipefd[0], (char *)right + read_total, bytes - read_total);
        if (r <= 0)
            break;
        read_total += (size_t)r;
    }

    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);

    int child_failed = !WIFEXITED(status) || WEXITSTATUS(status) != 0;
    if (left_result != 0 || child_failed || read_total != bytes)
        return 1;

    merge(base, n, width, mid, cmp);
    return 0;
}