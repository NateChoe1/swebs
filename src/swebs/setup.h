#ifndef HAVE_SETUP
#define HAVE_SETUP

#include <swebs/sockets.h>
#include <swebs/sitefile.h>

void setup(int argc, char **argv,
		Sitefile **site, Listener **listener, int *processes);
/* Setup parses args, utilizes them, and returns only what is needed in the
 * main loop. */

#endif
