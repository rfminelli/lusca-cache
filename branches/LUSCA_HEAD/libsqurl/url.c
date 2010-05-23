#include "../include/config.h"

#include <stdio.h>
#if HAVE_STRING_H
#include <string.h>
#endif

#include "../include/util.h"

#include "url.h"

char *
url_convert_hex(char *org_url, int allocate)
{
    static char code[] = "00";
    char *url = NULL;
    char *s = NULL;
    char *t = NULL;
    url = allocate ? (char *) xstrdup(org_url) : org_url;
    if ((int) strlen(url) < 3 || !strchr(url, '%'))
        return url;
    for (s = t = url; *s; s++) {
        if (*s == '%' && *(s + 1) && *(s + 2)) {
            code[0] = *(++s);
            code[1] = *(++s);
            *t++ = (char) strtol(code, NULL, 16);
        } else {
            *t++ = *s;
        }
    }
    do {
        *t++ = *s;
    } while (*s++);
    return url;
}

/*
 * Test if a URL is relative.
 *
 * RFC 2396, Section 5 (Page 17) implies that in a relative URL, a '/' will
 * appear before a ':'.
 */
int
urlIsRelative(const char *url)
{
    const char *p;

    if (url == NULL) {
        return (0);
    }
    if (*url == '\0') {
        return (0);
    }
    for (p = url; *p != '\0' && *p != ':' && *p != '/'; p++);

    if (*p == ':') {
        return (0);
    }
    return (1);
}

