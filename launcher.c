#include <signal.h>
#include <getopt.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <xcb/xcb.h>
#include <xcb/randr.h>

// Delay (in seconds) before accepting a new event of the same type
#define EVENTS_DELAY 3

// indent -kr -i8
static int VERBOSE = 0;
static int DAEMON = 0;

static xcb_connection_t *con;

static void sigterm_handler(int signum) {
	signal(signum, SIG_DFL);
    xcb_flush(con);
    xcb_disconnect(con);
	exit(EXIT_SUCCESS);
}

static void ar_log(const char *format, ...) {
	va_list args;

	if (VERBOSE) {
        va_start(args, format);
        if (DAEMON) vsyslog(LOG_INFO, format, args);
        else vprintf(format, args);
    	va_end(args);
    	fflush(stdout);
    }
}

static void ar_error(const char *format, ...) {
	va_list args;

	if (VERBOSE) {
        va_start(args, format);
        if (DAEMON) vsyslog(LOG_ERR, format, args);
        else vfprintf(stderr, format, args);
    	va_end(args);
    	fflush(stderr);
    }
}

static int ar_launch() {
	pid_t pid = fork();
	if (pid == 0) {
		static char *argv[] =
		    { "autorandr", "--change", "--force", "--default", "default", NULL };
        setsid();
		int ret = execvp(argv[0], argv);
        if (ret) {
            exit(EXIT_FAILURE);
        }
	}
	return 0;
}

static void ar_help() {
    static const char *help_str =
	    "Usage: autorandr_launcher [OPTION]\n"
	    "\n"
	    "Listens to X server screen change events and launches autorandr after an event occurs.\n"
	    "\n"
	    "\t-h,--help\t\t\tDisplay this help and exit\n"
	    "\t-d, --daemonize\t\t\tDaemonize program\n"
	    "\t--verbose\t\t\tOutput debugging information (prevents daemonizing)\n"
	    "\t--version\t\t\tDisplay version and exit\n";

    puts(help_str);
}

static void ar_version() {
    puts("v0.5\n");
}

static void ar_screen_change_listener() {
    xcb_timestamp_t last_timestamp = (xcb_timestamp_t) 0;
    time_t last_time = time(NULL);
    xcb_generic_event_t *evt;

    while (1) {
        ar_log("Waiting for event...\n");
        evt = xcb_wait_for_event(con);
        if (evt == NULL) {
            break;
        }
        if (evt->response_type & XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE) {
            time_t evt_time = time(NULL);

            ar_log("Screen change event\n");

			xcb_randr_screen_change_notify_event_t *randr_evt =
			    (xcb_randr_screen_change_notify_event_t *) evt;
			if (last_timestamp == 0 || // The first event can happen as early as possible
                (last_timestamp != randr_evt->timestamp &&
                    evt_time > (last_time + EVENTS_DELAY))) {
				ar_log("Launch autorandr!\n");
				ar_launch();
                last_timestamp = randr_evt->timestamp;
                last_time = evt_time;
			}
        }
        free(evt);
    }
    ar_error("Connection closed!");
}

int main(int argc, char **argv) {
	int help = 0;
	int version = 0;

	const struct option long_options[] = {
		{ "help", no_argument, &help, 1 },
		{ "daemonize", no_argument, &DAEMON, 1 },
		{ "version", no_argument, &version, 1 },
		{ "verbose", no_argument, &VERBOSE, 1 },
	};

	static const char *short_options = "hd";

	int option_index = 0;
	int ch = 0;
	while (ch != -1) {
		ch = getopt_long(argc, argv, short_options, long_options,
				 &option_index);
		switch (ch) {
		case 'h':
			help = 1;
			break;
		case 'd':
			DAEMON = 1;
			break;
		}
	}

	if (help == 1) {
		ar_help();
		exit(0);
	}
	if (version == 1) {
		ar_version();
		exit(0);
	}

	// Check for already running daemon?

	// Daemonize
	if (DAEMON == 1) {
        struct sigaction sa;
        memset(&sa, 0, sizeof sa);
        sa.sa_handler = sigterm_handler;
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGQUIT, &sa, NULL);
        signal(SIGHUP, SIG_IGN);

		daemon(0, 0);
        if (VERBOSE) {
            // Setup System logging for daemon program
            openlog("autorandr-service", LOG_PID|LOG_CONS, LOG_USER);
        }
        ar_log("Running as daemon");
	}

	int screenNum;
	con = xcb_connect(NULL, &screenNum);
	int conn_error = xcb_connection_has_error(con);
	if (conn_error) {
		ar_error("Connection error!\n");
		exit(EXIT_FAILURE);
	}

	// Get the screen whose number is screenNum
	const xcb_setup_t *setup = xcb_get_setup(con);
	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);

	// we want the screen at index screenNum of the iterator
	for (int i = 0; i < screenNum; ++i) {
		xcb_screen_next(&iter);
	}
	xcb_screen_t *default_screen = iter.data;
	ar_log("Connected to server\n");

	// Subscribe to screen change events
	xcb_randr_select_input(con, default_screen->root,
        XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);
	xcb_flush(con);
    ar_screen_change_listener();
}
