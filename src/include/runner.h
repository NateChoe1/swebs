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
#ifndef _HAVE_RUNNER
#define _HAVE_RUNNER
#include <sys/socket.h>

#include <sitefile.h>
#include <connections.h>

typedef struct {
	Sitefile *site;
	int *pending;
	//pending[thread id] = the number of connections being handled by that
	//   thread
	int notify;
	/*
	 * When this runner should accept a connection, notify will contain an
	 * int ready to be read.
	 */
	int id;
} RunnerArgs;
//my least favourite anti pattern

void *runServer(RunnerArgs *args);
#endif
