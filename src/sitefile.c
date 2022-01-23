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
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <util.h>
#include <sitefile.h>

typedef enum {
	SUCCESS,
	LINE_END,
	FILE_END,
	ERROR,
} ReturnCode;
//this isn't ideal, but it's necessary to avoid namespace collisions.

static void freeTokens(int argc, char **argv) {
	for (int i = 0; i < argc; i++)
		free(argv[i]);
	free(argv);
}

static ReturnCode getToken(FILE *file, char **ret) {
	typedef enum {
		QUOTED,
		NONQUOTED,
	} TokenType;
	TokenType type;

	for (;;) {
		int c = fgetc(file);
		if (c == '\n')
			return LINE_END;
		if (c == EOF)
			return FILE_END;
		if (c == '#') {
			while (c != '\n')
				c = fgetc(file);
			return LINE_END;
		}
		if (!isspace(c)) {
			if (c == '"')
				type = QUOTED;
			else {
				type = NONQUOTED;
				ungetc(c, file);
			}
			break;
		}
	}

	long allocatedLen = 30;
	long len;
	*ret = malloc(allocatedLen);

	for (len = 0;; len++) {
		if (len >= allocatedLen) {
			allocatedLen *= 2;
			char *newret = realloc(*ret, allocatedLen);
			if (newret == NULL)
				goto error;
			*ret = newret;
		}
		int c = fgetc(file);
		switch (type) {
			case QUOTED:
				if (c == '"')
					goto gotToken;
				break;
			case NONQUOTED:
				if (isspace(c)) {
					ungetc(c, file);
					goto gotToken;
				}
				break;
		}
		switch (c) {
			case '\\':
				c = fgetc(file);
				if (c == EOF)
					goto error;
				break;
			case EOF:
				if (type == NONQUOTED)
					goto gotToken;
				goto error;
		}
		(*ret)[len] = c;
	}
gotToken:
	(*ret)[len] = '\0';
	return SUCCESS;
error:
	free(*ret);
	return ERROR;
}

static ReturnCode getCommand(FILE *file, int *argcret, char ***argvret) {
//THIS FUNCTION WILL NOT RETURN LINE_END
	if (feof(file))
		return FILE_END;
	int argc = 0;
	int allocatedTokens = 5;
	char **argv = malloc(allocatedTokens * sizeof(char *));
	for (;;) {
		if (argc >= allocatedTokens) {
			allocatedTokens *= 2;
			char **newargv = realloc(argv,
			      allocatedTokens * sizeof(char *));
			if (newargv == NULL)
				goto error;
			argv = newargv;
		}
		ReturnCode code = getToken(file, argv + argc);

		switch (code) {
			case ERROR:
				goto error;
			case LINE_END:
				if (argc == 0)
					continue;
				//We allow empty lines
				//fallthrough
			case FILE_END:
				if (argc == 0) {
					free(argv);
					return FILE_END;
				}
				*argcret = argc;
				*argvret = argv;
				return SUCCESS;
			case SUCCESS:
				argc++;
				break;
		}
	}
error:
	freeTokens(argc, argv);
	return ERROR;
}

static char *copyString(char *str) {
	size_t len = strlen(str);
	char *ret = malloc(len + 1);
	if (ret == NULL)
		return NULL;
	memcpy(ret, str, len + 1);
	return ret;
}

Sitefile *parseFile(char *path) {
	FILE *file = fopen(path, "r");
	if (file == NULL)
		return NULL;
	Sitefile *ret = malloc(sizeof(Sitefile));
	if (ret == NULL)
		return NULL;
	int allocatedLength = 50;
	ret->size = 0;
	ret->content = malloc(allocatedLength * sizeof(SiteCommand));
	if (ret->content == NULL) {
		free(ret);
		return NULL;
	}
	RequestType respondto = GET;
	int argc;
	char **argv;
	for (;;) {
		ReturnCode status = getCommand(file, &argc, &argv);
		switch (status) {
			case FILE_END:
				fclose(file);
				return ret;
			case ERROR: case LINE_END:
				goto nterror;
			case SUCCESS:
				break;
		}
		if (strcmp(argv[0], "set") == 0) {
			if (argc < 3)
				goto error;
			if (strcmp(argv[1], "respondto") == 0) {
				respondto = getType(argv[2]);
				if (respondto == INVALID)
					goto error;
			}
			else
				goto error;
			continue;
		}
		if (ret->size >= allocatedLength) {
			allocatedLength *= 2;
			SiteCommand *newcontent = realloc(ret->content,
					allocatedLength * sizeof(SiteCommand));
			if (newcontent == NULL)
				goto error;
			ret->content = newcontent;
		}
		if (argc < 3)
			goto error;

		if (regcomp(&ret->content[ret->size].path, argv[1],
		            REG_EXTENDED | REG_NOSUB))
			goto error;

		if (strcmp(argv[0], "read") == 0) {
			ret->content[ret->size].arg = copyString(argv[2]);
			if (ret->content[ret->size].arg == NULL)
				goto error;
		}
		else
			goto error;
		freeTokens(argc, argv);
		ret->size++;
	}
error:
	freeTokens(argc, argv);
nterror:
	freeSitefile(ret);
	return NULL;
}

void freeSitefile(Sitefile *site) {
	for (long i = 0; i < site->size; i++) {
		regfree(&site->content[i].path);
		free(site->content[i].arg);
	}
	free(site->content);
	free(site);
}
