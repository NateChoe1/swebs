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

#define LEN(arr) (sizeof (arr) / sizeof (*arr))

int smalloc(size_t size);
/* returns an id passed to saddr, or -1 on error */
void *saddr(int id);
void sfree(void *addr);
void sdestroy(int id);

void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(char *str);
/* These functions should only be used during setup (reading sitefiles and such)
 * and not real runtime. */

int createLog(char *msg);
int createErrorLog(char *msg, int err);
int createFormatLog(char *fmt, ...);
int istrcmp(char *s1, char *s2);
/* case insensitive strcmp */
RequestType getType(char *str);

void sendFd(int fd, int dest, void *data, size_t len);
int recvFd(int source, void *data, size_t len);

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

void setsignal(int signal, void (*handler)(int));
void unsetsignal(int signal);

#endif
