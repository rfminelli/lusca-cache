#ifndef	__LIBCORE_DLINK_H__
#define	__LIBCORE_DLINK_H__

/*
 * Derived from squid/src/typedefs.h ; squid/src/structs.h
 */

struct _dlink_node;
struct _dlink_tail;

typedef struct _dlink_node dlink_node;
typedef struct _dlink_list dlink_list;

struct _dlink_node {
    void *data;
    dlink_node *prev;
    dlink_node *next;
};

struct _dlink_list {
    dlink_node *head;
    dlink_node *tail;
};

#endif
