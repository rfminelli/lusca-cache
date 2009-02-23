#ifndef	__LIBSQBGP_ASPATH_H__
#define	__LIBSQBGP_ASPATH_H__

struct _aspath_entry {
	hash_link *link;
	int *aspath;
	int aspath_cnt;
	int refcnt;
};
typedef struct _aspath_entry aspath_entry_t;

struct _aspath_head {
	hash_table *h;
};
typedef struct _aspath_head aspath_head_t;

void aspath_head_init(aspath_head_t *h);
void aspath_head_free(aspath_head_t *h);

aspath_entry_t * aspath_create(u_int16_t *aspath, int aspath_cnt);
aspath_entry_t * aspath_get(u_int16_t *aspath, int aspath_cnt);
void aspath_deref(aspath_entry_t *a);

#endif
