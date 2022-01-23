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
#ifndef _HAVE_UTIL
#define _HAVE_UTIL
typedef enum {
	GET,
	POST,
	PUT,
	HEAD,
	DELETE,
	PATCH,
	OPTIONS,
	INVALID,
	//this indicates an invalid type of request, there is no request called
	//INVALID in HTTP/1.1.
} RequestType;

RequestType getType(char *str);
#endif
