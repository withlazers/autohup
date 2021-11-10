/**
 * @author      : Enno Boland (mail@eboland.de)
 * @file        : main
 * @created     : Tuesday Nov 09, 2021 18:54:40 CET
 */

#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define EXIT_ERROR_CODE 255
#define MAX_EVENTS 1
#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (MAX_EVENTS * (EVENT_SIZE + NAME_MAX))

#define SIG(x) \
	{ .number = SIG##x, .name = #x }
const struct {
	int number;
	char name[7];
} signals[] = {SIG(ALRM), SIG(HUP),  SIG(INT),  SIG(QUIT),  SIG(TERM),
			   SIG(URG),  SIG(USR1), SIG(USR2), SIG(WINCH), {0}};
#undef SIG

pid_t child = 0;
int trigger_signal = SIGHUP;
int verbose = 0;

static const char *
get_sig_name(const int signal) {
	int i;

	for (i = 0; signals[i].number != 0; i++) {
		if (signal == signals[i].number) {
			return signals[i].name;
		}
	}

	return NULL;
}

static int
get_sig_number(const char *signal) {
	int i;
	if (strncasecmp("SIG", signal, 3) == 0) {
		signal += 3;
	}

	for (i = 0; signals[i].number != 0; i++) {
		if (strcasecmp(signals[i].name, signal) == 0) {
			return signals[i].number;
		}
	}

	return -1;
}

void
sig_handler_forward(int sig) {
	if (child == 0) {
		return;
	}

	kill(child, sig);
}

static void
sig_handler_child(int sig) {
	int status;

	while (waitpid(-1, &status, WNOHANG) > 0) {
	}

	exit(status);
}

int
list_signals() {
	int i;
	for (i = 0; signals[i].number != 0; i++) {
		fputs("SIG", stdout);
		puts(signals[i].name);
	}

	return EXIT_SUCCESS;
}

int
usage(char *arg0) {
	fprintf(stderr,
			"usage: %s [-s signal] [-v] [path ...] -- command [argument ...]\n",
			arg0);
	fprintf(stderr, "       %s -l\n", arg0);
	return EXIT_ERROR_CODE;
}

int
main(int argc, char **argv) {
	int rv;
	int inotify_fd;
	char buffer[BUF_LEN];
	const char *observed = argv[1];
	int opt;

	inotify_fd = inotify_init();

	while ((opt = getopt(argc, argv, "-s:lvV")) != -1) {
		switch (opt) {
		case 'l':
			return list_signals();
		case 's':
			rv = atoi(optarg);
			if (rv == 0) {
				rv = get_sig_number(optarg);
			}
			if (rv < 0) {
				fprintf(stderr, "%s: Signal cannot be found\n", optarg);
				return usage(argv[0]);
			}
			trigger_signal = rv;
			break;
		case 'V':
			fputs("autohup-" VERSION "\n", stderr);
			return EXIT_ERROR_CODE;
		case 'v':
			verbose++;
			break;
		case 1:
			rv = inotify_add_watch(inotify_fd, optarg, IN_ALL_EVENTS);
			if (rv < 0) {
				perror(observed);
				return EXIT_ERROR_CODE;
			}
			break;
		default:
			return EXIT_ERROR_CODE;
		}
	}
	if (optind == argc) {
		return usage(argv[0]);
	}

	signal(SIGCHLD, sig_handler_child);

	child = fork();
	if (child < 0) {
		perror(argv[2]);
	} else if (child == 0) {
		argv += optind;
		execvp(argv[0], argv);
		perror(argv[0]);

		return EXIT_ERROR_CODE;
	} else {
		signal(SIGHUP, sig_handler_forward);
		signal(SIGINT, sig_handler_forward);
		signal(SIGQUIT, sig_handler_forward);
		signal(SIGUSR1, sig_handler_forward);
		signal(SIGUSR2, sig_handler_forward);
		signal(SIGTERM, sig_handler_forward);
		signal(SIGCONT, sig_handler_forward);
		for (;;) {
			rv = read(inotify_fd, buffer, BUF_LEN);
			if (rv < 0) {
				perror("inotify");
				return EXIT_ERROR_CODE;
			}

			if (verbose) {
				fprintf(stderr, "DEBUG: sending signal %s to child process\n",
						get_sig_name(trigger_signal));
			}
			sig_handler_forward(trigger_signal);
		}
		return EXIT_ERROR_CODE;
	}
}