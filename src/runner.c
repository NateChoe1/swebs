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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <pwd.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>

#include <swebs/util.h>
#include <swebs/runner.h>
#include <swebs/sitefile.h>
#include <swebs/connections.h>

void runServer(int connfd, Sitefile *site, volatile int *pending, int id) {
	int allocConns = 100;
	struct pollfd *fds;
	Connection *connections;
	int connCount;
	Context **contexts;
	int i;

	connCount = 1;
	fds = xmalloc(allocConns * sizeof *fds);
	connections = xmalloc(allocConns * sizeof *connections);
	fds[0].fd = connfd;
	fds[0].events = POLLIN;
	/* connections are 1 indexed because fds[0] is the notify fd. I hate
	 * that poll() forces us to do these hacks. */

	contexts = xmalloc(site->portcount * sizeof *contexts);

	for (i = 0; i < site->portcount; ++i) {
		Port *port = site->ports + i;
		switch (port->type) {
			case TCP:
				contexts[i] = createContext(TCP);
				break;
			case TLS:
				contexts[i] = createContext(TLS, port->key, port->cert);
				break;
			default:
				createLog("Socket type is somehow invalid");
				return;
		}
		if (contexts[i] == NULL) {
			createErrorLog("Failed to create context", errno);
			exit(EXIT_FAILURE);
		}
	}

	{
		struct passwd *swebs, *root;
		swebs = getpwnam("swebs");
		if (swebs == NULL)
			createLog("Couldn't find swebs user");
		else
			if (seteuid(swebs->pw_uid))
				createErrorLog("seteuid() failed", errno);
		root = getpwnam("root");
		if (root != NULL) {
		/* I don't know why this if statement could be false but we have it
		 * just in case. */
			if (geteuid() == root->pw_uid)
				createLog("swebs probably should not be run as root");
		}
	}

	for (;;) {
		poll(fds, connCount, -1);

		createFormatLog("poll() finished with %d connections", connCount);

		for (i = 1; i < connCount; i++) {
			if (fds[i].revents & POLLIN) {
				createFormatLog("Connection %d has data", i);
				if (updateConnection(connections + i, site))
					goto remove;
			}
			continue;
remove:
			{
				int remove, replace;
				remove = i;
				replace = connCount - 1;
				freeConnection(connections + remove);

				memcpy(fds + remove, fds + replace,
						sizeof(struct pollfd));
				memcpy(connections + remove,
						connections + replace,
						sizeof(struct pollfd));

				--pending[id];

				--i;
				--connCount;
			}
		}

		if (fds[0].revents & POLLIN) {
			Stream *newstream;
			int newfd;
			int portind;
			createLog("Main fd has data");
			newfd = recvFd(connfd, &portind, sizeof portind);
			if (newfd < 0) {
				createLog("Message received that included an invalid fd, quitting");
				exit(EXIT_FAILURE);
			}

			newstream = createStream(contexts[portind], O_NONBLOCK, newfd);
			if (newstream == NULL) {
				createLog(
"Stream couldn't be created from file descriptor");
				shutdown(newfd, SHUT_RDWR);
				close(newfd);
				continue;
			}

			if (connCount >= allocConns) {
				struct pollfd *newfds;
				Connection *newconns;
				allocConns *= 2;
				newfds = realloc(fds,
					sizeof(struct pollfd) * allocConns);
				if (newfds == NULL) {
					allocConns /= 2;
					continue;
				}
				fds = newfds;

				newconns = realloc(connections,
					sizeof(Connection) * allocConns);
				if (newconns == NULL) {
					allocConns /= 2;
					continue;
				}
				connections = newconns;
			}

			if (newConnection(newstream, connections + connCount, portind)) {
				createLog("Couldn't initialize connection from stream");
				continue;
			}
			fds[connCount].fd = newfd;
			fds[connCount].events = POLLIN;
			connCount++;
			pending[id]++;
		}
	}
}
