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

#include <swebs/util.h>
#include <swebs/responseutil.h>

#define CONST_FIELDS "Server: swebs/0.1\r\n"

static int resilientSend(Stream *stream, void *data, size_t len) {
	char *buffer = (char *) data;
	size_t left = len;

	while (left) {
		ssize_t sent;

		sent = sendStream(stream, buffer, left);

		if (sent < 0)
			return 1;
		if (sent == 0)
			break;
		buffer += sent;
		left -= sent;
	}
	return left != 0;
}

static int sendString(Stream *stream, char *s) {
	return resilientSend(stream, (void *) s, strlen(s));
}

static int sendStreamValist(Stream *stream, char *format, ...) {
	va_list ap;
	int len;
	char *data;
	va_start(ap, format);
	len = vsnprintf(NULL, 0, format, ap);
	data = malloc(len + 1);
	if (data == NULL)
		return 1;

	va_end(ap);
	va_start(ap, format);

	vsprintf(data, format, ap);
	if (resilientSend(stream, data, len)) {
		free(data);
		return 1;
	}
	free(data);
	return 0;
}

static int sendHeaderValist(Stream *stream, const char *status, va_list ap) {
	if (sendStreamValist(stream,
		"HTTP/1.1 %s\r\n"
		CONST_FIELDS, status))
		return 1;
	for (;;) {
		char *header;
		header = va_arg(ap, char *);
		if (header == NULL)
			break;
		if (resilientSend(stream, header, strlen(header)))
			return 1;
	}
	va_end(ap);
	return 0;
}

static int sendHeaderKnown(Stream *stream, const char *status, size_t len, va_list ap) {
	if (sendHeaderValist(stream, status, ap))
		return 1;
	return sendStreamValist(stream, "Content-Length: %lu\r\n\r\n", len);
}

static int sendHeaderChunked(Stream *stream, const char *status, va_list ap) {
	if (sendHeaderValist(stream, status, ap))
		return 1;
	return sendString(stream, "Transfer-Encoding: chunked\r\n\r\n");
}

char *getCode(int code) {
	switch (code) {
		case 200:
			return CODE_200;
		case 400:
			return ERROR_400;
		case 403:
			return ERROR_403;
		case 404:
			return ERROR_404;
		case 500:
			return ERROR_500;
		default:
			return NULL;
	}
}

int sendStringResponse(Stream *stream, const char *status, char *str, ...) {
	va_list ap;
	size_t len;
	va_start(ap, str);
	len = strlen(str);
	if (sendHeaderKnown(stream, status, len, ap))
		return 1;
	return resilientSend(stream, str, len);
}

int sendErrorResponse(Stream *stream, const char *error) {
	const char *template =
		"<meta charset=utf-8>"
		"<h1 text-align=center>"
		  "%s"
		"</h1>";
	int ret;
	int len = snprintf(NULL, 0, template, error);
	char *response = malloc(len + 1);
	if (response == NULL)
		return 1;
	sprintf(response, template, error);
	ret = sendStringResponse(stream, error, response,
			"Content-Type: text/html\r\n", NULL);
	free(response);
	return ret;
}

static int sendBinaryResponseValist(Stream *stream, const char *status,
		void *data, size_t len, va_list ap) {
	if (sendHeaderKnown(stream, status, len, ap))
		return 1;
	return resilientSend(stream, data, len);
}

static int sendKnownPipeValist(Stream *stream, const char *status,
		int fd, size_t len, va_list ap) {
	size_t totalSent = 0;
	int result;
	sendHeaderKnown(stream, status, len, ap);
	for (;;) {
		char buffer[1024];
		ssize_t inBuffer = read(fd, buffer, sizeof(buffer));
		if (inBuffer < 0) {
			result = 1;
			goto end;
		}
		if (inBuffer == 0) {
			result = totalSent != len;
			goto end;
		}
		if (resilientSend(stream, buffer, inBuffer)) {
			result = 1;
			goto end;
		}
		totalSent += inBuffer;
	}
end:
	close(fd);
	return result;
}

int sendKnownPipe(Stream *stream, const char *status, int fd, size_t len, ...) {
	va_list ap;
	va_start(ap, len);
	return sendKnownPipeValist(stream, status, fd, len, ap);
}

int sendBinaryResponse(Stream *stream, const char *status,
		void *data, size_t len, ...) {
	va_list ap;
	va_start(ap, len);
	return sendBinaryResponseValist(stream, status, data, len, ap);
}

int sendSeekableFile(Stream *stream, const char *status, int fd, ...) {
	off_t len;
	va_list ap;
	va_start(ap, fd);
	len = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	va_start(ap, fd);
	return sendKnownPipeValist(stream, status, fd, len, ap);
}

int sendPipe(Stream *stream, const char *status, int fd, ...) {
	va_list ap;
	va_start(ap, fd);
	if (sendHeaderChunked(stream, status, ap)) {
		close(fd);
		return 1;
	}

	for (;;) {
		ssize_t len;
		char buff[1024];

		len = read(fd, buff, sizeof buff);
		if (len <= 0) {
			break;
		}
		sendStreamValist(stream, "%lx\r\n", len);
		resilientSend(stream, buff, (size_t) len);
		sendString(stream, "\r\n");
	}
	close(fd);
	return 0;
}
