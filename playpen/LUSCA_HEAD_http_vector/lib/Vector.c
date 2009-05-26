#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "../include/config.h"
#include "../include/util.h"
#include "Vector.h"

static int
vector_grow(vector_t *v, int new_count)
{
	void *t;

	if (new_count < v->alloc_count)
		return 0;

	t = xrealloc(v->data, (v->obj_size * new_count));
	if (t == NULL)
		return 0;

	v->data = t;
	v->alloc_count = new_count;
	return 1;
}

/*
 * Setup a vector. We don't ever fail here - if allocation fails
 * then "get" will return NULL.
 */
void
vector_init(vector_t *v, int obj_size, int obj_count)
{
	v->obj_size = obj_size;
	v->alloc_count = 0;
	v->used_count = 0;
	v->data = NULL;
	(void) vector_grow(v, obj_count);
}

void
vector_done(vector_t *v)
{
	v->used_count = 0;
	v->alloc_count = 0;
	if (v->data)
		xfree(v->data);
	v->data = NULL;
}


void *
vector_get_real(const vector_t *v, int offset)
{
	if (offset > v->used_count)
		return NULL;
	if (v->data == NULL)
		return NULL;

	return ((char *) v->data + (v->obj_size * offset));
}

void *
vector_append(vector_t *v)
{
	int offset;
	if (v->used_count == v->alloc_count)
		(void) vector_grow(v, v->alloc_count + 16);

	if (v->used_count == v->alloc_count)
		return NULL;
	offset = v->used_count;
	v->used_count++;
	return ((char *) v->data + (v->obj_size * offset));
}

void *
vector_insert(vector_t *v, int offset)
{
	int position = offset;

	if (position >= v->alloc_count)
		(void) vector_grow(v, v->alloc_count + 1);

	/* If we're asked to insert past the end of the list, just turn this into an effective append */
	if (position > v->used_count)
		position = v->used_count;

	/* Relocate the rest */
	if (position < v->alloc_count)
		memmove((char *) v->data + (position + 1) * v->obj_size,
		    (char *) v->data + position * v->obj_size, (v->used_count - position) * v->obj_size);
	v->used_count++;
	return ((char *) v->data + (v->obj_size * position));
}

int
vector_copy_item(vector_t *v, int dst, int src)
{
	if (dst >= v->used_count)
		return -1;
	if (src >= v->used_count)
		return -1;
	memcpy( (char *) v->data + (dst) * v->obj_size, 
		(char *) v->data + (src) * v->obj_size, 
		v->obj_size );
	return 1;
}
