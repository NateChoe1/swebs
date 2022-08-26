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

typedef struct {
	struct pollfd *fds;
	Connection *conns;
	int len;
	int alloc;
} ConnList;

static int createConnList(ConnList *list);
static int addConnList(ConnList *list, struct pollfd *fd, Connection *conn);
static void removeConnList(ConnList *list, int ind);
static void pollConnList(ConnList *list);
static void freeConnList(ConnList *list);

void runServer(int connfd, Sitefile *site, volatile int *pending, int id) {
	Context **contexts;
	int i;
	ConnList conns;

	if (createConnList(&conns))
		return;

	{
		struct pollfd newfd;
		Connection newconn;

		newfd.fd = connfd;
		newfd.events = POLLIN;

		if (addConnList(&conns, &newfd, &newconn)) {
			freeConnList(&conns);
			return;
		}
	}
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
		/* I don't know why this if statement could be false but we have
		 * it just in case. */
			if (geteuid() == root->pw_uid)
				createLog("swebs probably should not be run as root");
		}
	}

	for (;;) {
		pollConnList(&conns);

		createFormatLog("poll() finished with %d connections",
				conns.len);

		for (i = 1; i < conns.len; i++) {
			if (conns.fds[i].revents & POLLIN) {
				createFormatLog("Connection %d has data", i);
				if (updateConnection(conns.conns + i, site)) {
					freeConnection(conns.conns + i);
					removeConnList(&conns, i);
					--i;
				}
			}
		}

		if (conns.fds[0].revents & POLLIN) {
			Stream *newstream;
			Connection newconn;
			int portind;
			struct pollfd newfd;

			createLog("Main fd has data");
			newfd.fd = recvFd(connfd, &portind, sizeof portind);
			if (newfd.fd < 0) {
				createLog("Message received that included an invalid fd, quitting");
				exit(EXIT_FAILURE);
			}
			newfd.events = POLLIN;

			newstream = createStream(contexts[portind],
					O_NONBLOCK, newfd.fd);
			if (newstream == NULL) {
				createLog(
"Stream couldn't be created from file descriptor");
				shutdown(newfd.fd, SHUT_RDWR);
				close(newfd.fd);
				continue;
			}

			if (newConnection(newstream, &newconn, portind)) {
				createLog("Couldn't initialize connection from stream");
				continue;
			}

			if (addConnList(&conns, &newfd, &newconn)) {
				freeConnection(&newconn);
				continue;
			}
			pending[id]++;
		}
	}
}

static int createConnList(ConnList *list) {
	list->alloc = 100;
	list->fds = xmalloc(list->alloc * sizeof *list->fds);
	list->conns = xmalloc(list->alloc * sizeof *list->conns);
	list->len = 0;
	return 0;
}

static int addConnList(ConnList *list, struct pollfd *fd, Connection *conn) {
	if (list->len >= list->alloc) {
		int newalloc;
		struct pollfd *newfds;
		Connection *newconns;
		newalloc = list->alloc * 2;
		newfds = realloc(list->fds, newalloc * sizeof *list->fds);
		if (newfds == NULL)
			return 1;
		newconns = realloc(list->conns, newalloc * sizeof *list->conns);
		if (newconns == NULL)
			return 1;
		list->alloc = newalloc;
		list->fds = newfds;
		list->conns = newconns;
	}
	memcpy(list->fds + list->len, fd, sizeof *fd);
	memcpy(list->conns + list->len, conn, sizeof *conn);
	++list->len;
	return 0;
}

static void removeConnList(ConnList *list, int ind) {
	const int replace = list->len - 1;

	memcpy(list->fds + ind, list->fds + replace, sizeof *list->fds);
	memcpy(list->conns + ind, list->conns + replace, sizeof *list->conns);

	--list->len;
}

static void pollConnList(ConnList *list) {
	poll(list->fds, list->len, -1);
}

static void freeConnList(ConnList *list) {
	int i;
	for (i = 0; i < list->len; ++i)
		freeConnection(list->conns + i);
	free(list->fds);
	free(list->conns);
}
