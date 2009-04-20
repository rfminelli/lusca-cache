
# This is a very hacky, freebsd specific makefile to build
# a "null" Lusca module.

CFLAGS=-O2 -g -I../../.. -shared
LDFLAGS=-shared
RM=rm

null.so: null.o
	$(CC) $(LDFLAGS) -fPIC -Wl,-soname,null.so -o null.so null.o

all: null.so

clean:
	$(RM) -f null.o null.so
