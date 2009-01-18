#include "squid.h"

int
buf_fill(buf_t *b, int fd, int grow_size)
{
	int ret;

	/* extend buffer to have enough space */
	buf_grow_to_min_free(b, grow_size);

	/* read into empty space */
	ret = FD_READ_METHOD(fd, buf_buf(b) + buf_len(b), buf_capacity(b) - buf_len(b));

	/* error/eof? return */
	if (ret <= 0)
		return ret;

	/* update counters */
	/* XXX do them! */

	/* update buf_t */
	b->len += ret;

	/* return size */
	return ret;
}
