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
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

#include <responses.h>
#include <responseutil.h>

static void readResponse(Connection *conn, char *path) {
	FILE *file;
	struct stat statbuf;
	if (stat(path, &statbuf)) {
		sendErrorResponse(conn, ERROR_404);
		return;
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
				return;
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

		file = fopen(requestPath, "r");
		free(assembledPath);
	}
	else
		file = fopen(path, "r");
	if (file == NULL)
		goto forbidden;
	fseek(file, 0, SEEK_END);
	long len = ftell(file);
	char *data = malloc(len);
	if (data == NULL)
		return;
	fseek(file, 0, SEEK_SET);
	if (fread(data, 1, len, file) < len) {
		fclose(file);
		goto error;
	}
	fclose(file);
	if (sendBinaryResponse(conn, "200 OK", data, len) < len)
		goto error;
	free(data);
	return;
error:
	sendErrorResponse(conn, ERROR_500);
	return;
forbidden:
	sendErrorResponse(conn, ERROR_403);
	return;
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
		if (istrcmp(site->content[i].host, host))
			continue;
		if (regexec(&site->content[i].path, conn->path, 0, NULL, 0)
		    == 0) {
			switch (site->content[i].command) {
				case READ:
					readResponse(conn, site->content[i].arg);
					goto end;
				default:
					return 1;
			}
		}
	}
	sendErrorResponse(conn, ERROR_404);
end:
	resetConnection(conn);
	return 0;
}
