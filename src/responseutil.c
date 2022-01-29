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
#include <stdarg.h>
#include <string.h>
#include <limits.h>

#include <sys/stat.h>

#include <responseutil.h>

#define CONST_FIELDS "Server: swebs/0.1\r\n"

static int sendConnection(Connection *conn, char *format, ...) {
	va_list ap;
	va_start(ap, format);
	int len = vsnprintf(NULL, 0, format, ap);
	char *data = malloc(len + 1);
	if (data == NULL)
		return 1;

	va_end(ap);
	va_start(ap, format);

	vsprintf(data, format, ap);
	if (sendStream(conn->stream, data, len) < len) {
		free(data);
		return 1;
	}
	free(data);
	return 0;
}

int sendStringResponse(Connection *conn, char *status, char *str) {
	return sendConnection(conn,
		"HTTP/1.1 %s\r\n"
		CONST_FIELDS
		"Content-Length: %lu\r\n"
		"\r\n"
		"%s"
		, status, strlen(str), str
	);
}

int sendErrorResponse(Connection *conn, char *error) {
	const char *template =
		"<meta charset=utf-8>"
		"<h1 text-align=center>"
		  "%s"
		"</h1>";
	int len = snprintf(NULL, 0, template, error);
	char *response = malloc(len + 1);
	sprintf(response, template, error);
	int ret = sendStringResponse(conn, error, response);
	free(response);
	return ret;
}

int sendBinaryResponse(Connection *conn, char *status,
		void *data, size_t len) {
	if (sendConnection(conn,
		"HTTP/1.1 %s\r\n"
		CONST_FIELDS
		"Content-Length: %lu\r\n"
		"\r\n"
		, status, len)
	)
		return 1;
	return sendStream(conn->stream, data, len) < len;
}
