/*
 * $Id$
 *
 * DEBUG: section ??    HTTP Status-line
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

#include "squid.h"


/* local constants */

/* local routines */
static char *httpStatusString(http_status status);

void
httpStatusLineInit(HttpStatusLine *sline) {
    httpStatusLineSet(sline, 0.0, 500, NULL);
}

void
httpStatusLineClean(HttpStatusLine *sline) {
    httpStatusLineSet(sline, 0.0, 500, NULL);
}

void httpStatusLineSet(HttpStatusLine *sline, double version, http_status status, const char *reason) {
    assert(sline);
    /* we must ensure that version occupies 3 characters only @?@ */
    if (sline -> version < 0.0 || sline -> version > 9.9)
	sline -> version = 0.1; /* 0.0 will clash with unset value */

    sline->version = version;
    sline->status = status;
    /* Note: no xstrdup for reason, assumes constant reasons @?@ */
    sline->reason = reason ? reason : httpStatusString(status);
    sline->packed_size =
	5 + /* HTTP/ */
	3 + /* version */
	1 + /* space */
	3 + /* status */
	1 + /* space */
	strlen(reason) +
	2 + /* CRLF */
	1;  /* terminating 0 */
}

void
httpStatusLineSwap(HttpStatusLine *sline, StoreEntry *e) {    
    assert(sline && e);
    storeAppendPrintf(e, "HTTP/%3.1f %d %s\r\n",
	sline->version, sline->status, sline->reason);
}

int
httpStatusLineParse(HttpStatusLine *sline, const char *start, const char *end) {
    /* @?@ implement it */
    assert(sline);
    assert(0);
    return 0;
}

static char *
httpStatusString(http_status status)
{
    /* why not to return matching string instead of using "p" ? @?@ */
    char *p = NULL;
    switch (status) {
    case 100:
	p = "Continue";
	break;
    case 101:
	p = "Switching Protocols";
	break;
    case 200:
	p = "OK";
	break;
    case 201:
	p = "Created";
	break;
    case 202:
	p = "Accepted";
	break;
    case 203:
	p = "Non-Authoritative Information";
	break;
    case 204:
	p = "No Content";
	break;
    case 205:
	p = "Reset Content";
	break;
    case 206:
	p = "Partial Content";
	break;
    case 300:
	p = "Multiple Choices";
	break;
    case 301:
	p = "Moved Permanently";
	break;
    case 302:
	p = "Moved Temporarily";
	break;
    case 303:
	p = "See Other";
	break;
    case 304:
	p = "Not Modified";
	break;
    case 305:
	p = "Use Proxy";
	break;
    case 400:
	p = "Bad Request";
	break;
    case 401:
	p = "Unauthorized";
	break;
    case 402:
	p = "Payment Required";
	break;
    case 403:
	p = "Forbidden";
	break;
    case 404:
	p = "Not Found";
	break;
    case 405:
	p = "Method Not Allowed";
	break;
    case 406:
	p = "Not Acceptable";
	break;
    case 407:
	p = "Proxy Authentication Required";
	break;
    case 408:
	p = "Request Time-out";
	break;
    case 409:
	p = "Conflict";
	break;
    case 410:
	p = "Gone";
	break;
    case 411:
	p = "Length Required";
	break;
    case 412:
	p = "Precondition Failed";
	break;
    case 413:
	p = "Request Entity Too Large";
	break;
    case 414:
	p = "Request-URI Too Large";
	break;
    case 415:
	p = "Unsupported Media Type";
	break;
    case 500:
	p = "Internal Server Error";
	break;
    case 501:
	p = "Not Implemented";
	break;
    case 502:
	p = "Bad Gateway";
	break;
    case 503:
	p = "Service Unavailable";
	break;
    case 504:
	p = "Gateway Time-out";
	break;
    case 505:
	p = "HTTP Version not supported";
	break;
    default:
	p = "Unknown";
	debug(11, 0) ("Unknown HTTP status code: %d\n", status);
	break;
    }
    return p;
}
