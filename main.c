#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define SOCK_UNSET -1
#define HTTPBUFSIZE 64000

#define HTTP_SCHEME "http://"
#define HTTP_VER "HTTP/1.0"
#define DEFAULT_PORT "80"
#define CRLF "\r\n"

#define STRLLEN(strl) sizeof strl - 1

static const char *ignored_headers[] = {"Connection", "Proxy-Connection",
					"User-Agent"};
static const char *proxy_headers[] = {
    "Connection: close\r\n", "Proxy-Connection: close\r\n",
    "User-Agent: "
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/108.0.0.0 Safari/537.36\r\n"};

int parse_request(char *src, char *dst, char *hostname, char *port) {
	char *rp = src, *wp = dst;

	// trim leading whitespace
	while (isspace(*rp))
		++rp; // rp at http method

	// copy http method
	while (!isspace(*rp))
		*wp++ = *rp++;

	// advance
	rp = strstr(rp, HTTP_SCHEME) + STRLLEN(HTTP_SCHEME); // rp at hostname
	*wp++ = ' '; // wp ready for abs_path

	// copy hostname
	while (!strchr(":/ ", *rp))
		*hostname++ = *rp++;
	*hostname = 0;

	// copy port or set default
	if (*rp == ':') {
		++rp;
		while (!strchr("/ ", *rp))
			*port++ = *rp++;
		*port = 0;
	} else {
		strcpy(port, DEFAULT_PORT);
	}

	// copy abs_path or set default
	if (*rp == '/') {
		while (!isspace(*rp))
			*wp++ = *rp++;
	} else {
		*wp++ = '/';
	}

	// advance
	rp = strstr(rp, CRLF) + STRLLEN(CRLF); // rp at headers
	*wp++ = ' ';			       // wp ready for http version

	// copy http version
	for (char *p = HTTP_VER, *end = HTTP_VER + STRLLEN(HTTP_VER); p != end;)
		*wp++ = *p++;

	// append CRLF
	for (char *p = CRLF, *end = CRLF + STRLLEN(CRLF); p != end;)
		*wp++ = *p++; // wp ready for headers

	// process headers
	while (strncmp(rp, CRLF, STRLLEN(CRLF))) {
		// todo collect Content-Length

		// check for ignored headers
		int skip = 0;
		for (int i = 0, len = (sizeof ignored_headers /
				       sizeof *ignored_headers);
		     !skip && i < len; ++i) {
			if (!strncmp(rp, ignored_headers[i],
				     strlen(ignored_headers[i])))
				skip = 1;
		}
		if (skip) {
			rp = strstr(rp, CRLF) +
			     STRLLEN(CRLF); // rp at next header
			continue;
		}

		// copy until next header
		char *end = strstr(rp, CRLF) + STRLLEN(CRLF);
		while (rp != end)
			*wp++ = *rp++;
	}

	// append proxy headers
	for (int i = 0, len = (sizeof proxy_headers / sizeof *ignored_headers);
	     i < len; ++i) {
		for (const char *
			 p = proxy_headers[i],
			*end = proxy_headers[i] + strlen(proxy_headers[i]);
		     p != end;)
			*wp++ = *p++;
	}

	// append null line
	for (char *p = CRLF, *end = CRLF + STRLLEN(CRLF); p != end;)
		*wp++ = *p++; // wp ready for body

	// todo copy body

	// terminate string
	*wp = 0;

	// return new len
	int len = wp - dst;
	return len;
}

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

/**
 * Reads bytes into the provided buffer until the buffer is full, EOF, or error.
 * Returns -1 on error.
 */
int recv_all(int connfd, char *buf, int capacity) {
	int bytes_read, total = 0;
	while ((bytes_read = read(connfd, buf + total, capacity - total))) {
		if (bytes_read < 0) {
			return -1;
		}
		total += bytes_read;
	}

	if (total == capacity) {
		printf("WARNING: buffer limit reached\n");
	}

	return total;
}

/**
 * Reads bytes into the provided buffer until the request is terminated with a
 * null line, EOF, buf capacity is reached, or error. Returns -1 on error.
 */
int recv_http_request(int connfd, char *buf, int capacity) {
	// TODO handle http request body
	int bytes_read, total = 0;
	while ((bytes_read = read(connfd, buf + total, capacity - total))) {
		if (bytes_read < 0) {
			return -1;
		}
		total += bytes_read;
		if (!strncmp(buf + total - 4, "\r\n\r\n", 4))
			break;
	}

	if (total == capacity) {
		printf("WARNING: buffer limit reached\n");
	}

	return total;
}

/**
 * Sends bytes from the provided buffer until error or all bytes have been sent.
 * Returns -1 on error.
 */
int send_all(int connfd, char *buf, int len) {
	int bytes_written, total = 0;
	while ((bytes_written = write(connfd, buf + total, len - total))) {
		if (bytes_written < 0) {
			return -1;
		}
		total += bytes_written;
	}
	return total;
}

int main() {
	int listenfd = SOCK_UNSET;

	listenfd = getlistenfd("8080");

	while (1) {
		int connfd = SOCK_UNSET, clientfd = SOCK_UNSET;
		char connaddrbuf[INET6_ADDRSTRLEN] = {0};

		int bytes_received, bytes_sent;
		char buf[HTTPBUFSIZE] = {0};
		char parsed[HTTPBUFSIZE] = {0};
		char host[512] = {0};
		char port[8] = {0};

		// accept a client connection
		connfd = getconnfd(listenfd, connaddrbuf, sizeof connaddrbuf);
		if (connfd < 0) {
			fprintf(stderr, "getconnfd: %s\n", strerror(errno));
			continue; // no open sockets at this point
		}
		printf("***%s CONNECTED TO PROXY***\n", connaddrbuf);

		// read req from client
		bytes_received = recv_http_request(connfd, buf, sizeof buf);
		if (bytes_received < 0) {
			fprintf(stderr, "recv_http_request: %s\n",
				strerror(errno));
			goto close_connection;
		}

		// parse request
		int parsed_len = parse_request(buf, parsed, host, port);
		if (parsed_len >= HTTPBUFSIZE) {
			fprintf(stderr,
				"BUFFER OVERRUN: copied %d bytes to size %d\n",
				parsed_len, HTTPBUFSIZE);
			exit(1);
		}

		printf("%s %s\n", host, port);
		printf("raw:\n%s\n", buf);
		printf("parsed:\n%s\n", parsed);

		// connect to remote host
		clientfd = getclientfd(host, port);
		if (clientfd < 0) {
			fprintf(stderr, "getclientfd: %s\n", strerror(errno));
			goto close_connection;
		}

		// send req to remote host
		bytes_sent = send_all(clientfd, parsed, parsed_len);
		if (bytes_sent < 0) {
			fprintf(stderr, "send_all: %s\n", strerror(errno));
			goto close_connection;
		}

		// read res from remote host
		bytes_received = recv_all(clientfd, buf, sizeof buf);
		if (bytes_received < 0) {
			fprintf(stderr, "recv_all: %s\n", strerror(errno));
			goto close_connection;
		}

		// forward res to client
		bytes_sent = send_all(connfd, buf, bytes_received);
		if (bytes_sent < 0) {
			fprintf(stderr, "send_all: %s\n", strerror(errno));
			goto close_connection;
		}

	close_connection:
		printf("***%s DISCONNECTED FROM PROXY***\n", connaddrbuf);
		if (clientfd != SOCK_UNSET)
			close(clientfd);
		if (connfd != SOCK_UNSET)
			close(connfd);
	}

	exit(0);
}
