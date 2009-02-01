#include "squid.h"

int
buf_read(buf_t *b, int fd, int grow_size)
{
	int ret;

	/* extend buffer to have enough space */
	/* XXX for now, just make it grow 4k bytes at a time */
	buf_grow_to_min_free(b, 4096);

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

int
memBufFill(MemBuf *mb, int fd, int grow_size)
{
	int ret;

	/* We only grow the memBuf if its almost full */
	if (mb->capacity - mb->size < 1024)
		memBufGrow(mb, mb->capacity + grow_size);

	ret = FD_READ_METHOD(fd,  mb->buf + mb->size, mb->capacity - mb->size - 1);
	if (ret <= 0)
		return ret;
	mb->size += ret;
	assert(mb->size <= mb->capacity);
	mb->buf[mb->size] = '\0';
	return ret;
}
