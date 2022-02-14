#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <swebs/swebs.h>

int getResponse(Request *request, Response *response) {
	char *str = "<h1>Hello world!</h1>";
	response->type = BUFFER;
	response->response.buffer.data = malloc(30);
	strcpy(response->response.buffer.data, str);
	response->response.buffer.len = strlen(str);
	return 200;
}
