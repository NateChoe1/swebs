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
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>

#include <runner.h>
#include <sitefile.h>
#include <connections.h>

void *runServer(RunnerArgs *args) {
	Sitefile *site = args->site;
	int *pending = args->pending;
	int *schedule = args->schedule;
	int id = args->id;

	Connection *connections = NULL;
	Connection *last = NULL;
	//Connections are processed in a queue, which is really just a linked
	//list where we add to the end and read from the beginning.
	for (;;) {
		if (schedule[0] == id) {
			Connection *newconn = newConnection(schedule[1]);
			assert(newconn != NULL);

			if (last == NULL)
				connections = newconn;
			else
				last->next = newconn;
			last = newconn;
			pending[id]++;
			schedule[0] = -1;
		}

		Connection *prev = NULL;
		Connection *iter = connections;
		//I know of the Linus Thorvalds good taste code thing, it just
		//gets very confusing very fast to think about pointers to
		//pointers which have pointers.
		while (iter != NULL) {
			if (updateConnection(iter, site)) {
				if (iter == last)
					last = prev;
				Connection *old = iter;
				iter = iter->next;
				freeConnection(old);
				if (prev == NULL)
					connections = iter;
				else
					prev->next = iter;
				pending[id]--;
			}
			else {
				prev = iter;
				iter = iter->next;
			}
		}
	}
	return NULL;
}
