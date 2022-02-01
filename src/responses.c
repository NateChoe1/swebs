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
#include <sys/wait.h>

#include <responses.h>
#include <responseutil.h>

static int resilientSend(Stream *stream, char *buff, size_t len) {
//Will either write len bytes, or return 1 for error.
	while (len > 0) {
		ssize_t sent = sendStream(stream, buff, len);
		if (sent < 0)
			return 1;
		len -= sent;
		buff += sent;
	}
	return 0;
}

static int readResponse(Connection *conn, char *path) {
	int fd = 1;
	struct stat statbuf;
	if (stat(path, &statbuf)) {
		sendErrorResponse(conn, ERROR_404);
		return 1;
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
				return 1;
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
			return 1;
		}
		if (S_ISDIR(requestbuf.st_mode)) {
			free(assembledPath);
			sendErrorResponse(conn, ERROR_400);
			return 1;
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
	if (lseek(fd, 0, SEEK_SET) < 0) {
		sendErrorResponse(conn, ERROR_500);
		close(fd);
		return 1;
	}
	sendHeader(conn, CODE_200, len);

	while (len > 0) {
		char buff[1024];
		size_t readLen = read(fd, buff, sizeof(buff));
		if (resilientSend(conn->stream, buff, readLen))
			goto error;
		len -= readLen;
	}

	return 0;
error:
	sendErrorResponse(conn, ERROR_500);
	return 1;
forbidden:
	sendErrorResponse(conn, ERROR_403);
	return 1;
}

static int execResponse(Connection *conn, char *path) {
	int output[2];
	if (pipe(output))
		goto error;

	pid_t pid = fork();
	if (pid < 0)
		goto error;
	if (pid == 0) {
		close(1);
		close(output[0]);
		dup2(output[1], 1);
		char **args = malloc((conn->fieldCount*2 + 2) * sizeof(char *));
		if (args == NULL) {
			close(output[1]);
			exit(EXIT_FAILURE);
		}
		args[0] = path;
		for (int i = 0; i < conn->fieldCount; i++) {
			args[i * 2 + 1] = conn->fields[i].field;
			args[i * 2 + 2] = conn->fields[i].value;
		}
		args[conn->fieldCount*2 + 1] = NULL;
		if (execv(path, args) < 0) {
			close(output[1]);
			free(args);
			exit(EXIT_FAILURE);
		}
		close(output[1]);
		free(args);
		exit(EXIT_SUCCESS);
	}
	size_t allocResponse = 1024;
	size_t responseLen = 0;
	char *response = malloc(allocResponse);
	close(output[1]);
	for (;;) {
		if (responseLen == allocResponse) {
			allocResponse *= 2;
			char *newresponse = realloc(response, allocResponse);
			if (newresponse == NULL)
				goto looperror;
			response = newresponse;
		}
		ssize_t len = read(output[0],
				response + responseLen,
				allocResponse - responseLen);
		if (len < 0)
			goto looperror;
		if (len == 0)
			break;
		responseLen += len;
		continue;
looperror:
		close(output[0]);
		int status;
		waitpid(pid, &status, 0);
		free(response);
		goto error;
	}
	close(output[0]);
	int status;
	waitpid(pid, &status, 0);
	sendHeader(conn, CODE_200, responseLen);
	if (resilientSend(conn->stream, response, responseLen)) {
		free(response);
		return 1;
	}
	free(response);
	return 0;
error:
	sendErrorResponse(conn, ERROR_500);
	return 1;
}

static int fullmatch(regex_t *regex, char *str) {
	regmatch_t match;
	if (regexec(regex, str, 1, &match, 0))
		return 1;
	return match.rm_so != 0 || match.rm_eo != strlen(str);
}

int sendResponse(Connection *conn, Sitefile *site) {
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
			switch (site->content[i].command) {
				case READ:
					readResponse(conn,
							site->content[i].arg);
					break;
				case EXEC:
					execResponse(conn,
							site->content[i].arg);
				default:
					sendErrorResponse(conn, ERROR_500);
					return 1;
			}
			resetConnection(conn);
			return 0;
		}
	}
	sendErrorResponse(conn, ERROR_404);
	return 1;
}
