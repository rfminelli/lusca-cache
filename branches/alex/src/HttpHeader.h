/*
 * $Id$
 *
 * AUTHOR: Alex Rousskov
 *
 * SQUID Internet Object Cache  http://squid.nlanr.net/Squid/
 * --------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from the
 *  Internet community.  Development is led by Duane Wessels of the
 *  National Laboratory for Applied Network Research and funded by
 *  the National Science Foundation.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *  
 */

#ifndef _HTTP_HEADER_H_
#define _HTTP_HEADER_H_

struct _HttpHeaderField {
	char *name;   /* field-name from HTTP/1.1 (no column!) */
	char *value;  /* field-value from HTTP/1.1 */
};

struct _HttpHeader {
	/* public, read only */
	size_t packed_size;  /* packed header size (see httpHeaderPack()) */

	/* protected, do not use these, use interface functions instead */
	int count;        /* #headers */
	int capacity;     /* max #headers before we have to grow */
	struct _HttpHeaderField **fields;
};

typedef struct _HttpHeaderField HttpHeaderField;
typedef struct _HttpHeader HttpHeader;

/*
 * use HttpHeaderPos as opaque type, do not interpret, 
 * it is not what you think it is
 */
typedef size_t HttpHeaderPos; 

/* use this and only this to initialize HttpHeaderPos */
#define httpHeaderInitPos (-1)

/* create/init/destroy */
extern HttpHeader *httpHeaderCreate();
extern void httpHeaderDestroy(HttpHeader *hdr);

/* parse/pack */
/* parse a 0-terminating buffer and fill internal structires; _end points at the first character after the header; returns true if successfull */
extern int httpHeaderParse(HttpHeader *hdr, const char *header_start, const char *header_end);
/* pack header into the buffer, does not check for overflow, check hdr.packed_size first! */
extern void httpHeaderPack(HttpHeader *hdr, char *buf);

/* iterate through fields with name (or find first field with name) */
extern const char *httpHeaderGetStr(HttpHeader *hdr, const char *name, HttpHeaderPos *pos);
extern long httpHeaderGetInt(HttpHeader *hdr, const char *name, HttpHeaderPos *pos);

/* iterate through all fields */
extern HttpHeaderField *httpHeaderGetField(HttpHeader *hdr, const char **name, const char **value, HttpHeaderPos *pos);

/* delete field(s) by name or pos */
extern int httpHeaderDelFields(HttpHeader *hdr, const char *name);
extern void httpHeaderDelField(HttpHeader *hdr, HttpHeaderPos pos);

/* add a field (appends) */
extern const char *httpHeaderAddStrField(HttpHeader *hdr, const char *name, const char *value);
extern long httpHeaderAddIntField(HttpHeader *hdr, const char *name, long value);

/* put report about current header usage and other stats into a static string */
extern const char *httpHeaderReport();

#endif /* ndef _HTTP_HEADER_H_ */
