
/*
 * $Id: store_rebuild_coss.c 14048 2009-05-08 13:34:06Z adrian.chadd $
 *
 * DEBUG: section 47    Store COSS Directory Routines
 * AUTHOR: Eric Stern
 *
 * SQUID Web Proxy Cache          http://www.squid-cache.org/
 * ----------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from
 *  the Internet community; see the CONTRIBUTORS file for full
 *  details.   Many organizations have provided support for Squid's
 *  development; see the SPONSORS file for full details.  Squid is
 *  Copyrighted (C) 2001 by the Regents of the University of
 *  California; see the COPYRIGHT file for full details.  Squid
 *  incorporates software developed and/or copyrighted by other
 *  sources; see the CREDITS file for full details.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *
 */

#include "squid.h"

#include "../../libasyncio/aiops.h"
#include "../../libasyncio/async_io.h"
#include "store_coss.h"
#include "store_log_coss.h"

void
storeCossDirOpenSwapLog(SwapDir * sd)
{
}

void
storeCossDirCloseSwapLog(SwapDir * sd)
{
}

void
storeCossDirSwapLog(const SwapDir * sd, const StoreEntry * e, int op)
{
}
