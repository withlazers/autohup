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
#include <unistd.h>

#define EXIT_ERROR_CODE 255
#define MAX_EVENTS 1
#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (MAX_EVENTS * (EVENT_SIZE + NAME_MAX))
#define INOTIFY_EVENT_MASK \
	IN_CLOSE_WRITE | IN_ATTRIB | IN_CREATE | IN_MOVE | IN_DELETE

#define SIG(x) \
	{ .number = SIG##x, .name = #x }
const struct {
	int number;
	char name[7];
} signals[] = {SIG(ALRM), SIG(HUP),  SIG(INT),  SIG(QUIT),  SIG(TERM),
			   SIG(URG),  SIG(USR1), SIG(USR2), SIG(WINCH), {0}};
#undef SIG

pid_t observed_pid = 0, event_script_pid = 0;
int observed_exit_status = 0;
int trigger_signal = SIGHUP;
int verbose = 0;
const char *event_script = NULL;
int inotify_fd = 0;

static void
wait_for_children(int options) {
	for (int pid, status; (pid = waitpid(-1, &status, options)) > 0;) {
		if (pid == event_script_pid) {
			event_script_pid = 0;
			if (observed_pid != 0) {
				break;
			}
		} else if (pid == observed_pid) {
			observed_pid = 0;
			observed_exit_status = status;
		}
	}

	if (observed_pid == 0 && event_script_pid == 0) {
		exit(observed_exit_status);
	}
}

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

static void
sig_handler_forward(const int sig) {
	if (observed_pid == 0) {
		return;
	}

	kill(observed_pid, sig);
}

static void
exec_event_script() {
	if (event_script == NULL) {
		return;
	}
	// We do not use system(3) here as it interferes with SIGCHLD
	event_script_pid = fork();
	if (event_script_pid == 0) {
		execl("/bin/sh", "/bin/sh", "-c", event_script, NULL);
	} else if (event_script_pid < 0) {
		perror("event script");
	} else {
		wait_for_children(0);
	}
}

static void
sig_handler_alarm(const int sig) {
	char buffer[BUF_LEN];
	fd_set rfds;
	struct timeval tv;

	if (observed_pid == 0) {
		return;
	}

	if (verbose) {
		fputs("DEBUG: executing event script\n", stderr);
	}
	exec_event_script();

	if (verbose) {
		fprintf(stderr, "DEBUG: sending signal %s to child process\n",
				get_sig_name(trigger_signal));
	}
	sig_handler_forward(trigger_signal);

	FD_ZERO(&rfds);
	FD_SET(inotify_fd, &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	while (select(1, &rfds, NULL, NULL, &tv) > 0) {
		read(inotify_fd, buffer, BUF_LEN);
	}
}

static void
sig_handler_child(const int sig) {
	wait_for_children(WNOHANG);
}

static int
list_signals() {
	int i;
	for (i = 0; signals[i].number != 0; i++) {
		fputs("SIG", stdout);
		puts(signals[i].name);
	}

	return EXIT_SUCCESS;
}

static int
usage(const char *arg0) {
	fprintf(stderr,
			"usage: %s [-s signal] [-e script] [-v] [path ...] -- command "
			"[argument ...]\n",
			arg0);
	fprintf(stderr, "       %s -l\n", arg0);
	return EXIT_ERROR_CODE;
}

int
main(int argc, char **argv) {
	int rv;
	char buffer[BUF_LEN];
	int opt;

	inotify_fd = inotify_init();

	while ((opt = getopt(argc, argv, "-s:e:lvV")) != -1) {
		switch (opt) {
		case 'l':
			return list_signals();
		case 'e':
			event_script = optarg;
			break;
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
			rv = inotify_add_watch(inotify_fd, optarg, INOTIFY_EVENT_MASK);
			if (rv < 0) {
				perror(optarg);
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
	signal(SIGALRM, sig_handler_alarm);

	observed_pid = fork();
	if (observed_pid < 0) {
		perror(argv[2]);
	} else if (observed_pid == 0) {
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
				fprintf(stderr,
						"DEBUG: scheduled sending %s to child process\n",
						get_sig_name(trigger_signal));
			}
			alarm(1);
		}
		return EXIT_ERROR_CODE;
	}
}
