#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define LISTEN_BACKLOG 128
#define BUF_SIZE 4096

static void usage(const char *progname) {
    fprintf(stderr,
        "usage: %s [-u USERNAME] [-t TIMEOUT] PORT [HOSTNAME:PORT]\n",
        progname);
    exit(EXIT_FAILURE);
}

static struct pollfd *pfds = NULL;
static int pfds_len = 0;
static int pfds_cap = 0;

static void pfds_add(int fd, short events) {
    if (pfds_len == pfds_cap) {
        pfds_cap = pfds_cap ? pfds_cap * 2 : 8;
        pfds = realloc(pfds, pfds_cap * sizeof(*pfds));
        if (!pfds) {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
    }
    pfds[pfds_len].fd = fd;
    pfds[pfds_len].events = events;
    pfds[pfds_len].revents = 0;
    pfds_len++;
}

static void pfds_remove(int idx) {
    if (idx < pfds_len - 1) {
        pfds[idx] = pfds[pfds_len - 1];
    }
    pfds_len--;
}

static int make_listener(const char *port) {
    struct addrinfo hints, *res, *p;
    int sockfd, yes = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, port, &hints, &res) != 0) {
        perror(port);
        exit(EXIT_FAILURE);
    }

    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0) continue;

        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) < 0) {
            close(sockfd);
            continue;
        }
        break;
    }

    if (p == NULL) {
        freeaddrinfo(res);
        perror(port);
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(res);

    if (listen(sockfd, LISTEN_BACKLOG) < 0) {
        perror("listen");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

static int make_connection(const char *hostname, const char *port) {
    struct addrinfo hints, *res, *p;
    int sockfd, status;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(hostname, port, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "%s: %s\n", hostname, gai_strerror(status));
        exit(EXIT_FAILURE);
    }

    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0) continue;

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) < 0) {
            close(sockfd);
            continue;
        }
        break;
    }

    if (p == NULL) {
        freeaddrinfo(res);
        fprintf(stderr, "%s: connection failed\n", hostname);
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(res);
    return sockfd;
}

static int send_all(int fd, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, buf + sent, len - sent);
        if (n <= 0) return -1;
        sent += n;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    const char *username = "nobody";
    int timeout = -1;
    int opt_idx = 1;

    while (opt_idx < argc) {
        if (strcmp(argv[opt_idx], "-u") == 0) {
            if (opt_idx + 1 >= argc) usage(argv[0]);
            username = argv[opt_idx + 1];
            opt_idx += 2;
        } else if (strcmp(argv[opt_idx], "-t") == 0) {
            if (opt_idx + 1 >= argc) usage(argv[0]);
            timeout = atoi(argv[opt_idx + 1]);
            opt_idx += 2;
        } else if (argv[opt_idx][0] == '-') {
            usage(argv[0]);
        } else {
            break;
        }
    }

    int remaining = argc - opt_idx;
    if (remaining < 1 || remaining > 2) {
        usage(argv[0]);
    }

    const char *local_port = argv[opt_idx];
    char *remote_host = NULL;
    char *remote_port = NULL;
    char *remote_arg_copy = NULL;

    if (remaining == 2) {
        remote_arg_copy = strdup(argv[opt_idx + 1]);
        if (!remote_arg_copy) {
            perror("strdup");
            exit(EXIT_FAILURE);
        }
        char *colon = strrchr(remote_arg_copy, ':');
        if (!colon || colon == remote_arg_copy) {
            free(remote_arg_copy);
            usage(argv[0]);
        }
        *colon = '\0';
        remote_host = remote_arg_copy;
        remote_port = colon + 1;
    }

    int listener = make_listener(local_port);

    fcntl(listener, F_SETFL, O_NONBLOCK);

    pfds_add(STDIN_FILENO, POLLIN);
    pfds_add(listener, POLLIN);

    if (remote_host) {
        int conn = make_connection(remote_host, remote_port);
        fcntl(conn, F_SETFL, O_NONBLOCK);
        pfds_add(conn, POLLIN);
    }

    free(remote_arg_copy);

    size_t prefix_len = strlen(username) + 2;
    char *prefix = malloc(prefix_len + 1);
    if (!prefix) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    snprintf(prefix, prefix_len + 1, "%s: ", username);

    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    int stdin_open = 1;
    char buf[BUF_SIZE];

    while (1) {
        int ret = poll(pfds, pfds_len, timeout);

        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }

        if (ret == 0) {
            break;
        }

        for (int i = 0; i < pfds_len; i++) {
            if (pfds[i].revents == 0) continue;

            if (pfds[i].fd == STDIN_FILENO && stdin_open) {
                if (pfds[i].revents & POLLIN) {
                    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
                    if (n <= 0) {
                        stdin_open = 0;
                        pfds_remove(i);
                        i--;
                        continue;
                    }

                    size_t msg_len = prefix_len + n;
                    char *msg = malloc(msg_len);
                    if (!msg) {
                        perror("malloc");
                        break;
                    }
                    memcpy(msg, prefix, prefix_len);
                    memcpy(msg + prefix_len, buf, n);

                    for (int j = 0; j < pfds_len; j++) {
                        if (pfds[j].fd == STDIN_FILENO) continue;
                        if (pfds[j].fd == listener) continue;
                        send_all(pfds[j].fd, msg, msg_len);
                    }
                    free(msg);
                } else if (pfds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                    stdin_open = 0;
                    pfds_remove(i);
                    i--;
                }
            } else if (pfds[i].fd == listener) {
                while (1) {
                    int newfd = accept(listener, NULL, NULL);
                    if (newfd < 0) {
                        break;
                    }
                    fcntl(newfd, F_SETFL, O_NONBLOCK);
                    pfds_add(newfd, POLLIN);
                }
            } else {
                if (pfds[i].revents & POLLIN) {
                    ssize_t n = read(pfds[i].fd, buf, sizeof(buf));
                    if (n <= 0) {
                        close(pfds[i].fd);
                        pfds_remove(i);
                        i--;
                        continue;
                    }

                    write(STDOUT_FILENO, buf, n);

                    int sender_fd = pfds[i].fd;
                    for (int j = 0; j < pfds_len; j++) {
                        if (pfds[j].fd == STDIN_FILENO) continue;
                        if (pfds[j].fd == listener) continue;
                        if (pfds[j].fd == sender_fd) continue;
                        send_all(pfds[j].fd, buf, n);
                    }
                } else if (pfds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                    close(pfds[i].fd);
                    pfds_remove(i);
                    i--;
                }
            }
        }
    }

    for (int i = 0; i < pfds_len; i++) {
        if (pfds[i].fd != STDIN_FILENO) {
            close(pfds[i].fd);
        }
    }
    free(pfds);
    free(prefix);

    return EXIT_SUCCESS;
}
