/* @(#)rprintmsg.c	2.1 88/08/11 4.0 RPCSRC */
/*
 * rprintmsg.c: remote version of "printmsg.c"
 */
#include <stdio.h>
#include <rpc/rpc.h>		/* always need this */
#include "msg.h"		/* need this too: will be generated by rpcgen*/

main(argc, argv)
	int argc;
	char *argv[];
{
	CLIENT *cl;
	int *result;
	char *server;
	char *message;

	if (argc < 3) {
		fprintf(stderr, "usage: %s host message\n", argv[0]);
		exit(1);
	}

	/*
	 * Remember what our command line arguments refer to
	 */
	server = argv[1];
	message = argv[2];

	/*
	 * Create client "handle" used for calling MESSAGEPROG on the
	 * server designated on the command line. We tell the rpc package
	 * to use the "tcp" protocol when contacting the server.
	 */
	cl = clnt_create(server, MESSAGEPROG, MESSAGEVERS, "tcp");
	if (cl == NULL) {
		/*
		 * Couldn't establish connection with server.
		 * Print error message and die.
		 */
		clnt_pcreateerror(server);
		exit(1);
	}

	/*
	 * Call the remote procedure "printmessage" on the server
	 */
	result = printmessage_1(&message, cl);
	if (result == NULL) {
		/*
		 * An error occurred while calling the server.
	 	 * Print error message and die.
		 */
		clnt_perror(cl, server);
		exit(1);
	}

	/*
	 * Okay, we successfully called the remote procedure.
	 */
	if (*result == 0) {
		/*
		 * Server was unable to print our message.
		 * Print error message and die.
		 */
		fprintf(stderr, "%s: sorry, %s couldn't print your message\n",
			argv[0], server);
		exit(1);
	}

	/*
	 * The message got printed on the server's console
	 */
	printf("Message delivered to %s!\n", server);
}
