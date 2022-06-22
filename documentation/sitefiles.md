# Part 1: Introduction
Every single website hosted with swebs has a sitefile associated with it.

sitefiles specify the layout of the hosted site. It basically tells swebs what to do to get the response.

sitefiles consist of commands, which are of the form

```[action] [arguments]```

sitefiles also allow comments with #

# Part 2: Commands

* ```set [variable] [value]``` - sets some local variable for the following pages
* ```define [variable] [value]``` - sets some global variable
* ```read [http path] [file path]``` - if the requested path matches ```[http path]```, return the contents of ```[file path]```. If [file path] is a directory, then the http path is appended to [file path] and that is read instead.
* ```linked``` - Run getResponse() from the library loaded from the library global variable
* ```throw [http path] [error code]``` - If the requested path matches ```[http path]```, send back the http error code ```[error code]```. For standardization purposes, these error codes are just the number.
* ```declare [transport] [port]``` - Declares that port ```[port]``` will be used with transport ```[transport]``` where ```[transport]``` is one of ```TCP```, ```TLS```
* ```key [key file] [port]``` - Sets the key file for port ```[port]``` to ```[key file]```
* ```cert [cert file] [port]``` - Sets the certificate file for port ```[port]``` to ```[cert file]```
* ```timeout [timeout] [port]``` - Sets the connection timeout for port ```[port]``` to ```[timeout]``` milliseconds

##### Other than set, commands should take in a regex as argument 1 and operate on a file specified in argument 2.

# Part 3: Local variables

* ```respondto``` - The type of http request to respond to. One of:
	* GET (defualt)
	* POST
* ```host``` - The hostname to respond to. Case insensitive regex, default: .*
* ```port``` - The port to respond to, default: 80

# Part 4: Global variables

* ```library``` - the path of a library that is linked in during runtime if ```DYNAMIC_LINKED_PAGES```is set.
