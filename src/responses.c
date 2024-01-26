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

#include <features.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <swebs/responses.h>
#include <swebs/responseutil.h>

static const char *contenttemplate = "Content-Type: %s\r\n";

static int readResponse(Connection *conn, SiteCommand *command) {
	int fd = -1;
	struct stat statbuf;
	char *path;
	path = command->arg;
	if (stat(path, &statbuf)) {
		sendErrorResponse(conn->stream, ERROR_404);
		return 1;
	}
	if (S_ISDIR(statbuf.st_mode)) {
		size_t reqPathLen = conn->path.len;
		size_t pathLen = strlen(path);
		char *assembledPath = malloc(reqPathLen + pathLen + 1);
		char requestPath[PATH_MAX], responsePath[PATH_MAX];
		size_t responsePathLen;
		struct stat requestBuff;
		if (assembledPath == NULL)
			goto error;
		memcpy(assembledPath, path, pathLen);
		memcpy(assembledPath + pathLen,
				conn->path.data, reqPathLen + 1);

		if (realpath(assembledPath, requestPath) == NULL) {
			if (errno == ENOENT) {
				free(assembledPath);
				sendErrorResponse(conn->stream, ERROR_404);
				return 1;
			}
			free(assembledPath);
			goto error;
		}

		if (realpath(path, responsePath) == NULL) {
			free(assembledPath);
			goto error;
		}
		responsePathLen = strlen(responsePath);
		if (memcmp(requestPath, responsePath, responsePathLen)) {
			free(assembledPath);
			goto forbidden;
		}
		/*
		 * in theory an attacker could just request
		 *  /blog/../../../../site/privatekey.pem
		 * so we make sure that the filepath is actually within the path
		 * specified by the page.
		 * */

		if (stat(requestPath, &requestBuff)) {
			free(assembledPath);
			sendErrorResponse(conn->stream, ERROR_404);
			return 1;
		}
		if (S_ISDIR(requestBuff.st_mode)) {
			free(assembledPath);
			sendErrorResponse(conn->stream, ERROR_400);
			return 1;
		}

		fd = open(requestPath, O_RDONLY);
		free(assembledPath);
	}
	else
		fd = open(path, O_RDONLY);
	if (fd < 0)
		goto forbidden;
	{
		int ret;
		char *contenthead, *contenttype;
		contenttype = command->contenttype;
		contenthead = malloc(snprintf(NULL, 0, contenttemplate, contenttype) + 1);
		if (contenthead == NULL)
			return 1;
		sprintf(contenthead, contenttemplate, contenttype);
		ret = sendSeekableFile(conn->stream, CODE_200, fd, contenthead, NULL);
		free(contenthead);
		return ret;
	}
error:
	sendErrorResponse(conn->stream, ERROR_500);
	return 1;
forbidden:
	sendErrorResponse(conn->stream, ERROR_403);
	return 1;
}

static int linkedResponse(Connection *conn,
		int (*getResponse)(Request *request, Response *response),
		char *contenttype) {
	Request request;
	Response response;
	int code;
	int ret;
	char *header;

	header = malloc(snprintf(NULL, 0, contenttemplate, contenttype) + 1);
	if (header == NULL)
		return sendErrorResponse(conn->stream, ERROR_500);
	sprintf(header, contenttemplate, contenttype);

	request.fieldCount = conn->fieldCount;
	request.fields = conn->fields;
	request.path.path = conn->path;
	request.path.fieldCount = conn->pathFieldCount;
	request.path.fields = conn->pathFields;
	request.type = conn->type;
	request.body = conn->body;
	request.bodylen = conn->bodylen;

	code = getResponse(&request, &response);

	ret = 1;

	switch (response.type) {
		case FILE_KNOWN_LENGTH:
			ret =  sendKnownPipe(conn->stream, getCode(code),
					response.response.file.fd,
					response.response.file.len,
					header, NULL);
			break;
		case FILE_UNKNOWN_LENGTH:
			ret = sendPipe(conn->stream, getCode(code),
					response.response.file.fd,
					header, NULL);
			break;
		case BUFFER: case BUFFER_NOFREE:
			ret = sendBinaryResponse(conn->stream, getCode(code),
					response.response.buffer.data,
					response.response.buffer.len,
					header, NULL);
			if (response.type == BUFFER)
				free(response.response.buffer.data);
			break;
		case DEFAULT:
			ret = sendErrorResponse(conn->stream, getCode(code));
			break;
	}
	free(header);
	return ret;
}

static int fullmatch(regex_t *regex, char *str) {
	regmatch_t match;
	if (regexec(regex, str, 1, &match, 0))
		return 1;
	return match.rm_so != 0 || match.rm_eo != strlen(str);
}

static char *nextdirective(char *header) {
	char *loc;
	loc = strstr(header, "; ");
	if (loc == NULL)
		return NULL;
	return loc + 2;
}

static char *gettype(char *request, char **type) {
	char *typeret;
	char *ret;
	{
		char *next;
		next = strchr(request, ',');
		if (next == NULL) {
			typeret = strdup(request);
			if (typeret == NULL) {
				*type = NULL;
				return NULL;
			}
			ret = NULL;
		}
		else {
			size_t biglen;
			biglen = next - request;
			typeret = malloc(biglen + 1);
			if (typeret == NULL) {
				*type = NULL;
				return NULL;
			}
			memcpy(typeret, request, biglen);
			typeret[biglen] = '\0';
			ret = next + 1;
		}
	}
	{
		char *set;
		set = strchr(typeret, ';');
		if (set != NULL)
			set[0] = '\0';
	}
	*type = typeret;
	return ret;
}

static int ismatch(char *request, char *type) {
/* Matches a single MIME type. Note that * /html is valid for request. */
	int i;
	if (request[0] == '\0' || type[0] == '\0')
		return request[0] != type[0];

	if (request[0] == '*' && (request[1] == '/' || request[1] == '\0')) {
		char *nexttype;
		nexttype = type;
		while (nexttype[0] != '/' && nexttype[0] != '\0')
			++nexttype;
		if (request[1] != nexttype[0])
			return 0;
		if (nexttype[0] == '\0')
			return 1;
		return ismatch(request + 2, nexttype + 1);
	}

	for (i = 0; request[i] == type[i] &&
			request[i] != '/' && request[i] != '\0'; ++i) ;
	if (request[i] != type[i])
		return 0;
	if (request[i] == '\0')
		return 1;
	return ismatch(request + i + 1, type + i + 1);
}

static int wasasked(char *request, char *type) {
/* request is the Accept header field and type is the type of the page. */
	char *mimetype; /* the actual mime type*/
	char *checkloc;
	{
		char *typeptr;
		mimetype = NULL;
		typeptr = type;
		while (mimetype == NULL) {
			char *next;
			if (typeptr == NULL)
				return 0;
			next = nextdirective(typeptr);
			if (strncmp(typeptr, "charset=", 8) == 0) {
				typeptr = next;
				continue;
			}
			if (strncmp(typeptr, "boundary=", 9) == 0) {
				typeptr = next;
				continue;
			}
			if (next == NULL) {
				mimetype = strdup(typeptr);
				if (mimetype == NULL)
					return 0;
			}
			else {
				size_t mimelen;
				mimelen = next - typeptr;
				mimetype = malloc(mimelen + 1);
				if (mimetype == NULL)
					return 0;
				memcpy(mimetype, typeptr, mimelen);
				mimetype[mimelen] = '\0';
			}
		}
	}

	checkloc = request;
	while (checkloc != NULL) {
		char *check;
		checkloc = gettype(checkloc, &check);
		if (check == NULL)
			return 0;
		if (ismatch(check, mimetype)) {
			free(check);
			return 1;
		}
		free(check);
	}
	return 0;
}

static int sendCertainResponse(Connection *conn, Sitefile *site, int index) {
	int ret;
	ret = 0;
	switch (site->content[index].command) {
	case READ:
		ret = readResponse(conn, site->content + index);
		break;
	case THROW:
		ret = sendErrorResponse(conn->stream, site->content[index].arg);
		break;
	case LINKED:
#if DYNAMIC_LINKED_PAGES
		if (!site->getResponse) {
			sendErrorResponse(conn->stream, ERROR_500);
			ret = 1;
		}
		else
			ret = linkedResponse(conn, site->getResponse,
					site->content[index].contenttype);
#else
		/* Unreachable state (if a linked response was in the sitefile,
		 * the parse would've thrown an error) */
		ret = sendErrorResponse(conn->stream, ERROR_500);
#endif
		break;
	default:
		sendErrorResponse(conn->stream, ERROR_500);
		return 1;
	}
	resetConnection(conn);
	return ret;
}

int sendResponse(Connection *conn, Sitefile *site) {
	char *host = NULL;
	char *accept = "*/*";
	int i;
	for (i = 0; i < conn->fieldCount; i++) {
		if (strcmp(conn->fields[i].field, "Host") == 0)
			host = conn->fields[i].value;
		else if (strcmp(conn->fields[i].field, "Accept") == 0)
			accept = conn->fields[i].value;
	}
	if (host == NULL) {
		sendErrorResponse(conn->stream, ERROR_400);
		return 1;
	}
	for (i = 0; i < site->size; i++) {
		if (site->content[i].respondto != conn->type)
			continue;
		if (fullmatch(&site->content[i].host, host))
			continue;
		if (!wasasked(accept, site->content[i].contenttype))
			continue;
		{
			int j;
			const unsigned short currport = site->ports[conn->portind].num;
			for (j = 0; j < site->content[i].portcount; ++j)
				if (site->content[i].ports[j] == currport)
					goto foundport;
			continue;
		}
foundport:
		if (fullmatch(&site->content[i].path, conn->path.data) == 0)
			return sendCertainResponse(conn, site, i);
	}
	sendErrorResponse(conn->stream, ERROR_404);
	return 1;
}
