# Part 1: Introduction
Every single website hosted with swebs has a sitefile associated with it.

sitefiles specify the layout of the hosted site. It basically tells swebs what to do to get the response.

sitefiles consist of commands, which are of the form

```[action] [arguments]```

sitefiles also allow comments with #

# Part 2: Commands

* ```set [variable] [value]``` - sets some variable for the following pages
* ```read [http path] [file path]``` - if the requested path matches ```[http path]```, return the contents of ```[file path]```. If [file path] is a directory, then the http path is appended to [file path] and that is read instead.

##### Other than set, commands should take in a regex as argument 1 and operate on a file specified in argument 2.

# Part 3: Variables

* ```respondto```: - The type of http request to respond to. One of:
	* GET (defualt)
	* POST
* ```host```: - The hostname to respond to. Case insensitive, default: localhost
