
/*
 * $Id$
 *
 * DEBUG: section 0     Debug Routines
 * AUTHOR: Harvest Derived
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

static char *debug_log_file = NULL;
#if HAVE_SYSLOG
static void _db_print_syslog(const char *format, va_list args);
#endif
static void _db_print_file(const char *format, va_list args);

#ifdef _SQUID_MSWIN_
extern LPCRITICAL_SECTION dbg_mutex;
#endif

#ifdef _SQUID_LINUX_
/* Workaround for crappy glic header files */
extern int backtrace(void *, int);
extern void backtrace_symbols_fd(void *, int, int);
extern int setresuid(uid_t, uid_t, uid_t);
#endif /* _SQUID_LINUX */

static int debug_log_dirty = 0;
int
debug_log_flush(void)
{
    static time_t last_flush = 0;
    if (!debug_log_dirty)
	return 0;
    if (last_flush != squid_curtime) {
	fflush(debug_log);
	last_flush = squid_curtime;
	debug_log_dirty = 0;
    }
    return debug_log_dirty;
}

static void
_db_print_file(const char *format, va_list args)
{
    if (debug_log == NULL)
	return;
    /* give a chance to context-based debugging to print current context */
    if (!Ctx_Lock)
	ctx_print();
    vfprintf(debug_log, format, args);
    if (!Config.onoff.buffered_logs)
	fflush(debug_log);
    else
	debug_log_dirty++;
}

#if HAVE_SYSLOG
static void
_db_print_syslog(const char *format, va_list args)
{
    LOCAL_ARRAY(char, tmpbuf, BUFSIZ);
    /* level 0,1 go to syslog */
    if (_db_level > 1)
	return;
    if (0 == opt_syslog_enable)
	return;
    tmpbuf[0] = '\0';
    vsnprintf(tmpbuf, BUFSIZ, format, args);
    tmpbuf[BUFSIZ - 1] = '\0';
    syslog((_db_level == 0 ? LOG_WARNING : LOG_NOTICE) | syslog_facility, "%s", tmpbuf);
}
#endif /* HAVE_SYSLOG */

static void
debugOpenLog(const char *logfile)
{
    if (logfile == NULL) {
	debug_log = stderr;
	return;
    }
    safe_free(debug_log_file);
    debug_log_file = xstrdup(logfile);	/* keep a static copy */
    if (debug_log && debug_log != stderr)
	fclose(debug_log);
    debug_log = fopen(logfile, "a+");
    if (!debug_log) {
	fprintf(stderr, "WARNING: Cannot write log file: %s\n", logfile);
	perror(logfile);
	fprintf(stderr, "         messages will be sent to 'stderr'.\n");
	fflush(stderr);
	debug_log = stderr;
    }
#ifdef _SQUID_WIN32_
    setmode(fileno(debug_log), O_TEXT);
#endif
}

#if HAVE_SYSLOG
void
_db_set_syslog(const char *facility)
{
    opt_syslog_enable = 1;
#ifdef LOG_LOCAL4
#ifdef LOG_DAEMON
    syslog_facility = LOG_DAEMON;
#else
    syslog_facility = LOG_LOCAL4;
#endif
    if (facility) {
        syslog_facility = syslog_ntoa(facility);
        if (syslog_facility != 0)
	    return;
         
	fprintf(stderr, "unknown syslog facility '%s'\n", facility);
	exit(1);
    }
#else
    if (facility)
	fprintf(stderr, "syslog facility type not supported on your system\n");
#endif
}
#endif

void
_db_init_log(const char *logfile)
{
    _db_register_handler(_db_print_file, 1);
#if HAVE_SYSLOG
    _db_register_handler(_db_print_syslog, 0);
#endif
    debugOpenLog(logfile);
}

void
_db_rotate_log(void)
{
    int i;
    LOCAL_ARRAY(char, from, MAXPATHLEN);
    LOCAL_ARRAY(char, to, MAXPATHLEN);
#ifdef S_ISREG
    struct stat sb;
#endif

    if (debug_log_file == NULL)
	return;
#ifdef S_ISREG
    if (stat(debug_log_file, &sb) == 0)
	if (S_ISREG(sb.st_mode) == 0)
	    return;
#endif

    /*
     * NOTE: we cannot use xrename here without having it in a
     * separate file -- tools.c has too many dependencies to be
     * used everywhere debug.c is used.
     */
    /* Rotate numbers 0 through N up one */
    for (i = Config.Log.rotateNumber; i > 1;) {
	i--;
	snprintf(from, MAXPATHLEN, "%s.%d", debug_log_file, i - 1);
	snprintf(to, MAXPATHLEN, "%s.%d", debug_log_file, i);
#ifdef _SQUID_MSWIN_
	remove(to);
#endif
	rename(from, to);
    }
/*
 * You can't rename open files on Microsoft "operating systems"
 * so we close before renaming.
 */
#ifdef _SQUID_MSWIN_
    if (debug_log != stderr)
	fclose(debug_log);
#endif
    /* Rotate the current log to .0 */
    if (Config.Log.rotateNumber > 0) {
	snprintf(to, MAXPATHLEN, "%s.%d", debug_log_file, 0);
#ifdef _SQUID_MSWIN_
	remove(to);
#endif
	rename(debug_log_file, to);
    }
    /* Close and reopen the log.  It may have been renamed "manually"
     * before HUP'ing us. */
    if (debug_log != stderr)
	debugOpenLog(Config.Log.log);
}

