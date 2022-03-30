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

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>

#include <swebs/util.h>
#include <swebs/setup.h>
#include <swebs/runner.h>
#include <swebs/sockets.h>
#include <swebs/sitefile.h>

int main(int argc, char **argv) {
	Sitefile *site;
	Listener *listener;
	int processes;

	int *pending, pendingid, (*notify)[2];
	pthread_t *threads;

	int i;

	setup(argc, argv, &site, &listener, &processes);

	pendingid = smalloc(sizeof(int) * (processes - 1));
	if (pendingid < 0) {
		createLog("smalloc() failed");
		exit(EXIT_FAILURE);
	}
	pending = saddr(pendingid);;
	if (pending == NULL) {
		createLog("saddr() failed");
		exit(EXIT_FAILURE);
	}
	memset(pending, 0, sizeof(int) * (processes - 1));

	notify = malloc(sizeof(int[2]) * (processes - 1));
	if (notify == NULL) {
		createLog("malloc() failed");
		exit(EXIT_FAILURE);
	}

	threads = malloc(sizeof(pthread_t) * (processes - 1));
	if (threads == NULL) {
		createLog("malloc() failed");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < processes - 1; i++) {
		RunnerArgs *args = malloc(sizeof(RunnerArgs));
		if (args == NULL) {
			createLog("malloc() failed");
			exit(EXIT_FAILURE);
		}
		if (pipe(notify[i])) {
			createLog("pipe() failed");
			exit(EXIT_FAILURE);
		}
		args->site = site;
		args->pendingid = pendingid;
		args->notify = notify[i][0];
		args->id = i;
		pthread_create(threads + i, NULL,
		               (void*(*)(void*)) runServer, args);
	}

	signal(SIGPIPE, SIG_IGN);

	createLog("swebs started");

	for (;;) {
		Stream *stream = acceptStream(listener, O_NONBLOCK);
		int lowestThread;
		createLog("Accepted stream");
		if (stream == NULL) {
			createLog("Accepting a stream failed");
			continue;
		}

		lowestThread = 0;
		for (i = 1; i < processes - 1; i++)
			if (pending[i] < pending[lowestThread])
				lowestThread = i;
		if (write(notify[lowestThread][1], &stream, sizeof(&stream))
				< sizeof(&stream))
			continue;
	}
	
}
