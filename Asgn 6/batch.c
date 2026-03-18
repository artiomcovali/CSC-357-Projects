#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

typedef struct {
    pid_t pid;
    char **args;
    int   argc;
} Job;

static Job   *jobs_running     = NULL;
static int    jobs_running_cnt = 0;
static int    jobs_running_cap = 0;

static volatile sig_atomic_t got_term_signal = 0;

static void add_job(pid_t pid, char **args, int argc) {
    if (jobs_running_cnt >= jobs_running_cap) {
        jobs_running_cap = jobs_running_cap ? jobs_running_cap * 2 : 8;
        Job *tmp = realloc(jobs_running, jobs_running_cap * sizeof(Job));
        if (!tmp) {
            perror("realloc");
            for (int i = 0; i < jobs_running_cnt; i++) kill(jobs_running[i].pid, SIGTERM);
            exit(EXIT_FAILURE);
        }
        jobs_running = tmp;
    }
    jobs_running[jobs_running_cnt].pid  = pid;
    jobs_running[jobs_running_cnt].args = args;
    jobs_running[jobs_running_cnt].argc = argc;
    jobs_running_cnt++;
}

static int remove_job(pid_t pid, Job *out) {
    for (int i = 0; i < jobs_running_cnt; i++) {
        if (jobs_running[i].pid == pid) {
            *out = jobs_running[i];
            jobs_running[i] = jobs_running[--jobs_running_cnt];
            return 0;
        }
    }
    return -1;
}

static void kill_all_running(void) {
    for (int i = 0; i < jobs_running_cnt; i++) {
        kill(jobs_running[i].pid, SIGTERM);
    }
}

static void signal_handler(int sig) {
    (void)sig;
    got_term_signal = 1;
    kill_all_running();
}

static void print_cmd(const char *prefix, char **args, int argc) {
    fprintf(stderr, "%s", prefix);
    for (int i = 0; i < argc; i++) {
        if (i > 0) fputc(' ', stderr);
        fprintf(stderr, "%s", args[i]);
    }
    fputc('\n', stderr);
}

static void usage(const char *prog) {
    fprintf(stderr, "usage: %s [-n N] [-e] [-v] -- COMMAND [-- COMMAND ...]\n", prog);
}

int main(int argc, char *argv[]) {
    int max_jobs = -1;
    int eflag = 0, vflag = 0;
    int i = 1;

    while (i < argc && !(argv[i][0] == '-' && argv[i][1] == '-' && argv[i][2] == '\0')) {
        if (strcmp(argv[i], "-e") == 0) {
            eflag = 1; i++;
        } else if (strcmp(argv[i], "-v") == 0) {
            vflag = 1; i++;
        } else if (strcmp(argv[i], "-n") == 0) {
            if (i + 1 >= argc) { usage(argv[0]); return EXIT_FAILURE; }
            char *end;
            long n = strtol(argv[i+1], &end, 10);
            if (*end != '\0' || n < 1) { usage(argv[0]); return EXIT_FAILURE; }
            max_jobs = (int)n;
            i += 2;
        } else {
            usage(argv[0]); return EXIT_FAILURE;
        }
    }

    typedef struct { char **args; int argc; } Cmd;
    int cmd_cap = 8, cmd_cnt = 0;
    Cmd *cmds = malloc(cmd_cap * sizeof(Cmd));
    if (!cmds) { perror("malloc"); return EXIT_FAILURE; }

    while (i < argc) {
        if (strcmp(argv[i], "--") != 0) { usage(argv[0]); free(cmds); return EXIT_FAILURE; }
        i++;
        if (i >= argc) { usage(argv[0]); free(cmds); return EXIT_FAILURE; }

        int start = i;
        while (i < argc && strcmp(argv[i], "--") != 0) i++;

        if (i == start) { usage(argv[0]); free(cmds); return EXIT_FAILURE; }

        if (cmd_cnt >= cmd_cap) {
            cmd_cap *= 2;
            Cmd *tmp = realloc(cmds, cmd_cap * sizeof(Cmd));
            if (!tmp) { perror("realloc"); free(cmds); return EXIT_FAILURE; }
            cmds = tmp;
        }
        cmds[cmd_cnt].args = &argv[start];
        cmds[cmd_cnt].argc = i - start;
        cmd_cnt++;
    }

    if (cmd_cnt == 0) { usage(argv[0]); free(cmds); return EXIT_FAILURE; }
    if (max_jobs == -1) max_jobs = cmd_cnt;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    int overall_success = 1;
    int abort_requested = 0;
    int next_cmd = 0;
    int active = 0;

    while (next_cmd < cmd_cnt || active > 0) {
        if (got_term_signal && !abort_requested) {
            overall_success = 0;
            abort_requested = 1;
            kill_all_running();
        }

        while (!abort_requested && next_cmd < cmd_cnt && active < max_jobs) {
            int cargc = cmds[next_cmd].argc;
            char **child_argv = malloc((cargc + 1) * sizeof(char *));
            if (!child_argv) {
                perror("malloc");
                overall_success = 0;
                if (eflag) {
                    abort_requested = 1;
                    kill_all_running();
                }
                next_cmd++;
                continue;
            }
            for (int j = 0; j < cargc; j++) child_argv[j] = cmds[next_cmd].args[j];
            child_argv[cargc] = NULL;

            if (vflag) print_cmd("+ ", cmds[next_cmd].args, cargc);

            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
                free(child_argv);
                overall_success = 0;
                if (eflag) {
                    abort_requested = 1;
                    kill_all_running();
                }
                next_cmd++;
                continue;
            }

            if (pid == 0) {
                signal(SIGINT,  SIG_DFL);
                signal(SIGQUIT, SIG_DFL);
                signal(SIGTERM, SIG_DFL);
                execvp(child_argv[0], child_argv);
                perror(child_argv[0]);
                _exit(EXIT_FAILURE);
            }

            free(child_argv);
            add_job(pid, cmds[next_cmd].args, cargc);
            active++;
            next_cmd++;
        }

        if (active == 0) break;

        int status;
        pid_t done;
        do {
            done = wait(&status);
        } while (done < 0 && errno == EINTR);

        if (done < 0) break;

        Job finished;
        if (remove_job(done, &finished) == 0) {
            active--;
            if (vflag) print_cmd("- ", finished.args, finished.argc);
        } else {
            if (active > 0) active--;
        }

        int exit_status = EXIT_FAILURE;
        if (WIFEXITED(status)) exit_status = WEXITSTATUS(status);

        if (exit_status != EXIT_SUCCESS) {
            overall_success = 0;
            if (eflag && !abort_requested) {
                abort_requested = 1;
                kill_all_running();
            }
        }

        if (got_term_signal && !abort_requested) {
            overall_success = 0;
            abort_requested = 1;
            kill_all_running();
        }
    }

    free(cmds);
    free(jobs_running);

    return overall_success ? EXIT_SUCCESS : EXIT_FAILURE;
}