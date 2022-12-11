#ifndef __CPROXY_NETHELPERS_H__
#define __CPROXY_NETHELPERS_H__

#include <sys/socket.h>

/**
 * From Beej's Guide to Network Programming.
 *
 * Helper function that consumes a sockaddr_storage, and
 * returns the address of the sin(6?)_addr
 */
void *getinaddr(struct sockaddr_storage *ss);

/**
 * Get a socket listener or die trying
 */
int getlistenfd(const char *port);

/**
 * Block while waiting for a client. Returns -1 on error, or the connected
 * socket fd if successful. addrbuf is optional, and will be filled in with an
 * address string a la inet_ntop(3)
 */
int getconnfd(int listenfd, char *addrbuf, socklen_t addrlen);

/**
 * Get a connection to a remote host. Returns -1 on error.
 */
int getclientfd(const char *host, const char *service);

#endif
