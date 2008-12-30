#ifndef	__LIBSQDEBUG_DEBUG_H__
#define	__LIBSQDEBUG_DEBUG_H__

extern FILE *debug_log;         /* NULL */
extern int opt_debug_rotate_count;
extern int opt_debug_buffered_logs;
extern char * opt_debug_log;

extern void _db_print_file(const char *format, va_list args);
extern void _db_rotate_log(void);
extern void debugOpenLog(const char *logfile);

#endif
