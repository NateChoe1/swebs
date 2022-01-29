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

##### Other than set, commands should take in a regex as argument 1 and operate on a file specified in argument 2.

# Part 3: Local variables

* ```respondto``` - The type of http request to respond to. One of:
	* GET (defualt)
	* POST
* ```host``` - The hostname to respond to. Case insensitive, default: localhost

# Part 4: Global variables

* ```transport``` - the type of connection to use. One of:
	* TCP (default)
	* TLS
* ```key``` - The filepath of the private key to use if transport == TLS
* ```cert``` - The filepath of the certificate to use if transport == TLS
