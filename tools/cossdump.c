#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>

#include "../src/defines.h"
#include "../src/enums.h"

struct _tlv;
typedef struct _tlv tlv;

struct _tlv {
    char type;
    int length;
    void *value;
    tlv *next;
};

#undef debug
#define	debug(a, b)	printf

#define	MEM_TLV	sizeof(tlv)
#define	memAllocate(a)	malloc(a)
#define	memFree(a, b)	free(a)
#define xmemcpy(a, b, c) memcpy(a, b, c)
#define xmalloc(a) malloc(a)
#define xfree(a) free(a)

#define squid_off_t off_t

static tlv **
storeSwapTLVAdd(int type, const void *ptr, size_t len, tlv ** tail)
{
    tlv *t = memAllocate(MEM_TLV);
    t->type = (char) type;
    t->length = (int) len;
    t->value = xmalloc(len);
    xmemcpy(t->value, ptr, len);
    *tail = t;
    return &t->next;		/* return new tail pointer */
}

void
storeSwapTLVFree(tlv * n)
{
    tlv *t;
    while ((t = n) != NULL) {
	n = t->next;
	xfree(t->value);
	memFree(t, MEM_TLV);
    }
}

char *
storeSwapMetaPack(tlv * tlv_list, int *length)
{
    int buflen = 0;
    tlv *t;
    int j = 0;
    char *buf;
    assert(length != NULL);
    buflen++;			/* STORE_META_OK */
    buflen += sizeof(int);	/* size of header to follow */
    for (t = tlv_list; t; t = t->next)
	buflen += sizeof(char) + sizeof(int) + t->length;
    buflen++;			/* STORE_META_END */
    buf = xmalloc(buflen);
    buf[j++] = (char) STORE_META_OK;
    xmemcpy(&buf[j], &buflen, sizeof(int));
    j += sizeof(int);
    for (t = tlv_list; t; t = t->next) {
	buf[j++] = (char) t->type;
	xmemcpy(&buf[j], &t->length, sizeof(int));
	j += sizeof(int);
	xmemcpy(&buf[j], t->value, t->length);
	j += t->length;
    }
    buf[j++] = (char) STORE_META_END;
    assert((int) j == buflen);
    *length = buflen;
    return buf;
}

tlv *
storeSwapMetaUnpack(const char *buf, int *hdr_len)
{
    tlv *TLV;			/* we'll return this */
    tlv **T = &TLV;
    char type;
    int length;
    int buflen;
    int j = 0;
    assert(buf != NULL);
    assert(hdr_len != NULL);
    if (buf[j++] != (char) STORE_META_OK)
	return NULL;
    xmemcpy(&buflen, &buf[j], sizeof(int));
    j += sizeof(int);
    /*
     * sanity check on 'buflen' value.  It should be at least big
     * enough to hold one type and one length.
     */
    if (buflen <= (sizeof(char) + sizeof(int)))
	    return NULL;
    while (buflen - j > (sizeof(char) + sizeof(int))) {
	type = buf[j++];
	/* VOID is reserved, but allow some slack for new types.. */
	if (type <= STORE_META_VOID || type > STORE_META_END + 10) {
	    debug(20, 0) ("storeSwapMetaUnpack: bad type (%d)!\n", type);
	    break;
	}
	xmemcpy(&length, &buf[j], sizeof(int));
	if (length < 0 || length > (1 << 16)) {
	    debug(20, 0) ("storeSwapMetaUnpack: insane length (%d)!\n", length);
	    break;
	}
	j += sizeof(int);
	if (j + length > buflen) {
	    debug(20, 0) ("storeSwapMetaUnpack: overflow!\n");
	    debug(20, 0) ("\ttype=%d, length=%d, buflen=%d, offset=%d\n",
		type, length, buflen, (int) j);
	    break;
	}
	T = storeSwapTLVAdd(type, &buf[j], (size_t) length, T);
	j += length;
    }
    *hdr_len = buflen;
    return TLV;
}


#define	STRIPESIZE 1048576
#define	BLOCKSIZE 1024
#define BLKBITS 10

void
parse_stripe(int stripeid, char *buf, int len)
{
	int j = 0;
	int o = 0;
	int bl = 0;
	tlv *t, *tlv_list;
	int64_t *l;
	int tmp;

	while (j < len) {
		l = NULL;
		bl = 0;
		tlv_list = storeSwapMetaUnpack(&buf[j], &bl);
		if (tlv_list == NULL) {
			printf("  Object: NULL\n");
			return;
		}
		printf("  Object: (filen %d) hdr size %d\n", j / BLOCKSIZE + (stripeid * STRIPESIZE / BLOCKSIZE), bl);
		for (t = tlv_list; t; t = t->next) {
			switch(t->type) {
				case STORE_META_URL:
					printf("    URL: %s\n", t->value);
					break;
				case STORE_META_OBJSIZE:
					l = t->value;
					printf("Size: %lld (len %d)\n", *l, t->length);
					break;
			}
		}
		if (l == NULL) {
			printf("  STRIPE: Completed, got an object with no size\n");
			return;
		}
		j = j + *l + bl;
		/* And now, the blocksize! */
		tmp = j / BLOCKSIZE;
		tmp = (tmp+1) * BLOCKSIZE;
		j = tmp;
	}
}

int
main(int argc, char *argv[])
{
	int fd;
	char buf[STRIPESIZE];
	int i = 0, len;

	if (argc < 2) {
		printf("Usage: %s <path to COSS datafile>\n", argv[0]);
		exit(1);
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("open");
		exit(1);
	}
	while ((len = read(fd, buf, STRIPESIZE)) > 0) {
		printf("STRIPE: %d (len %d)\n", i, len);
		parse_stripe(i, buf, len);
		i++;
	}
}
