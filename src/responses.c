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
#include <string.h>
#include <unistd.h>

#include <responses.h>

static int sendConn(int fd, char *str) {
	size_t len = strlen(str);
	if (write(fd, str, len))
		return 1;
	return 0;
}

int sendResponse(Connection *conn, Sitefile *site) {
	printf("test %d\n", site->size);
	for (int i = 0; i < site->size; i++) {
		printf("%s %s\n", site->content[i].path, site->content[i].arg);
	}

	sendConn(conn->fd, "HTTP/1.1 200 OK\r\n");
	sendConn(conn->fd, "Content-Type: text/html\r\n");
	sendConn(conn->fd, "Content-Length: 16\r\n");
	sendConn(conn->fd, "\r\n");
	sendConn(conn->fd, "<p>Hi there!</p>");
	resetConnection(conn);
	return 0;
}
