#ifndef __LIBCORE_DEBUG_H__
#define __LIBCORE_DEBUG_H__

#define MAX_DEBUG_SECTIONS 100
extern int debugLevels[MAX_DEBUG_SECTIONS];
extern int _db_level;

#define do_debug(SECTION, LEVEL) \
    ((_db_level = (LEVEL)) <= debugLevels[SECTION])
#define debug(SECTION, LEVEL) \
    !do_debug(SECTION, LEVEL) ? (void) 0 : _db_print

#if STDC_HEADERS
extern void
_db_print(const char *,...) PRINTF_FORMAT_ARG1;
#else
extern void _db_print();
#endif

#endif /* __LIBCORE_DEBUG_H__ */
