#ifndef	__LIBMEM_VECTOR_H__
#define	__LIBMEM_VECTOR_H__

struct _vector_t {
	int alloc_count;
	int used_count;
	int obj_size;
	void *data;
};
typedef struct _vector_t vector_t;

void vector_init(vector_t *v, int obj_size, int obj_count);
void vector_done(vector_t *v);
void * vector_get(vector_t *v, int offset);
void * vector_append(vector_t *v);
void * vector_insert(vector_t *v, int position);

#define	vector_numentries(v)	( (v)->used_count )
#define	vector_size(v)		( (v)->alloc_count )

#endif
