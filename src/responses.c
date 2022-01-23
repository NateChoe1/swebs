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
#include <unistd.h>

#include <responses.h>

static int sendConnection(Connection *conn, char *format, ...) {
	va_list ap;
	va_start(ap, format);
	int len = vsnprintf(NULL, 0, format, ap);
	char *data = malloc(len + 1);
	if (data == NULL)
		return 1;

	va_end(ap);
	va_start(ap, format);

	vsnprintf(data, len + 1, format, ap);
	write(conn->fd, data, len);
	free(data);
	return 0;
}

static void readResponse(Connection *conn, char *path) {
	FILE *file = fopen(path, "r");
	if (file == NULL) {
		sendConnection(conn,
			"HTTP/1.1 403 Forbidden\r\n"
			"Content-Type: text/html; charset=UTF-8\r\n"
			"Server: swebs/0.1\r\n"
			"Content-Length: 21\r\n"
			"\r\n"
			"<h1>Invalid page</h1>\n"
		);
		return;
	}
	fseek(file, 0, SEEK_END);
	long len = ftell(file);
	char *data = malloc(len);
	if (data == NULL)
		return;
	fseek(file, 0, SEEK_SET);
	fread(data, 1, len, file);
	fclose(file);
	sendConnection(conn,
		"HTTP/1.1 200 OK\r\n"
		"Server: swebs/0.1\r\n"
		"Content-Length: %ld\r\n"
		"\r\n", len
	);
	write(conn->fd, data, len);
	free(data);
	fsync(conn->fd);
}

int sendResponse(Connection *conn, Sitefile *site) {
	if (conn->path == NULL)
		return 1;
	for (int i = 0; i < site->size; i++) {
		if (site->content[i].respondto != conn->type)
			continue;
		if (strcmp(conn->path, site->content[i].path) == 0) {
			switch (site->content[i].command) {
				case READ:
					readResponse(conn, site->content[i].arg);
					goto end;
				default:
					return 1;
			}
		}
	}
	sendConnection(conn,
		"HTTP/1.1 404 Not Found\r\n"
		"Content-Type: text/html; charset=UTF-8\r\n"
		"Server: swebs/0.1\r\n"
		"Content-Length: 24\r\n"
		"\r\n"
		"<h1>File not found</h1>\n"
	);
end:
	resetConnection(conn);
	return 0;
}
