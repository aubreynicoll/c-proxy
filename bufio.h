#ifndef __CPROXY_BUFIO_H__
#define __CPROXY_BUFIO_H__

#include <stdlib.h>

/**
 * Default buffer of 8kb, but allow for user override
 */
#ifndef BUFFERED_FD_T_BUFSIZE
#define BUFFERED_FD_T_BUFSIZE 8192
#endif

/**
 * Type used for the buffered reading of a file descriptor
 */
struct buffered_fd_t {
	int fd;
	ssize_t remaining;
	char *bufp;
	char buf[BUFFERED_FD_T_BUFSIZE];
};

/**
 * Initialize a buffered_fd_t
 */
void buffered_fd_init(struct buffered_fd_t *bfd, int fd);

/**
 * Reads up to one newline char from a buffered fd. Returns -1 on error, 0 on
 * EOF, or number of bytes read. The destination buffer will be null-terminated.
 * Once a buffered_fd_t is read from, one should not read directly from the
 * original fd, as that would result in missing bytes.
 */
ssize_t buffered_readline(struct buffered_fd_t *bfd, char *dst,
			  size_t capacity);

/**
 * Reads up to n bytes from a buffered fd. Returns -1 on error, 0 on EOF, or
 * number of bytes read. Once a buffered_fd_t is read from, one should not read
 * directly from the original fd, as that would result in missing bytes
 */
ssize_t buffered_readn(struct buffered_fd_t *bfd, char *dst, size_t n);

/**
 * Writes up to n bytes to a buffered fd. Returns -1 on error, or 0 if
 * successful. This is kind of an unnecessary function in that one could still
 * write directly to the fd and it wouldn't interfere with the internal
 * buffering operations, but it is included to provide a more complete API for
 * buffered_fd_t.
 */
ssize_t buffered_writen(struct buffered_fd_t *bfd, char *src, size_t n);

#endif
