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
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

#include <responses.h>
#include <responseutil.h>

static int readResponse(Connection *conn, char *path) {
	int fd = -1;;
	struct stat statbuf;
	if (stat(path, &statbuf)) {
		sendErrorResponse(conn, ERROR_404);
		return -1;
	}
	if (S_ISDIR(statbuf.st_mode)) {
		long reqPathLen = strlen(conn->path);
		long pathLen = strlen(path);
		char *assembledPath = malloc(reqPathLen + pathLen + 1);
		if (assembledPath == NULL)
			goto error;
		memcpy(assembledPath, path, pathLen);
		memcpy(assembledPath + pathLen, conn->path, reqPathLen + 1);

		char requestPath[PATH_MAX];
		if (realpath(assembledPath, requestPath) == NULL) {
			if (errno == ENOENT) {
				free(assembledPath);
				sendErrorResponse(conn, ERROR_404);
				return -1;
			}
			free(assembledPath);
			goto error;
		}

		char responsePath[PATH_MAX];
		if (realpath(path, responsePath) == NULL) {
			free(assembledPath);
			goto error;
		}
		size_t responsePathLen = strlen(responsePath);
		if (memcmp(requestPath, responsePath, responsePathLen)) {
			free(assembledPath);
			goto forbidden;
		}
		//in theory an attacker could just request
		// /blog/../../../../site/privatekey.pem
		//so we make sure that the filepath is actually within the path
		//specified by the page.

		struct stat requestbuf;
		if (stat(requestPath, &requestbuf)) {
			free(assembledPath);
			sendErrorResponse(conn, ERROR_404);
			return -1;
		}
		if (S_ISDIR(requestbuf.st_mode)) {
			free(assembledPath);
			sendErrorResponse(conn, ERROR_400);
			return -1;
		}

		fd = open(requestPath, O_RDONLY);
		free(assembledPath);
	}
	else
		fd = open(path, O_RDONLY);
	if (fd < 0)
		goto forbidden;
	off_t len = lseek(fd, 0, SEEK_END);
	if (len < 0)
		goto error;
	lseek(fd, 0, SEEK_SET);
	sendHeader(conn, CODE_200, len);
	conn->len = len;
	return fd;
error:
	sendErrorResponse(conn, ERROR_500);
	return -1;
forbidden:
	sendErrorResponse(conn, ERROR_403);
	return -1;
}

static int fullmatch(regex_t *regex, char *str) {
	regmatch_t match;
	if (regexec(regex, str, 1, &match, 0))
		return 1;
	return match.rm_so != 0 || match.rm_eo != strlen(str);
}

int getResponse(Connection *conn, Sitefile *site) {
	char *host = NULL;
	for (int i = 0; i < conn->fieldCount; i++) {
		if (strcmp(conn->fields[i].field, "Host") == 0) {
			host = conn->fields[i].value;
			break;
		}
	}
	if (host == NULL) {
		sendErrorResponse(conn, ERROR_400);
		return 1;
	}
	for (int i = 0; i < site->size; i++) {
		if (site->content[i].respondto != conn->type)
			continue;
		if (fullmatch(&site->content[i].host, host))
			continue;
		if (fullmatch(&site->content[i].path, conn->path) == 0) {
			int fd = -1;
			switch (site->content[i].command) {
				case READ:
					fd = readResponse(conn,
						site->content[i].arg);
					break;
				default:
					sendErrorResponse(conn, ERROR_500);
					return 1;
			}
			if (fd == -1)
				return 1;
			conn->fd = fd;
			conn->progress = SEND_RESPONSE;
			return 0;
		}
	}
	sendErrorResponse(conn, ERROR_404);
	return -1;
}
