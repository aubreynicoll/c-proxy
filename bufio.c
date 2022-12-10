#include "bufio.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>

/* Static Function Declarations */
static ssize_t read_bfd(struct buffered_fd_t *bfd, char *dst, size_t n);

/* Function Definitions */
void buffered_fd_init(struct buffered_fd_t *bfd, int fd) {
	bfd->fd = fd;
	bfd->remaining = 0;
	bfd->bufp = bfd->buf;
}

ssize_t buffered_readline(struct buffered_fd_t *bfd, char *dst,
			  size_t capacity) {
	char *wp = dst;
	ssize_t count;

	--capacity; // leave room for null char

	while (capacity && (count = read_bfd(bfd, wp, 1))) {
		if (count < 0)
			return -1;

		if (*wp++ == '\n')
			break;

		--capacity;
	}

	*wp = 0;

	return wp - dst;
}

ssize_t buffered_readn(struct buffered_fd_t *bfd, char *dst, size_t n) {
	char *wp = dst;
	ssize_t count;

	while (n && (count = read_bfd(bfd, wp, n))) {
		if (count < 0)
			return -1;

		wp += count;
		n -= count;
	}

	return wp - dst;
}

ssize_t buffered_writen(struct buffered_fd_t *bfd, char *src, size_t n) {
	char *rp = src;
	ssize_t count;

	while (n && (count = write(bfd->fd, rp, n))) {
		if (count < 0)
			return -1;

		rp += count;
		n -= count;
	}

	return 0;
}

static ssize_t read_bfd(struct buffered_fd_t *bfd, char *dst, size_t n) {
	while (bfd->remaining <= 0) {
		// read from fd into bfd
		bfd->remaining = read(bfd->fd, bfd->buf, sizeof bfd->buf);

		if (bfd->remaining < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}

		if (bfd->remaining == 0)
			return 0;

		bfd->bufp = bfd->buf;
	}

	// copy from bfd to dst

	ssize_t bytes = (size_t)bfd->remaining < n ? (size_t)bfd->remaining : n;
	memcpy(dst, bfd->bufp, bytes);

	bfd->bufp += bytes;
	bfd->remaining -= bytes;

	return bytes;
}
