#include "bufio.h"
#include "nethelpers.h"
#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SOCK_UNSET -1
#define HTTPBUFSIZE 64000

#define HTTP_SCHEME "http://"
#define HTTP_VER "HTTP/1.0"
#define DEFAULT_PORT "80"
#define CRLF "\r\n"
#define ERR_NOT_IMPLEMENTED "HTTP/1.0 501 Not Implemented\r\n\r\n"
#define ERR_BAD_REQUEST "HTTP/1.0 400 BAD REQUEST\r\n\r\n"

#define STRLLEN(strl) sizeof strl - 1

typedef struct vargp_s {
	int connfd;
	char connaddrbuf[INET6_ADDRSTRLEN];
} vargp_t;

static const char *ignored_headers[] = {"Connection", "Proxy-Connection",
					"User-Agent"};
static const char *proxy_headers[] = {
    "Connection: close\r\n", "Proxy-Connection: close\r\n",
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36\r\n"};

ssize_t parse_proxy_request(char *src, char *dst, char *hostname, char *port) {
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

ssize_t read_proxy_request(struct buffered_fd_t *bfd, char *buf,
			   size_t capacity) {
	char *wp = buf;
	ssize_t count;

	// read reqline
	count = buffered_readline(bfd, wp, capacity);
	if (count < 0)
		return -1;

	// check for valid request line
	char method[16];
	char uri[512];
	char httpver[16];

	int scanned = sscanf(wp, "%s %s %s\r\n", method, uri, httpver);
	if (scanned < 3 || strncasecmp(uri, HTTP_SCHEME, strlen(HTTP_SCHEME))) {
		buffered_writen(bfd, ERR_BAD_REQUEST, strlen(ERR_BAD_REQUEST));
		return -1;
	}

	// only support GET method for now
	if (strcasecmp(method, "GET")) {
		buffered_writen(bfd, ERR_NOT_IMPLEMENTED,
				strlen(ERR_NOT_IMPLEMENTED));
		return -1;
	}

	wp += count;
	capacity -= count;

	// get headers
	while (strcmp(wp - count, CRLF)) {
		count = buffered_readline(bfd, wp, capacity);
		if (count < 0)
			return -1;

		wp += count;
		capacity -= count;
	}

	return wp - buf;
}

void *thread_start(void *vargp) {
	// start up
	pthread_detach(pthread_self());
	vargp_t *args = vargp;

	// main code here
	printf("***%s CONNECTED TO PROXY***\n", args->connaddrbuf);

	struct buffered_fd_t bufconnfd, bufclientfd;
	ssize_t bytes_received, bytes_sent;
	int clientfd = SOCK_UNSET;

	char buf[HTTPBUFSIZE];
	char parsed[HTTPBUFSIZE];
	char host[512];
	char port[8];

	buffered_fd_init(&bufconnfd, args->connfd);

	// read req from client
	bytes_received = read_proxy_request(&bufconnfd, buf, sizeof buf);
	if (bytes_received < 0) {
		fprintf(stderr, "read_proxy_request: %s\n", strerror(errno));
		goto close_connection;
	}

	// parse request
	int parsed_len = parse_proxy_request(buf, parsed, host, port);
	if (parsed_len >= HTTPBUFSIZE) {
		fprintf(stderr, "BUFFER OVERRUN: copied %d bytes to size %d\n",
			parsed_len, HTTPBUFSIZE);
		exit(1);
	}

	// TODO delete these printf statements
	printf("%s %s\n", host, port);
	printf("raw:\n%s\n", buf);
	printf("parsed:\n%s\n", parsed);

	// connect to remote host
	clientfd = getclientfd(host, port);
	if (clientfd < 0) {
		fprintf(stderr, "getclientfd: %s\n", strerror(errno));
		goto close_connection;
	}

	buffered_fd_init(&bufclientfd, clientfd);

	// send req to remote host
	bytes_sent = buffered_writen(&bufclientfd, parsed, parsed_len);
	if (bytes_sent < 0) {
		fprintf(stderr, "send_all: %s\n", strerror(errno));
		goto close_connection;
	}

	// read res from remote host
	bytes_received = buffered_readn(&bufclientfd, buf, sizeof buf);
	if (bytes_received < 0) {
		fprintf(stderr, "recv_all: %s\n", strerror(errno));
		goto close_connection;
	}

	// forward res to client
	bytes_sent = buffered_writen(&bufconnfd, buf, bytes_received);
	if (bytes_sent < 0) {
		fprintf(stderr, "send_all: %s\n", strerror(errno));
		goto close_connection;
	}

close_connection:
	printf("***%s DISCONNECTED FROM PROXY***\n", args->connaddrbuf);
	if (clientfd != SOCK_UNSET)
		close(clientfd);
	if (args->connfd != SOCK_UNSET)
		close(args->connfd);

	// end thread
	free(args);
	pthread_exit(NULL);
}

int main() {
	int listenfd = SOCK_UNSET;
	listenfd = getlistenfd("8080");

	while (1) {
		int status;
		pthread_t tid;
		vargp_t *args = malloc(sizeof(vargp_t));
		if (args == NULL) {
			fprintf(stderr, "malloc: %s\n", strerror(errno));
			exit(1);
		}

		// accept a client connection
		args->connfd = getconnfd(listenfd, args->connaddrbuf,
					 sizeof args->connaddrbuf);
		if (args->connfd < 0) {
			fprintf(stderr, "getconnfd: %s\n", strerror(errno));
			free(args);
			continue; // no open sockets at this point
		}

		status = pthread_create(&tid, NULL, thread_start, args);
		if (status != 0) {
			fprintf(stderr, "pthread_create: %s\n",
				strerror(errno));
			exit(1);
		}
	}

	if (listenfd != SOCK_UNSET)
		close(listenfd);

	exit(0);
}
