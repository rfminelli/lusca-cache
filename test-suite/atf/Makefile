
include ./Makefile.inc

BASEDIR=../..

LDADD=-L$(BASEDIR)/libcore \
	-L$(BASEDIR)/lib \
	-L$(BASEDIR)/libcore \
	-L$(BASEDIR)/libsqdebug \
	-L$(BASEDIR)/libmem \
	-L$(BASEDIR)/libcb \
	-L$(BASEDIR)/libmime \
	-L$(BASEDIR)/libhelper \
	-L$(BASEDIR)/libstmem \
	-L$(BASEDIR)/libiapp \
	-L$(BASEDIR)/libhttp \
	-L$(BASEDIR)/libstat \
	-L$(BASEDIR)/libsqdns \
	-L$(BASEDIR)/libsqident \
	-L$(BASEDIR)/libsqinet \
	-L$(BASEDIR)/libsqname \
	-L$(BASEDIR)/libasyncio \
	-L$(BASEDIR)/libsqtlv \
	-L$(BASEDIR)/libsqstore \
	-lsqstore \
	-lsqtlv \
	-lasyncio \
	-lstmem \
	-lsqdns \
	-lsqident \
	-lsqinet \
	-lsqdebug \
	-lsqname \
	-lcb \
	-lhelper \
	-lmime \
	-liapp \
	-lhttp \
	-lmem \
	-lcore \
	-lstat \
	-lmiscutil \
	-lpthread -lm \
	-latf-c

LDFLAGS=$(LDADD) -L$(ATF_LIBDIR)
CFLAGS=-O -g -I$(BASEDIR) -I$(ATF_INCLUDEDIR)

all: libhttp lib_Vector

libhttp: libhttp.o core.o
	$(CC) core.o libhttp.o -o libhttp $(LDFLAGS)

lib_Vector: lib_Vector.o core.o
	$(CC) core.o lib_Vector.o -o lib_Vector $(LDFLAGS)


check: libhttp lib_Vector
	@env PATH=$$PATH:$(ATF_BINDIR) $(ATF_BINDIR)/atf-run

clean:
	rm -f libhttp *.o

