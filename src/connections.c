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

int newConnection(Stream *stream, Connection *ret) {
	ret->stream = stream;
	ret->progress = RECEIVE_REQUEST;

	ret->currLineAlloc = 30;
	ret->currLineLen = 0;
	ret->currLine = malloc(ret->currLineAlloc);
	if (ret->currLine == NULL)
		return 1;

	ret->allocatedFields = 10;
	ret->fields = malloc(sizeof(Field) * ret->allocatedFields);
	if (ret->fields == NULL) {
		free(ret->currLine);
		return 1;
	}

	ret->path = NULL;
	ret->body = NULL;
	//pointers to things that are allocated within functions should be
	//initialized to NULL so that free() doens't fail.

	struct timespec currentTime;
	if (clock_gettime(CLOCK_MONOTONIC, &currentTime) < 0) {
		free(ret->currLine);
		return 1;
	}
	memcpy(&ret->lastdata, &currentTime, sizeof(struct timespec));
	return 0;
}

void resetConnection(Connection *conn) {
	conn->progress = RECEIVE_REQUEST;
	conn->fieldCount = 0;
	free(conn->body);
	free(conn->path);
	conn->body = NULL;
	conn->path = NULL;
}

void freeConnection(Connection *conn) {
	freeStream(conn->stream);
	free(conn->currLine);
	free(conn->path);
	free(conn->fields);
	free(conn->body);
}

static int processRequest(Connection *conn) {
	char *line = conn->currLine;
	for (int i = 0;; i++) {
		if (line[i] == ' ') {
			line[i] = '\0';
			conn->type = getType(line);
			if (conn->type == INVALID)
				return 1;
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
		conn->bodylen = 0;
		for (size_t i = 0; i < conn->fieldCount; i++) {
			if (strcmp(conn->fields[i].field,
			           "Content-Length") == 0)
				conn->bodylen = atol(conn->fields[i].value);
		}
		conn->body = malloc(conn->bodylen + 1);
		if (conn->body == NULL)
			return 1;
		conn->receivedBody = 0;
		return 0;
	}

	if (conn->fieldCount >= conn->allocatedFields) {
		conn->allocatedFields *= 2;
		Field *newfields = realloc(conn->fields, conn->allocatedFields *
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

	char *field = malloc(split - line + 1);
	memcpy(field, line, split - line);
	field[split - line] = '\0';

	linelen -= split - line + 2;
	line += split - line + 2;
	char *value = malloc(linelen + 1);
	memcpy(value, line, linelen + 1);

	conn->fields[conn->fieldCount].field = field;
	conn->fields[conn->fieldCount].value = value;

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
	if (conn->progress == RECEIVE_BODY &&
	    conn->receivedBody >= conn->bodylen)
		getResponse(conn, site);
	if (conn->progress == SEND_RESPONSE) {
		for (;;) {
			char buffer[1024];
			size_t readLen = read(conn->fd, buffer, sizeof(buffer));
			if (readLen == 0)
				break;
			if (readLen < 0)
				return 1;
			size_t writeLen = sendStream(conn->stream,
					buffer, readLen);
			if (writeLen < readLen)
				return 1;
		}
		resetConnection(conn);
	}
	return 0;
}

static long diff(struct timespec *t1, struct timespec *t2) {
	return (t2->tv_sec - t1->tv_sec) * 1000 +
		(t2->tv_nsec - t1->tv_nsec) / 1000000;
}

int updateConnection(Connection *conn, Sitefile *site) {
	char buff[300];
	for (;;) {
		struct timespec currentTime;
		if (clock_gettime(CLOCK_MONOTONIC, &currentTime) < 0)
			return 1;
		if (site->timeout > 0 &&
		    diff(&conn->lastdata, &currentTime) > site->timeout)
			return 1;
		ssize_t received = recvStream(conn->stream, buff, sizeof(buff));
		if (received < 0)
			return errno != EAGAIN;
		if (received == 0)
			return 0;
		memcpy(&conn->lastdata, &currentTime, sizeof(struct timespec));
		for (unsigned long i = 0; i < received; i++) {
			if (processChar(conn, buff[i], site))
				return 1;
		}
	}
}
