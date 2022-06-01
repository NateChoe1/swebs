/*
   swebs - a simple web server
   Copyright (C) 2022  Nate Choe
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/un.h>
#include <pthread.h>
#include <sys/wait.h>

#include <swebs/util.h>
#include <swebs/setup.h>
#include <swebs/runner.h>
#include <swebs/sockets.h>
#include <swebs/sitefile.h>

typedef struct {
	pid_t pid;
	int fd;
} Runner;

static Runner *runners;
static int processes;
static int mainfd;
static int *pending;
static Listener *listener;
static Sitefile *site;
static struct sockaddr_un addr;
/* We want to be able to handle a signal at any time, so some global variables
 * are needed. */
static const int signals[] = {
	SIGPIPE, SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGABRT, SIGBUS, SIGFPE,
	SIGKILL, SIGSEGV, SIGTERM, SIGTTIN, SIGTTOU, SIGURG, SIGXCPU, SIGXFSZ, SIGPIPE,
};

static void exitClean(int signal) {
	freeListener(listener);
	close(mainfd);
	remove(addr.sun_path);
	exit(EXIT_SUCCESS);
}

static void setsignal(int signal, void (*handler)(int)) {
	struct sigaction action;
	sigset_t sigset;
	sigemptyset(&sigset);
	action.sa_handler = handler;
	action.sa_mask = sigset;
	action.sa_flags = SA_NODEFER;
	sigaction(signal, &action, NULL);
}

static void unsetsignal(int signal) {
	struct sigaction action;
	sigset_t sigset;
	sigemptyset(&sigset);
	action.sa_handler = SIG_DFL;
	action.sa_mask = sigset;
	action.sa_flags = SA_NODEFER;
	sigaction(signal, &action, NULL);
}

static void createProcess(int id) {
	pid_t pid;
	int connfd;
	int i;
	socklen_t addrlen;

	createLog("Creating a new process");
	pending[id] = 0;

	pid = fork();
	switch (pid) {
		case -1:
			createErrorLog("fork() failed", errno);
			exit(EXIT_FAILURE);
		case 0:
			break;
		default:
			addrlen = sizeof(addr);
			runners[id].pid = pid;
			runners[id].fd = accept(mainfd,
					(struct sockaddr *) &addr, &addrlen);
			return;
	}

	for (i = 0; i < sizeof(signals) / sizeof(signals[0]); i++)
		unsetsignal(signals[i]);
	unsetsignal(SIGCHLD);

	connfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (connfd < 0)
		exit(EXIT_FAILURE);
	if (connect(connfd, (struct sockaddr *) &addr, sizeof(addr))) {
		createErrorLog("connect() failed, killing child", errno);
		exit(EXIT_FAILURE);
	}
	close(mainfd);
	runServer(connfd, site, listener, pending, id);
	createLog("child runServer() finished");
	exit(EXIT_SUCCESS);
}

static void remakeChild(int signal) {
	pid_t pid;
	int i;
	pid = wait(NULL);
	createLog("A child has died, recreating");
	for (i = 0; i < processes - 1; i++) {
		if (runners[i].pid == pid) {
			close(runners[i].fd);
			createProcess(i);
			return;
		}
	}
}

int main(int argc, char **argv) {
	int i;
	int pendingid;

	setup(argc, argv, &site, &listener, &processes);

	pendingid = smalloc(sizeof(int) * (processes - 1));
	if (pendingid < 0) {
		createErrorLog("smalloc() failed", errno);
		exit(EXIT_FAILURE);
	}
	pending = saddr(pendingid);
	if (pending == NULL) {
		createErrorLog("saddr() failed", errno);
		exit(EXIT_FAILURE);
	}
	memset(pending, 0, sizeof(int) * (processes - 1));

	mainfd = socket(AF_UNIX, SOCK_STREAM, 0);
	addr.sun_family = AF_UNIX;

	strncpy(addr.sun_path, SERVER_PATH, sizeof(addr.sun_path) - 1);
	addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
	createTmpName(addr.sun_path);

	bind(mainfd, (struct sockaddr *) &addr, sizeof(addr));
	listen(mainfd, processes);

	runners = malloc(sizeof(Runner) * (processes - 1));
	for (i = 0; i < processes - 1; i++)
		createProcess(i);

	for (i = 0; i < sizeof(signals) / sizeof(signals[0]); i++)
		setsignal(signals[i], exitClean);

	setsignal(SIGCHLD, remakeChild);

	createLog("swebs started");

	for (;;) {
		int fd;
		int lowestProc;

		fd = acceptConnection(listener);
		if (fd < 0) {
			if (errno == ENOTSOCK || errno == EOPNOTSUPP ||
					errno == EINVAL) {
				createErrorLog("You've majorly screwed up. Good luck",
						errno);
				exit(EXIT_FAILURE);
			}
			continue;
		}
		createLog("Accepted stream");

		lowestProc = 0;
		for (i = 1; i < processes - 1; i++)
			if (pending[i] < pending[lowestProc])
				lowestProc = i;
		sendFd(fd, runners[lowestProc].fd);
	}
}
