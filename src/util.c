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
#include <string.h>
#include <stdint.h>

#include <util.h>

RequestType getType(char *str) {
	if (strlen(str) >= 8)
		return INVALID;
	uint64_t type = 0;
	for (int i = 0; str[i]; i++) {
		type <<= 8;
		type |= str[i];
	}
	switch (type) {
		case 0x474554l:
			return GET;
		case 0x504f5354l:
			return POST;
		case 0x505554l:
			return PUT;
		case 0x48454144l:
			return HEAD;
		case 0x44454c455445l:
			return DELETE;
		case 0x5041544348l:
			return PATCH;
		case 0x4f5054494f4e53l:
			return OPTIONS;
		default:
			return INVALID;
	}
	//This would actually be far nicer in HolyC of all languages. I feel
	//like the context immediately following each magic number is enough.
}
