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
	ARG,
	LINE_END,
	FILE_END,
	TOKEN_ERROR
} TokenType;

typedef struct {
	TokenType type;
	char *data;
} Token;

typedef enum {
	NORMAL,
	PAST_END,
	COMMAND_ERROR
} CommandType;

static void freecommand(int argc, char **argv) {
	int i;
	for (i = 0; i < argc; i++)
		free(argv[i]);
	free(argv);
}

static void gettoken(FILE *file, Token *ret) {
	int c;
	char *data;
	size_t len;
	size_t alloc;
	for (;;) {
		c = fgetc(file);
		switch (c) {
		case '#':
			while (c != '\n' && c != EOF)
				c = fgetc(file);
		case '\n':
			ret->type = LINE_END;
			return;
		case EOF:
			ret->type = FILE_END;
			return;
		case ' ': case '\t':
			continue;
		}
		ret->type = ARG;
		ungetc(c, file);
		break;
	}

	alloc = 20;
	data = xmalloc(alloc);
	for (len = 0;; ++len) {
		if (len >= alloc) {
			alloc *= 2;
			data = xrealloc(data, alloc);
		}
		c = fgetc(file);
		if (isspace(c) || c == EOF) {
			data[len] = '\0';
			ret->type = ARG;
			ret->data = data;
			return;
		}
		switch (c) {
		case '\\':
			c = fgetc(file);
			if (c == EOF) {
				ret->type = TOKEN_ERROR;
				return;
			}
		default:
			data[len] = c;
		}
	}
}

static CommandType getcommand(FILE *file, int *argcret, char ***argvret) {
	int argc, argalloc;
	char **argv;
	argalloc = 5;
	argv = xmalloc(argalloc * sizeof *argv);

	for (argc = 0;; ++argc) {
		Token token;
		if (argc >= argalloc) {
			argalloc *= 2;
			argv = xrealloc(argv, argalloc * sizeof *argv);
		}
		gettoken(file, &token);
		switch (token.type) {
		case FILE_END:
			if (argc == 0)
				return PAST_END;
			goto gotcommand;
		case LINE_END:
			if (argc == 0)
				return getcommand(file, argcret, argvret);
			goto gotcommand;
		gotcommand:
			*argcret = argc;
			*argvret = argv;
			return NORMAL;
		case ARG:
			argv[argc] = token.data;
			break;
		case TOKEN_ERROR:
			return COMMAND_ERROR;
		}
	}
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
	char *contenttype;

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

	contenttype = xstrdup("text/html");

	for (;;) {
		int i;
		CommandType commandtype;
		commandtype = getcommand(file, &argc, &argv);
		switch (commandtype) {
		case PAST_END:
			free(ports);
			for (i = 0; i < ret->portcount; ++i) {
				Port *port = ret->ports + i;
				if (port->type == TLS &&
					(port->key == NULL ||
					 port->cert == NULL)) {
					fprintf(stderr,
"Port %hu declarS without proper TLS files\n", port->num);
					goto nterror;
				}
			}
			free(contenttype);
			free(host);
			fclose(file);
			return ret;
		case COMMAND_ERROR:
			goto nterror;
		case NORMAL:
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
			else if (strcmp(argv[1], "host") == 0) {
				free(host);
				host = xstrdup(argv[2]);
			}
			else if (strcmp(argv[1], "port") == 0) {
				free(ports);
				if (getports(&ports, &portcount, argv[2])) {
					fprintf(stderr, "Invalid port list %s\n",
							argv[2]);
					goto error;
				}
			}
			else if (strcmp(argv[1], "type") == 0) {
				free(contenttype);
				contenttype = strdup(argv[2]);
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
		freecommand(argc, argv);
		ret->content[ret->size].respondto = respondto;
		if (host == NULL)
			regcomp(&ret->content[ret->size].host, ".*", cflags);
		else
			regcomp(&ret->content[ret->size].host, host, cflags);

		ret->content[ret->size].ports = xmalloc(portcount *
				sizeof *ret->content[ret->size].ports);
		memcpy(ret->content[ret->size].ports, ports, portcount * sizeof *ports);
		ret->content[ret->size].portcount = portcount;

		ret->content[ret->size].contenttype = xstrdup(contenttype);

		ret->size++;
	}
error:
	freecommand(argc, argv);
nterror:
	freeSitefile(ret);
	return NULL;
}

void freeSitefile(Sitefile *site) {
	long i;
	for (i = 0; i < site->size; ++i) {
		regfree(&site->content[i].path);
		regfree(&site->content[i].host);
		/* This doesn't break because free(NULL) is harmless. */

		free(site->content[i].arg);
		free(site->content[i].ports);
		free(site->content[i].contenttype);
	}
	free(site->content);
	for (i = 0; i < site->portcount; ++i) {
		free(site->ports[i].key);
		free(site->ports[i].cert);
	}
	free(site->ports);
	free(site);
}
