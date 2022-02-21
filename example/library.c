#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <swebs/swebs.h>

int getResponse(Request *request, Response *response) {
	printf("%d\n", request->path.fieldCount);
	for (int i = 0; i < request->path.fieldCount; i++)
		printf("%s: %s\n", request->path.fields[i].var.data, request->path.fields[i].value.data);
	char *str = request->path.path.data;
	response->type = BUFFER;
	response->response.buffer.data = malloc(100);
	strcpy(response->response.buffer.data, str);
	response->response.buffer.len = strlen(str);
	return 200;
}
