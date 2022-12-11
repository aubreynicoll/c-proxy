#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/**
 * From Beej's Guide to Network Programming.
 *
 * Helper function that consumes a sockaddr_storage, and
 * returns the address of the sin(6?)_addr
 */
void *getinaddr(struct sockaddr_storage *ss) {
	if (ss->ss_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in *)ss;
		return &sin->sin_addr;
	}
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ss;
	return &sin6->sin6_addr;
}

/**
 * Get a socket listener or die trying
 */
int getlistenfd(const char *port) {
	int status, listenfd, yes = 1;
	struct addrinfo hints, *p, *listp;

	// initialize hints
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	// get host info
	status = getaddrinfo(NULL, port, &hints, &listp);
	if (status) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		exit(1);
	}

	// try to bind socket
	for (p = listp; p; p = p->ai_next) {
		// get socket
		listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listenfd < 0) {
			continue;
		}

		// set options
		status = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				    sizeof yes);
		if (status < 0) {
			fprintf(stderr, "setsockopt: %s\n", strerror(errno));
			exit(1);
		}

		// bind socket
		status = bind(listenfd, p->ai_addr, p->ai_addrlen);
		if (status < 0) {
			close(listenfd);
			continue;
		}

		break;
	}

	// check if failed to bind
	if (!p) {
		fprintf(stderr, "failed to bind socket");
		exit(1);
	}

	freeaddrinfo(listp);

	// listen on socket
	status = listen(listenfd, 20);
	if (status < 0) {
		fprintf(stderr, "listen: %s\n", strerror(errno));
		exit(1);
	}

	return listenfd;
}

/**
 * Block while waiting for a client. Returns -1 on error, or the connected
 * socket fd if successful. addrbuf is optional, and will be filled in with an
 * address string a la inet_ntop(3)
 */
int getconnfd(int listenfd, char *addrbuf, socklen_t addrlen) {
	int connfd;
	struct sockaddr_storage connaddr;
	socklen_t connaddrlen = sizeof connaddr;

	connfd = accept(listenfd, (struct sockaddr *)&connaddr, &connaddrlen);
	if (connfd < 0) {
		return -1;
	}

	if (addrbuf) {
		inet_ntop(connaddr.ss_family, getinaddr(&connaddr), addrbuf,
			  addrlen);
	}

	return connfd;
}

/**
 * Get a connection to a remote host. Returns -1 on error.
 */
int getclientfd(const char *host, const char *service) {
	int status, clientfd;
	struct addrinfo hints, *p, *listp;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	status = getaddrinfo(host, service, &hints, &listp);
	if (status) {
		return -1;
	}

	for (p = listp; p; p = p->ai_next) {
		clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (clientfd < 0) {
			continue;
		}

		status = connect(clientfd, p->ai_addr, p->ai_addrlen);
		if (status < 0) {
			close(clientfd);
			continue;
		}

		break;
	}

	freeaddrinfo(listp);

	if (!p) {
		return -1;
	}

	return clientfd;
}
