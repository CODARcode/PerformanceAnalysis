#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <nng/nng.h>
#include <nng/protocol/reqrep0/rep.h>
#include <nng/protocol/reqrep0/req.h>

#define CLIENT "client"
#define SERVER "server"
#define DATECMD 1

#define PUT64(ptr, u)                                        \
	do {                                                 \
		(ptr)[0] = (uint8_t)(((uint64_t)(u)) >> 56); \
		(ptr)[1] = (uint8_t)(((uint64_t)(u)) >> 48); \
		(ptr)[2] = (uint8_t)(((uint64_t)(u)) >> 40); \
		(ptr)[3] = (uint8_t)(((uint64_t)(u)) >> 32); \
		(ptr)[4] = (uint8_t)(((uint64_t)(u)) >> 24); \
		(ptr)[5] = (uint8_t)(((uint64_t)(u)) >> 16); \
		(ptr)[6] = (uint8_t)(((uint64_t)(u)) >> 8);  \
		(ptr)[7] = (uint8_t)((uint64_t)(u));         \
	} while (0)

#define GET64(ptr, v)                                 \
	v = (((uint64_t)((uint8_t)(ptr)[0])) << 56) + \
	    (((uint64_t)((uint8_t)(ptr)[1])) << 48) + \
	    (((uint64_t)((uint8_t)(ptr)[2])) << 40) + \
	    (((uint64_t)((uint8_t)(ptr)[3])) << 32) + \
	    (((uint64_t)((uint8_t)(ptr)[4])) << 24) + \
	    (((uint64_t)((uint8_t)(ptr)[5])) << 16) + \
	    (((uint64_t)((uint8_t)(ptr)[6])) << 8) +  \
	    (((uint64_t)(uint8_t)(ptr)[7]))

void
fatal(const char *func, int rv)
{
	fprintf(stderr, "%s: %s\n", func, nng_strerror(rv));
	exit(1);
}

void
showdate(time_t now)
{
	struct tm *info = localtime(&now);
	printf("%s", asctime(info));
}

int
server(const char *url)
{
	nng_socket sock;
	int        rv;

	if ((rv = nng_rep0_open(&sock)) != 0) {
		fatal("nng_rep0_open", rv);
	}
	if ((rv = nng_listen(sock, url, NULL, 0)) != 0) {
		fatal("nng_listen", rv);
	}
	for (;;) {
		char *   buf = NULL;
		size_t   sz;
		uint64_t val;
		if ((rv = nng_recv(sock, &buf, &sz, NNG_FLAG_ALLOC)) != 0) {
			fatal("nng_recv", rv);
		}
		if ((sz == sizeof(uint64_t)) &&
		    ((GET64(buf, val)) == DATECMD)) {
			time_t now;
			printf("SERVER: RECEIVED DATE REQUEST\n");
			now = time(&now);
			printf("SERVER: SENDING DATE: ");
			showdate(now);

			// Reuse the buffer.  We know it is big enough.
			PUT64(buf, (uint64_t) now);
			rv = nng_send(sock, buf, sz, NNG_FLAG_ALLOC);
			if (rv != 0) {
				fatal("nng_send", rv);
			}
			continue;
		}
		// Unrecognized command, so toss the buffer.
		nng_free(buf, sz);
	}
}

int
client(const char *url)
{
	nng_socket sock;
	int        rv;
	size_t     sz;
	char *     buf = NULL;
	uint8_t    cmd[sizeof(uint64_t)];

	PUT64(cmd, DATECMD);

	if ((rv = nng_req0_open(&sock)) != 0) {
		fatal("nng_socket", rv);
	}
	if ((rv = nng_dial(sock, url, NULL, 0)) != 0) {
		fatal("nng_dial", rv);
	}
	printf("CLIENT: SENDING DATE REQUEST\n");
	if ((rv = nng_send(sock, cmd, sizeof(cmd), 0)) != 0) {
		fatal("nng_send", rv);
	}
	if ((rv = nng_recv(sock, &buf, &sz, NNG_FLAG_ALLOC)) != 0) {
		fatal("nng_recv", rv);
	}

	if (sz == sizeof(uint64_t)) {
		uint64_t now;
		GET64(buf, now);
		printf("CLIENT: RECEIVED DATE: ");
		showdate((time_t) now);
	} else {
		printf("CLIENT: GOT WRONG SIZE!\n");
	}

	// This assumes that buf is ASCIIZ (zero terminated).
	nng_free(buf, sz);
	nng_close(sock);
	return (0);
}

int
main(const int argc, const char **argv)
{
	if ((argc > 1) && (strcmp(CLIENT, argv[1]) == 0))
		return (client(argv[2]));

	if ((argc > 1) && (strcmp(SERVER, argv[1]) == 0))
		return (server(argv[2]));

	fprintf(stderr, "Usage: reqrep %s|%s <URL> ...\n", CLIENT, SERVER);
	return (1);
}