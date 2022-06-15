# Part 1: The problem

A static webpage is easy. Just tell swebs to read a few files given a few http paths and you're done. Dyanmic pages are harder. There are a few solutions, you could execute a program and return stdout (too slow), encourage each website to create their own forks of swebs and make it really easy to directly modify the source code to add a dynamic page (inelegant), write a php interpreter (bloated), execute a program the user creates and keep it running while the web server communicates with it (what's the point of writing a web server if the user does all the work of setting up a server and responding to requests?)

# Part 2: The solution

The solution I thought of was to dynamically load a C library the user optionally writes that creates pages. The user is responsible for writing C code that generates pages, and swebs is responsible for parsing requests and asking for said pages. The library the user creates must be a shared object file, and define this function:

```int getResponse(Request *request, Response *response)```

```getResponse``` returns the HTTP response code of that request.

```Request``` and ```Response``` are defined in ```<swebs/types.h>```. ```getResponse()```. ```<swebs/types.h>``` is guarunteed to be included by ```<swebs/swebs.h>```, where ```getResponse()``` is defined.

The specific library to use is set with the ```library``` global variable in a sitefile.

The various data types important to you in this scenario are:

```
typedef struct {
	char *field;
	char *value;
} Field;
/*HTTP field*/

typedef enum {
	GET,
	POST,
	PUT,
	HEAD,
	DELETE,
	PATCH,
	OPTIONS,
	INVALID
	/*
	 * Indicates an invalid type of request, if you see this then
	 * something's gone terribly wrong.
	 * */
} RequestType;

typedef struct {
	char *data;
	/* data is guarunteed to be NULL terminated, although not necessarily
	 * once. */
	size_t len;
	size_t allocatedLen;
	/* The amount of bytes allocated, for internal use. */
} BinaryString;

typedef struct {
	BinaryString var;
	BinaryString value;
} PathField;

typedef struct {
	BinaryString path;
	long fieldCount;
	PathField *fields;
} Path;

typedef struct {
	long fieldCount;
	Field *fields;
	Path path;
	RequestType type;
} Request;
/*HTTP request, pretty self explanatory*/

typedef enum {
	FILE_KNOWN_LENGTH,
	/* A file where the total length is known (i.e. a file on disk) */
	FILE_UNKNOWN_LENGTH,
	/* A file where the total length is unknown (i.e. a pipe) */
	BUFFER,
	/* A buffer stored in memory. free() will be called on said buffer. */
	BUFFER_NOFREE,
	/* Same as BUFFER but free() won't be called */
	DEFAULT
	/* The default response for the response code */
} ResponseType;

typedef struct {
	int fd;
	size_t len;
	/* This field is sometimes optional */
} File;

typedef struct {
	void *data;
	/* This data will be freed. */
	size_t len;
} Buffer;

typedef struct {
	ResponseType type;
	union {
		File file;
		Buffer buffer;
	} response;
} Response;
```
