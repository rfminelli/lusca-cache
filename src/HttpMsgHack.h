/*
 * $Id$
 *
 * AUTHOR: Alex Rousskov    Hack to share HttpMsg fields
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

/*
 * No ifdef stuff for this hack becuase we include it several times
 */

/*
 * this gets included at the beginning of HttpMsg, HttpReq, and HttpReply
 * structures
 */

/* struct _Http* { */
    /* public, writable (using corresponding interfaces) */
    HttpHeader header;
    HttpConn *conn;    /* the connection this came from if any */

    /* methods for "friends"; usually you do not call these directly */
    /* called when new data avalibale from conn */
    void (*noteConnDataReady)(HttpMsg *msg);
    /* called when new data us avalibale in a buffer */
    size_t (*noteBuffDataReady)(HttpMsg *msg, const char *buf, size_t size);
    /* called when space conn buffer is avalibale */
    void (*noteSpaceReady)(HttpMsg *msg);
    /* set new rstate, useful for hooks */
    void (*setRState)(HttpMsg *msg, ReadState rstate);
    /* parses the first line of an http message */
    int (*parseStart)(HttpMsg *msg, const char *start, const char *end);
    /* called on connection close() */
    void (*noteConnClosed)(HttpMsg *msg);
    /* called when [parsing] error is detected */
    void (*noteError)(HttpMsg *msg, HttpReply *error);
    /* called to do force destroy of an object */
    void (*noteException)(HttpMsg *msg, int status);
    /* called to do force destroy of an object */
    void (*destroy)(HttpMsg *msg);

    /* protected, do not use these, use interface functions instead */
    IOBuffer *buf;     /* comm | buf | store */
    StoreEntry *entry; /* body (if any) is always passed via store */
    ReadState rstate;  /* current read state */
/* }; */

