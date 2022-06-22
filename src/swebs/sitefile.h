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
#ifndef HAVE_SITEFILE
#define HAVE_SITEFILE
#include <regex.h>

#include <swebs/types.h>
#include <swebs/config.h>

typedef enum {
	READ,
	THROW,
	LINKED
} Command;

typedef struct {
	SocketType type;
	unsigned short num;
	int timeout;
	char *key;
	char *cert;
	/* key and cert are possible unused */
} Port;

typedef struct {
	RequestType respondto;
	regex_t host;
	Command command;
	regex_t path;
	char *arg;
	unsigned short port;
} SiteCommand;

typedef struct {
	size_t size;
	size_t alloc;
	SiteCommand *content;

	size_t portcount;
	size_t portalloc;
	Port *ports;

#if DYNAMIC_LINKED_PAGES
	int (*getResponse)(Request *, Response *);
#endif
} Sitefile;

Sitefile *parseSitefile(char *path);
void freeSitefile(Sitefile *site);
#endif
