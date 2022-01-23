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
#ifndef _HAVE_CONNECTIONS
#define _HAVE_CONNECTIONS
#include <runner.h>
#include <sitefile.h>

typedef enum {
	RECEIVE_REQUEST,
	RECEIVE_HEADER,
	RECEIVE_BODY,
	SEND_RESPONSE,
} ConnectionSteps;

typedef struct {
	char *field;
	char *value;
} Field;

typedef struct Connection {
	int fd;
	ConnectionSteps progress;

	RequestType type;
	char *path;
	//ephemeral

	Field *fields;
	//pointer to array of 2 pointers, persistent
	size_t fieldCount;
	size_t allocatedFields;

	char *body;
	//ephemeral
	size_t bodylen;
	size_t receivedBody;

	struct Connection *next;

	char *currLine;
	//persistent
	size_t currLineAlloc;
	size_t currLineLen;
} Connection;
//The 2 types of fields:
//Persistent fields: Things which aren't freed after a reset, currLine, fields
//Ephemeral fields: Things which are freed and reallocated after each new
//request, path, body

Connection *newConnection(int fd);
//returns non-zero on error. creates a new connection bound to fd
void resetConnection(Connection *conn);
void freeConnection(Connection *conn);
int updateConnection(Connection *conn, Sitefile *site);
//returns non-zero on error.
//Generating a new connection and repeatedly calling updateConnection will
//handle everything.
#endif
