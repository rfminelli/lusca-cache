## Process this file with automake to produce Makefile.in

if USE_DNSSERVER
DNSSOURCE = dns_external.c
DNSSERVER = dnsserver
else
DNSSOURCE = dns_internal.c
DNSSERVER =
endif

LDADD = -L../lib -lmiscutil

libsqdns_a_SOURCES = \
	$(DNSSOURCE)

noinst_LIBRARIES = \
	libsqdns.a

libexec_PROGRAMS = \
	$(DNSSERVER)

