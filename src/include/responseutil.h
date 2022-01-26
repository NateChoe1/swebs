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
#ifndef _HAVE_RESPONSE_UTIL
#define _HAVE_RESPONSE_UTIL
#include <connections.h>

#define ERROR_403 "403 Forbidden"
#define ERROR_404 "404 Not Found"
#define ERROR_500 "500 Internal Server Error"

int sendStringResponse(Connection *conn, char *status, char *str);
int sendBinaryResponse(Connection *conn, char *status, void *data, size_t len);
int sendErrorResponse(Connection *conn, char *error);
//sendErrorResponse(conn, "404 Not found");
#endif
