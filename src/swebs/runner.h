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
#ifndef HAVE_RUNNER
#define HAVE_RUNNER
#include <sys/socket.h>

#include <swebs/sockets.h>
#include <swebs/sitefile.h>
#include <swebs/connections.h>

typedef struct {
	int valid;
	int portind;
} ConnInfo;

void runServer(int connfd, Sitefile *site, int *pending, int id,
		volatile ConnInfo *info);
/* pending and info are shared memory. pending[id] is the amount of connections
 * that are being processed by that process, and info contains info about the
 * connection being sent through. */
#endif
