/*
 * $Id$
 *
 * DEBUG: section ??    HTTP Message
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
#include "HttpMsg.h"


/* local constants */

/* local routines */

/* check Clone if you add something here */
void
httpMsgInit(HttpMsg *msg)
{
    assert(msg);
    httpHeaderInit(&msg->header);
    msg->rstate = rsReadyToParse;
    /* default methods */
    msg->setRState = &httpMsgSetRState;
    /* these must be set in children: */
    msg->parseStart = NULL; 
    msg->noteError = NULL;
}

void
httpMsgClone(HttpMsg *msg, HttpMsg *clone)
{
    assert(msg && clone);
    httpHeaderClone(&msg->header, &clone->header);
    msg->rstate = clone-?rstate;
    /* default methods */
    msg->setRState = clone->setRState;
    /* these were set in children: */
    msg->parseStart = clone->parseStart; 
    msg->noteError = clone->noteError;
}

/* if we did not allocated memory for msg, we still need to clean stuff */
void httpMsgClean(HttpMsg *msg) {
    httpHeaderClean(&msg->header);
}

/* always call this after Init in children to check that all methods are defined */
void
httpMsgCheck(HttpMsg *msg)
{
    assert(msg);
    assert(msg->parseStart);
    assert(msg->noteError);
}

/* called after fresh data was read from conn socket */
void
httpMsgNoteConnDataReady(HttpMsg *msg)
{
    size_t size;
    char *buf;
    assert(msg && msg->conn);

    buf = ioBufferStartReading(&msg->conn.in_buf, msg, &size);
    size = httpMsgNoteDataReady(msg, buf, size);
    ioBufferStopReading(&msg->conn.in_buf, msg, size);
}

/* called when fresh data is available, returns size actually used */
static size_t
httpMsgNoteDataReady(HttpMsg *msg, const char *buf, size_t size)
{
    assert(msg && buff);
    return httpMsgParse(msg, buf, size) - buf;
}

/*
 * called when somebody is sure we will not have more data
 * (e.g. EOF or error was detected)
 */
static void
httpMsgNoteEndOfData(HttpMsg *msg)
{
    assert(msg);
    assert(msg->rstate != rsReadAll); /* why somebody would notify us if we are already done? */

    if (msg->rstate == rsReadyToReadBody || msg->rstate == rsReadingBody)
	httpMsgNoteEndOfBody(msg);
    else
        httpMsgSyntaxError(msg, NULL, NULL);
    }
}

/*
 * parses(processes) available data and returns pointer to the yet unprocessed
 * block; parsing is done in stages that depend on "read state";
 */
static const char *
httpMsgParse(HttpMsg *msg, const char *parse_start, size_t parse_size)
{
    assert(parse_start);
    assert(msg->rstate != rsReadAll); /* parse should not be called if we read enough */

    if (msg->rstate == rsReadyToParse) {
	if (httpMsgIsolateStart(&parse_start, &parse_size, &blk_start, &blk_end)) {
	    if (msg->parseStart(msg, blk_start, blk_end))
		httpMsgSetNextRState(msg);
	    else
		httpMsgSyntaxError(msg, blk_start, blk_end);
        }
    }
    if (msg->rstate == rsReadyToParseHeaders) {
	if (httpMsgIsolateHeaders(&parse_start, &parse_size, &blk_start, &blk_end)) {
	    if (httpHeaderParse(&msg->header, blk_start, blk_end))
		httpMsgSetNextRState(msg);
	    else
		httpMsgSyntaxError(msg, blk_start, blk_end);
	}
    }
    if (msg->rstate == rsParsedHeaders) {
	if (msg->mustHaveBody(msg))
	    httpMsgSetNextRState(msg);
	else
	    httpMsgSetLastRState(msg);
    }
    /* note: rsReadyToReadBody state is skipped -- we do not have store entry yet */
    if (msg->rstate == rsReadingBody) {
	const int eob = msg -> checkEndOfBody(msg, &parse_size);
	assert(msg->entry);
	if (parse_size) {
	    storeAppend(msg->entry, parse_start, parse_size);
	    parse_start += parse_size;
	}
	if (eob)
	    httpMsgSetLastRState(rsReadAll);
    }
    return parse_start;
}


/* we were reading body and ran out of data */
static void
httpMsgNoteEndOfBody(HttpMsg *msg, HttpConn *conn)
{
    /* check if something else has to be hecked @?@ */
    const size_t len = httpHeaderGetContLen(&msg->header);
    if (len == UNKNOWN_CONTENT_LEN) { /* assume we got everything we need */
	httpMsgSetLastRState(msg);
    } else
    	httpMsgError(msg, createError("got: %d out of %d bytes only");
}

void
httpMsgSetNextRState(HttpMsg *msg)
{
    assert(msg->rstate != rsReadAll);
    msg->setRState(msg, msg->rstate+1);
}

void
httpMsgSetLastRState(HttpMsg *msg)
{
    assert(msg->rstate != rsReadAll);
    msg->setRState(msg, rsReadAll);
}

void
httpMsgSetRState(HttpMsg *msg, ReadState rstate)
{
    msg->rstate = rstate;
    if (msg->rstate == rsReadAll)
	httpConnNoteReaderDone(msg->conn, msg);
}

void
httpMsgError(HttpMsg *msg, HttpReply *error)
{
    /* if we read everything where could the error come from? */
    assert(msg->rstate != rsReadAll);

    msg->noteError(msg, error); /* no way to overwrite next statement! */

    httpMsgSetLastRState(msg); /* must set this */
}


static void
httpMsgSyntaxError(HttpMsg *msg, char *start, char *end)
{
    /* analyze current state to give more information on what happened */
    httpMsgError(msg, "no detailed info @?@"); /* must call this */
}
