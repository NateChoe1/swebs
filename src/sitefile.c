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

#include <swebs/config.h>

#include <swebs/util.h>
#include <swebs/sitefile.h>
#include <swebs/responseutil.h>
#if DYNAMIC_LINKED_PAGES
#include <swebs/dynamic.h>
#endif
/*
 * This if isn't technically necessary, but it generates warnings, which is
 * good.
 * */

typedef enum {
	SUCCESS,
	LINE_END,
	FILE_END,
	ERROR
} ReturnCode;
/* this isn't ideal, but it's necessary to avoid namespace collisions. */

static void freeTokens(int argc, char **argv) {
	int i;
	for (i = 0; i < argc; i++)
		free(argv[i]);
	free(argv);
}

static ReturnCode getToken(FILE *file, char **ret) {
	typedef enum {
		QUOTED,
		NONQUOTED
	} TokenType;
	TokenType type;
	size_t allocatedLen = 50;
	size_t len;

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

	*ret = malloc(allocatedLen);

	for (len = 0;; len++) {
		int c;
		if (len >= allocatedLen) {
			char *newret;
			allocatedLen *= 2;
			newret = realloc(*ret, allocatedLen);
			if (newret == NULL)
				goto error;
			*ret = newret;
		}
		c = fgetc(file);
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
/* THIS FUNCTION WILL NOT RETURN LINE_END */
	int argc;
	char **argv;
	int allocatedTokens;
	if (feof(file))
		return FILE_END;
	argc = 0;
	allocatedTokens = 5;
	argv = malloc(allocatedTokens * sizeof(*argv));
	for (;;) {
		ReturnCode code;
		if (argc >= allocatedTokens) {
			char **newargv;
			allocatedTokens *= 2;
			newargv = realloc(argv,
			      allocatedTokens * sizeof(char *));
			if (newargv == NULL)
				goto error;
			argv = newargv;
		}
		code = getToken(file, argv + argc);

		switch (code) {
			case ERROR:
				goto error;
			case LINE_END:
				if (argc == 0)
					continue;
				/* We allow empty lines */
				/* fallthrough */
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

static char *getport(char *data, unsigned short *ret) {
	*ret = 0;
	while (isdigit(data[0])) {
		*ret *= 10;
		*ret += data[0] - '0';
		++data;
	}
	if (data[0] == ',')
		return data + 1;
	if (data[0] != '\0')
		return NULL;
	return data;
}

static int getports(unsigned short **ports, int *portcount, char *data) {
	int alloc;
	alloc = 10;
	*portcount = 0;
	*ports = xmalloc(alloc * sizeof **ports);
	for (;;) {
		if (data[0] == '\0')
			return 0;
		if (*portcount >= alloc) {
			alloc *= 2;
			*ports = xrealloc(*ports, alloc * sizeof **ports);
		}
		{
			unsigned short newport;
			data = getport(data, &newport);
			(*ports)[*portcount] = newport;
			++*portcount;
		}
		if (data == NULL) {
			free(*ports);
			return 1;
		}
	}
}

Sitefile *parseSitefile(char *path) {
	FILE *file;
	RequestType respondto = GET;
	const int cflags = REG_EXTENDED | REG_ICASE;
	char *host = NULL;
	int argc;
	char **argv;
	Sitefile *ret;
	unsigned short *ports;
	int portcount;

	file = fopen(path, "r");
	if (file == NULL)
		return NULL;
	ret = xmalloc(sizeof *ret);

	ports = malloc(sizeof *ports);
	ports[0] = 80;
	portcount = 1;

	ret->size = 0;
	ret->alloc = 50;
	ret->content = xmalloc(ret->alloc * sizeof *ret->content);
	ret->portcount = 0;
	ret->portalloc = 5;
	ret->ports = xmalloc(ret->portalloc * sizeof *ret->ports);
#if DYNAMIC_LINKED_PAGES
	ret->getResponse = NULL;
#endif

	for (;;) {
		ReturnCode status = getCommand(file, &argc, &argv);
		switch (status) {
			int i;
			case FILE_END:
				free(ports);
				for (i = 0; i < ret->portcount; ++i) {
					Port *port = ret->ports + i;
					if (port->type == TLS &&
						(port->key == NULL ||
						 port->cert == NULL)) {
						fprintf(stderr,
"Port %hu declared as TLS without proper TLS files\n", port->num);
						goto nterror;
					}
				}
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
			else if (strcmp(argv[1], "host") == 0)
				host = xstrdup(argv[2]);
			else if (strcmp(argv[1], "port") == 0) {
				free(ports);
				if (getports(&ports, &portcount, argv[2])) {
					fprintf(stderr, "Invalid port list %s\n",
							argv[2]);
					goto error;
				}
			}
			else
				goto error;
			continue;
		}
		else if (strcmp(argv[0], "define") == 0) {
			if (argc < 3)
				goto error;
			else if (strcmp(argv[1], "library") == 0) {
#if DYNAMIC_LINKED_PAGES
				ret->getResponse = loadGetResponse(argv[2]);
#else
				fputs(
"This version of swebs has no dynamic page support\n", stderr);
				exit(EXIT_FAILURE);
#endif
			}
			else
				goto error;
			continue;
		}
		else if (strcmp(argv[0], "declare") == 0) {
			Port newport;
			int i;
			if (argc < 3) {
				fputs(
"Usage: declare [transport] [port]\n", stderr);
				goto error;
			}
			newport.num = atoi(argv[2]);

			for (i = 0; i < ret->portcount; ++i) {
				if (ret->ports[i].num == newport.num) {
					fprintf(stderr,
"Port %hu declared multiple times\n", newport.num);
					goto error;
				}
			}

			if (strcmp(argv[1], "TCP") == 0)
				newport.type = TCP;
			else if (strcmp(argv[1], "TLS") == 0)
				newport.type = TLS;
			else {
				fprintf(stderr, "Invalid transport %s\n",
						argv[1]);
				goto error;
			}
			newport.timeout = 2000;
			newport.key = newport.cert = NULL;
			if (ret->portcount >= ret->portalloc) {
				ret->portalloc *= 2;
				ret->ports = xrealloc(ret->ports,
					ret->portalloc * sizeof *ret->ports);
			}
			memcpy(ret->ports + ret->portcount, &newport,
					sizeof newport);
			++ret->portcount;
			continue;
		}
#define PORT_ATTRIBUTE(name, func) \
		else if (strcmp(argv[0], #name) == 0) { \
			int i; \
			unsigned short port; \
			if (argc < 3) { \
				fputs("Usage: " #name " [" #name "] [port]\n", \
						stderr); \
				goto error; \
			} \
			port = atoi(argv[2]); \
			for (i = 0; i < ret->portcount; ++i) \
				if (ret->ports[i].num == port) \
					ret->ports[i].name = func(argv[1]); \
			continue; \
		}
		PORT_ATTRIBUTE(key, xstrdup)
		PORT_ATTRIBUTE(cert, xstrdup)
		PORT_ATTRIBUTE(timeout, atoi)
#undef PORT_ATTRIBUTE
		if (ret->size >= ret->alloc) {
			SiteCommand *newcontent;
			ret->alloc *= 2;
			newcontent = realloc(ret->content, ret->alloc *
					sizeof *newcontent);
			if (newcontent == NULL)
				goto error;
			ret->content = newcontent;
		}

		if (regcomp(&ret->content[ret->size].path, argv[1],
		            cflags))
			goto error;

		if (strcmp(argv[0], "read") == 0) {
			if (argc < 3)
				goto error;
			ret->content[ret->size].arg = xstrdup(argv[2]);
			if (ret->content[ret->size].arg == NULL)
				goto error;
			ret->content[ret->size].command = READ;
		}
		else if (strcmp(argv[0], "throw") == 0) {
			if (argc < 3)
				goto error;
			ret->content[ret->size].arg = getCode(atoi(argv[2]));
			if (ret->content[ret->size].arg == NULL)
				goto error;
			ret->content[ret->size].command = THROW;
		}
		else if (strcmp(argv[0], "linked") == 0) {
#if DYNAMIC_LINKED_PAGES
			ret->content[ret->size].command = LINKED;
#else
			fputs(
"This version of swebs doesn't have linked page support", stderr);
			goto error;
#endif
		}
		else {
			fprintf(stderr, "Unknown sitefile command %s", argv[0]);
			goto error;
		}
		freeTokens(argc, argv);
		ret->content[ret->size].respondto = respondto;
		if (host == NULL)
			regcomp(&ret->content[ret->size].host, ".*", cflags);
		else
			regcomp(&ret->content[ret->size].host, host, cflags);

		ret->content[ret->size].ports = xmalloc(portcount *
				sizeof *ret->content[ret->size].ports);
		memcpy(ret->content[ret->size].ports, ports, portcount * sizeof *ports);
		ret->content[ret->size].portcount = portcount;

		ret->size++;
	}
error:
	freeTokens(argc, argv);
nterror:
	freeSitefile(ret);
	return NULL;
}

void freeSitefile(Sitefile *site) {
	long i;
	for (i = 0; i < site->size; i++) {
		regfree(&site->content[i].path);
		regfree(&site->content[i].host);
		free(site->content[i].arg);
	}
	free(site->content);
	free(site);
}
