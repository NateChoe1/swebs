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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>

#include <runner.h>
#include <sitefile.h>

FILE *logs;

int main(int argc, char **argv) {
	char *logout = "/var/log/swebs.log";
	char *sitefile = NULL;
	int processes = 8;
	uint16_t port = htons(80);
	int backlog = 100;
	for (;;) {
		int c = getopt(argc, argv, "o:j:s:c:p:b:h");
		if (c == -1)
			break;
		switch (c) {
			case 'o':
				logout = optarg;
				break;
			case 'j':
				processes = atoi(optarg);
				break;
			case 's':
				sitefile = optarg;
				break;
			case 'p':
				port = htons(atoi(optarg));
				break;
			case 'b':
				backlog = atoi(optarg);
				break;
			case 'h':
				printf(
"Usage: swebs [options]\n"
"  -o [out]                  Set the log file (default: /var/log/swebs.log)\n"
"  -j [cores]                Use that many cores (default: 8)\n"
"  -s [site file]            Use that site file (required)\n"
"  -p [port]                 Set the port (default: 443)\n"
"  -b [backlog]              Set the socket backlog (default: 100)\n"
				);
				exit(EXIT_SUCCESS);
			case '?':
				exit(EXIT_FAILURE);
		}
	}

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	assert(fd >= 0);
	int opt = 1;
	assert(setsockopt(fd, SOL_SOCKET,
	                  SO_REUSEPORT,
	                  &opt, sizeof(opt)) >= 0);
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = port;
	socklen_t addrlen = sizeof(addr);
	assert(bind(fd, (struct sockaddr *) &addr, addrlen) >= 0);
	assert(listen(fd, backlog) >= 0);

	if (sitefile == NULL) {
		fprintf(stderr, "No sitefile configured\n");
		exit(EXIT_FAILURE);
	}
	Sitefile *site = parseFile(sitefile);
	if (site == NULL) {
		fprintf(stderr, "Invalid sitefile %s\n", sitefile);
		exit(EXIT_FAILURE);
	}

	logs = fopen(logout, "a");
	if (logs == NULL) {
		fprintf(stderr, "Couldn't open logs file %s\n", logout);
		exit(EXIT_FAILURE);
	}

	int *pending = calloc(processes - 1, sizeof(int));
	int *schedule = malloc(2 * sizeof(int));
	if (schedule == NULL)
		exit(EXIT_FAILURE);
	schedule[0] = -1;
	pthread_t *threads = malloc(sizeof(pthread_t) * (processes - 1));
	if (threads == NULL)
		exit(EXIT_FAILURE);

	for (int i = 0; i < processes - 1; i++) {
		RunnerArgs *args = malloc(sizeof(RunnerArgs));
		if (args == NULL)
			exit(EXIT_FAILURE);
		args->site = site;
		args->pending = pending;
		args->schedule = schedule;
		args->id = i;
		pthread_create(threads + i, NULL,
		               (void*(*)(void*)) runServer, args);
	}

	for (;;) {
		fsync(fd);
		//TODO: Find out why this works
		if (schedule[0] == -1) {
			int newfd = accept(fd, (struct sockaddr *) &addr,
			                       &addrlen);
			if (newfd < 0)
				exit(EXIT_FAILURE);
			int flags = fcntl(newfd, F_GETFL);
			if (fcntl(newfd, F_SETFL, flags | O_NONBLOCK))
				exit(EXIT_FAILURE);
			int lowestThread = 0;
			int lowestCount = pending[0];
			for (int i = 1; i < processes - 1; i++) {
				if (pending[i] < lowestCount) {
					lowestThread = i;
					lowestCount = pending[i];
				}
			}
			schedule[1] = newfd;
			schedule[0] = lowestThread;
		}
	}
}
