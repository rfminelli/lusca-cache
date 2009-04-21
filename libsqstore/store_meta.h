#ifndef	__LIBSQSTORE_STORE_META_H__
#define	__LIBSQSTORE_STORE_META_H__

/*
 * For now these aren't used in the application itself; they're
 * designed to be used by other bits of code which are manipulating
 * store swap entries.
 */

struct _storeMetaIndexOld {
	time_t timestamp;
	time_t lastref;
	time_t expires;
	time_t lastmod;
	size_t swap_file_sz;
	u_short refcount;
	u_short flags;
};
typedef struct _storeMetaIndexOld storeMetaIndexOld;

struct _storeMetaIndexNew {
	time_t timestamp;
	time_t lastref;
	time_t expires;
	time_t lastmod;
	squid_file_sz swap_file_sz;
	u_short refcount;
	u_short flags;
};
typedef struct _storeMetaIndexNew storeMetaIndexNew;

#endif
