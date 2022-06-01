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
#ifndef HAVE_UTIL
#define HAVE_UTIL

#include <swebs/types.h>

int initLogging(char *path);

int smalloc(size_t size);
/* returns an id passed to saddr, or -1 on error */
void *saddr(int id);
void sfree(void *addr);
void sdestroy(int id);

int createLog(char *msg);
int createErrorLog(char *msg, int err);
int istrcmp(char *s1, char *s2);
/* case insensitive strcmp */
RequestType getType(char *str);

void sendFd(int fd, int dest);
int recvFd(int source);

int createTmpName(char *path);
/* WIll set the 5 characters at the end of path to random data so that that
 * file does not exist. For example:
 *
 * char *path = "/some/path/to/some/dataXXXXX";
 * createTmpName(path);
 * puts(path);
 *
 * could output
 *
 * /some/path/to/some/data12ab6
 *
 * Returns non-zero on error, uses rand()
 * */
#endif
