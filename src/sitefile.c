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

#define CFLAGS (REG_EXTENDED | REG_ICASE)

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

typedef struct {
	RequestType respondto;
	char *host;
	unsigned short *ports;
	int portcount;
	char *contenttype;
} LocalVars;

typedef enum {
	DATA_CHANGE,
	SITE_SPEC,
	COMMAND_RET_ERROR
} CommandReturn;

static CommandReturn localvar(LocalVars *vars, Sitefile *sitefile,
		int argc, char **argv) {
	if (argc < 3)
		return COMMAND_RET_ERROR;
	if (strcmp(argv[1], "respondto") == 0) {
		if ((vars->respondto = getType(argv[2])) == INVALID)
			return COMMAND_RET_ERROR;
		return DATA_CHANGE;
	}
	if (strcmp(argv[1], "host") == 0) {
		free(vars->host);
		vars->host = xstrdup(argv[2]);
		return DATA_CHANGE;
	}
	else if (strcmp(argv[1], "port") == 0) {
		free(vars->ports);
		if (getports(&vars->ports, &vars->portcount, argv[2])) {
			fprintf(stderr, "Invalid port list %s\n", argv[2]);
			return COMMAND_RET_ERROR;
		}
		return DATA_CHANGE;
	}
	else if (strcmp(argv[1], "type") == 0) {
		free(vars->contenttype);
		vars->contenttype = strdup(argv[2]);
		return DATA_CHANGE;
	}
	return COMMAND_RET_ERROR;
}

static CommandReturn globalvar(LocalVars *vars, Sitefile *sitefile,
		int argc, char **argv) {
	if (argc < 3)
		return COMMAND_RET_ERROR;
	if (strcmp(argv[1], "library") == 0) {
#if DYNAMIC_LINKED_PAGES
		sitefile->getResponse = loadGetResponse(argv[2]);
		return DATA_CHANGE;
#else
		fputs("This version of swebs has no dynamic page support\n",
				stderr);
		return COMMAND_RET_ERROR;
#endif
	}
	return COMMAND_RET_ERROR;
}

static CommandReturn declareport(LocalVars *vars, Sitefile *sitefile,
		int argc, char **argv) {
	Port newport;
	int i;
	if (argc < 3) {
		fputs("Usage: declare [transport] [port]\n", stderr);
		return COMMAND_RET_ERROR;
	}
	newport.num = atoi(argv[2]);

	for (i = 0; i < sitefile->portcount; ++i) {
		if (sitefile->ports[i].num == newport.num) {
			fprintf(stderr, "Port %hu declared multiple times\n",
					newport.num);
			return COMMAND_RET_ERROR;
		}
	}

	if (strcmp(argv[1], "TCP") == 0)
		newport.type = TCP;
	else if (strcmp(argv[1], "TLS") == 0)
		newport.type = TLS;
	else {
		fprintf(stderr, "Invalid transport %s\n", argv[1]);
		return COMMAND_RET_ERROR;
	}
	newport.timeout = 2000;
	newport.key = newport.cert = NULL;
	if (sitefile->portcount >= sitefile->portalloc) {
		sitefile->portalloc *= 2;
		sitefile->ports = xrealloc(sitefile->ports,
			sitefile->portalloc * sizeof *sitefile->ports);
	}
	memcpy(sitefile->ports + sitefile->portcount, &newport,
			sizeof newport);
	++sitefile->portcount;
	return DATA_CHANGE;
}

static CommandReturn portvar(LocalVars *vars, Sitefile *sitefile,
		int argc, char **argv) {
#define PORT_ATTRIBUTE(name, func) \
	if (strcmp(argv[0], #name) == 0) { \
		int i; \
		unsigned short port; \
		if (argc < 3) { \
			fputs("Usage: " #name " [" #name "] [port]\n", \
					stderr); \
			return COMMAND_RET_ERROR; \
		} \
		port = atoi(argv[2]); \
		for (i = 0; i < sitefile->portcount; ++i) \
			if (sitefile->ports[i].num == port) \
				sitefile->ports[i].name = func(argv[1]); \
		return DATA_CHANGE; \
	}
	PORT_ATTRIBUTE(key, xstrdup)
	PORT_ATTRIBUTE(cert, xstrdup)
	PORT_ATTRIBUTE(timeout, atoi)
#undef PORT_ATTRIBUTE
	return COMMAND_RET_ERROR;
}

static int expandsitefile(Sitefile *sitefile, char *regex) {
	if (sitefile->size >= sitefile->alloc) {
		SiteCommand *newcontent;
		sitefile->alloc *= 2;
		newcontent = xrealloc(sitefile->content, sitefile->alloc *
				sizeof *newcontent);
		sitefile->content = newcontent;
	}

	return regcomp(&sitefile->content[sitefile->size].path, regex, CFLAGS);
}

static char *getcodestring(const char *str) {
	return getCode(atoi(str));
}

static CommandReturn defsitespec(LocalVars *vars, Sitefile *sitefile,
		int argc, char **argv) {
	const struct {
		char *command;
		char *(*getarg)(const char *);
		Command type;
	} sitespecs[] = {
		{"read", strdup, READ},
		{"throw", getcodestring, THROW},
	};
	int i;
	if (argc < 3)
		return COMMAND_RET_ERROR;
	expandsitefile(sitefile, argv[1]);
	for (i = 0; i < LEN(sitespecs); ++i) {
		if (strcmp(argv[0], sitespecs[i].command) == 0) {
			sitefile->content[sitefile->size].arg =
				sitespecs[i].getarg(argv[2]);
			if (sitefile->content[sitefile->size].arg == NULL)
				return COMMAND_RET_ERROR;
			sitefile->content[sitefile->size].command =
				sitespecs[i].type;
			return SITE_SPEC;
		}
	}
	return COMMAND_RET_ERROR;
}

static CommandReturn linkedsitespec(LocalVars *vars, Sitefile *sitefile,
		int argc, char **argv) {
#if DYNAMIC_LINKED_PAGES
	if (argc < 2)
		return COMMAND_RET_ERROR;
	expandsitefile(sitefile, argv[1]);
	sitefile->content[sitefile->size].command = LINKED;
	return SITE_SPEC;
#else
	fputs("This version of swebs doesn't have linked page support", stderr);
	return COMMAND_RET_ERROR;
#endif
}

Sitefile *parseSitefile(char *path) {
	FILE *file;
	int argc;
	char **argv;
	Sitefile *ret;
	const struct {
		char *name;
		CommandReturn (*updatesitefile)(LocalVars *vars,
				Sitefile *sitefile,
				int argc, char **argv);
	} commandspec[] = {
		{"set",     localvar},
		{"define",  globalvar},
		{"read",    defsitespec},
		{"throw",   defsitespec},
		{"linked",  linkedsitespec},
		{"declare", declareport},
		{"key",     portvar},
		{"cert",    portvar},
		{"timeout", portvar},
	};
	LocalVars vars;

	file = fopen(path, "r");
	if (file == NULL)
		return NULL;

	vars.respondto = GET;
	vars.host = xstrdup(".*");
	vars.ports = xmalloc(sizeof *vars.ports);
	vars.ports[0] = 80;
	vars.portcount = 1;
	vars.contenttype = xstrdup("text/html");

	ret = xmalloc(sizeof *ret);
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
		int i;
		CommandType commandtype;
nextcommand:
		commandtype = getcommand(file, &argc, &argv);
		switch (commandtype) {
		case PAST_END:
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
			free(vars.ports);
			free(vars.contenttype);
			free(vars.host);
			fclose(file);
			return ret;
		case COMMAND_ERROR:
			goto nterror;
		case NORMAL:
			break;
		}
		for (i = 0; i < LEN(commandspec); ++i) {
			if (strcmp(argv[0], commandspec[i].name) == 0) {
				switch (commandspec[i].updatesitefile(&vars,
							ret, argc, argv)) {
				case DATA_CHANGE:
					goto nextcommand;
				case SITE_SPEC:
					goto newsitespec;
				case COMMAND_RET_ERROR:
					goto error;
				}
				break;
			}
		}
		fprintf(stderr, "Unknown sitefile command %s", argv[0]);
		goto error;
newsitespec:
		freecommand(argc, argv);
		ret->content[ret->size].respondto = vars.respondto;
		regcomp(&ret->content[ret->size].host, vars.host, CFLAGS);

		ret->content[ret->size].ports = xmalloc(vars.portcount *
				sizeof *ret->content[ret->size].ports);
		memcpy(ret->content[ret->size].ports, vars.ports,
				vars.portcount * sizeof *vars.ports);
		ret->content[ret->size].portcount = vars.portcount;

		ret->content[ret->size].contenttype = xstrdup(vars.contenttype);

		++ret->size;
	}
error:
	freecommand(argc, argv);
nterror:
	free(vars.ports);
	free(vars.contenttype);
	free(vars.host);
	freeSitefile(ret);
	return NULL;
}

void freeSitefile(Sitefile *site) {
	long i;
	for (i = 0; i < site->size; ++i) {
		regfree(&site->content[i].path);
		regfree(&site->content[i].host);
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
