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
#ifndef _HAVE_SITEFILE
#define _HAVE_SITEFILE
#include <regex.h>

#include <util.h>

typedef enum {
	READ,
	EXEC,
} Command;

typedef struct {
	RequestType respondto;
	regex_t host;
	Command command;
	regex_t path;
	char *arg;
} SiteCommand;

typedef struct {
	int size;
	SiteCommand *content;
	SocketType type;
	char *key;
	char *cert;
	int timeout;
} Sitefile;

Sitefile *parseSitefile(char *path);
void freeSitefile(Sitefile *site);
#endif
