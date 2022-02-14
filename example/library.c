#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <swebs/swebs.h>

int getResponse(Request *request, Response *response) {
	char *str = request->path;
	response->type = BUFFER;
	response->response.buffer.data = malloc(100);
	strcpy(response->response.buffer.data, str);
	response->response.buffer.len = strlen(str);
	return 200;
}
