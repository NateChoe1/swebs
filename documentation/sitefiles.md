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
* ```exec [http path] [file path]``` - if the requested path matches ```[http path]```, execute ```[file path]``` and send the the contents of stdout. ```argv[0]``` is the requested path, ```argv[1]``` is a header, ```argv[2]``` is the value for that header, ```argv[3]``` is the next header, and so on.
* ```linked``` - Run getResponse() from the library loaded from the library global variable
* ```throw [http path] [error code]``` - If the requested path matches ```[http path]```, send back the http error code ```[error code]```. For standardization purposes, these error codes are just the number.

##### Other than set, commands should take in a regex as argument 1 and operate on a file specified in argument 2.

# Part 3: Local variables

* ```respondto``` - The type of http request to respond to. One of:
	* GET (defualt)
	* POST
* ```host``` - The hostname to respond to. Case insensitive regex, default: .*

# Part 4: Global variables

* ```port``` - the port to use. Note that this is a global variable, and so one instance of swebs cannot use multiple ports.
* ```transport``` - the type of connection to use. One of:
	* TCP (default)
	* TLS
* ```key``` - The filepath of the private key to use if transport == TLS
* ```cert``` - The filepath of the certificate to use if transport == TLS
* ```timeout``` - The amount of time to wait for data before closing the connection in ms. A timeout of 0 means to wait infinitely. (default: 2000)
* ```library``` - the path of a library that is linked in during runtime if ```DYNAMIC_LINKED_PAGES```is set.
