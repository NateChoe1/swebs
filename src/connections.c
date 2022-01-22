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
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>

#include <util.h>
#include <runner.h>
#include <sitefile.h>
#include <responses.h>
#include <connections.h>

int newConnection(Connection *ret, int fd) {
	ret->fd = fd;
	if (fcntl(ret->fd, F_SETFL, O_NONBLOCK))
		return 1;
	ret->progress = RECEIVE_REQUEST;

	ret->currLineAlloc = 30;
	ret->currLineLen = 0;
	ret->currLine = malloc(ret->currLineAlloc);
	if (ret->currLine == NULL)
		return 1;

	ret->allocatedFields = 10;
	ret->fields = malloc(sizeof(char *[2]) * ret->allocatedFields);
	if (ret->fields == NULL)
		return 1;

	ret->path = NULL;
	ret->body = NULL;
	//pointers to things that are allocated within functions should be
	//initialized to NULL so that free() doens't fail.

	ret->next = NULL;
	return 0;
}

void resetConnection(Connection *conn) {
	conn->type = RECEIVE_REQUEST;
	conn->fieldCount = 0;
	free(conn->body);
	free(conn->path);
	conn->body = NULL;
	conn->path = NULL;
}

void freeConnection(Connection *conn) {
	shutdown(conn->fd, SHUT_RDWR);
	free(conn->currLine);
	free(conn->path);
	free(conn->fields);
	free(conn->body);
	free(conn);
}

static int processRequest(Connection *conn) {
	char *line = conn->currLine;
	for (int i = 0;; i++) {
		if (line[i] == ' ') {
			line[i] = '\0';
			conn->type = getType(line);
			line += i + 1;
			break;
		}
		if (line[i] == '\0') {
			return 1;
		}
	}
	for (int i = 0;; i++) {
		if (line[i] == ' ') {
			line[i] = '\0';
			conn->path = malloc(i + 1);
			if (conn->path == NULL)
				return 1;
			memcpy(conn->path, line, i + 1);
			line += i + 1;
			break;
		}
		if (line[i] == '\0') {
			return 1;
		}
	}
	if (strcmp(line, "HTTP/1.1"))
		return 1;
	conn->progress = RECEIVE_HEADER;
	conn->fieldCount = 0;

	return 0;
}

static int processField(Connection *conn) {
	if (conn->currLineLen == 0) {
		conn->progress = RECEIVE_BODY;
		for (size_t i = 0; i < conn->fieldCount; i++) {
			if (strcmp(conn->fields[i][0], "Content-Length") == 0)
				conn->bodylen = atol(conn->fields[i][1]);
		}
		conn->body = malloc(conn->bodylen + 1);
		if (conn->body == NULL)
			return 1;
		conn->receivedBody = 0;
		return 0;
	}

	if (conn->fieldCount >= conn->allocatedFields) {
		conn->allocatedFields *= 2;
		char *(*newfields)[2] = realloc(conn->fields, conn->allocatedFields *
		                                sizeof(char *[2]));
		if (newfields == NULL)
			return 1;
		conn->fields = newfields;
	}

	char *line = conn->currLine;
	char *split = strstr(line, ": ");
	size_t linelen = conn->currLineLen;
	if (split == NULL)
		return 1;

	char *header = malloc(split - line + 1);
	memcpy(header, line, split - line);
	header[split - line] = '\0';

	linelen -= split - line + 2;
	line += split - line + 2;
	char *value = malloc(linelen + 1);
	memcpy(value, line, linelen + 1);

	conn->fields[conn->fieldCount][0] = header;
	conn->fields[conn->fieldCount][1] = value;

	conn->fieldCount++;

	return 0;
}

static int processChar(Connection *conn, char c, Sitefile *site) {
	if (conn->progress < RECEIVE_BODY) {
		if (conn->currLineLen >= conn->currLineAlloc) {
			conn->currLineAlloc *= 2;
			char *newline = realloc(conn->currLine,
			                        conn->currLineAlloc);
			assert(newline != NULL);
			conn->currLine = newline;
		}
		conn->currLine[conn->currLineLen++] = c;
		if (c == '\n') {
			if (--conn->currLineLen <= 0)
				return 1;
			if (conn->currLine[--conn->currLineLen] != '\r')
				return 1;
			conn->currLine[conn->currLineLen] = '\0';
			if (conn->progress == RECEIVE_REQUEST) {
				if (processRequest(conn))
					return 1;
			}
			else if (conn->progress == RECEIVE_HEADER) {
				if (processField(conn))
					return 1;
			}
			conn->currLineLen = 0;
		}
	}
	else if (conn->progress == RECEIVE_BODY) {
		if (conn->receivedBody < conn->bodylen)
			conn->body[conn->receivedBody++] = c;
	}
	if (conn->receivedBody >= conn->bodylen) {
		sendResponse(conn, site);
	}
	return 0;
}

int updateConnection(Connection *conn, Sitefile *site) {
	char buff[300];
	for (;;) {
		ssize_t received = read(conn->fd, buff, sizeof(buff));
		if (received < 0)
			return 1;
		if (received == 0)
			break;
		for (unsigned long i = 0; i < received; i++) {
			if (processChar(conn, buff[i], site))
				return 1;
		}
	}
	return 0;
}
