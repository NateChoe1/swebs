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
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <poll.h>
#include <unistd.h>

#include <runner.h>
#include <sitefile.h>
#include <connections.h>

void *runServer(RunnerArgs *args) {
	Sitefile *site = args->site;
	int *pending = args->pending;
	int notify = args->notify;
	int id = args->id;

	int allocConns = 100;
	struct pollfd *fds = malloc(sizeof(struct pollfd) * allocConns);
	Connection *connections = malloc(sizeof(Connection) * allocConns);
	int connCount = 1;
	/* connections are 1 indexed because fds[0] is the notify fd. */
	assert(fds != NULL);
	assert(connections != NULL);
	fds[0].fd = notify;
	fds[0].events = POLLIN;

	for (;;) {
		int i;
		poll(fds, connCount, site->timeout);

		for (i = 1; i < connCount; i++) {
			if (updateConnection(connections + i, site))
				goto remove;
			continue;
remove:
			connCount--;
			memcpy(fds + i, fds + connCount,
					sizeof(struct pollfd));
			memcpy(connections + i, fds + connCount,
					sizeof(struct pollfd));
			pending[id]--;
		}

		if (fds[0].revents & POLLIN) {
			Stream *newstream;
			if (connCount >= allocConns) {
				struct pollfd *newfds;
				Connection *newconns;
				allocConns *= 2;
				newfds = realloc(fds,
					sizeof(struct pollfd) * allocConns);
				if (newfds == NULL)
					exit(EXIT_FAILURE);
				fds = newfds;

				newconns = realloc(connections,
					sizeof(Connection) * allocConns);
				if (newconns == NULL)
					exit(EXIT_FAILURE);
				connections = newconns;
			}
			if (read(notify, &newstream, sizeof(newstream))
					< sizeof(newstream))
				exit(EXIT_FAILURE);
			fds[connCount].fd = newstream->fd;
			fds[connCount].events = POLLIN;

			if (newConnection(newstream, connections + connCount))
				exit(EXIT_FAILURE);
			connCount++;
			pending[id]++;
		}
	}
	return NULL;
}
