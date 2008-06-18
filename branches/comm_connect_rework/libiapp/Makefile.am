## Process this file with automake to produce Makefile.in

if USE_POLL
COMMLOOP_SOURCE = comm_poll.c
endif
if USE_EPOLL
COMMLOOP_SOURCE = comm_epoll.c
endif
if USE_SELECT
COMMLOOP_SOURCE = comm_select.c
endif
if USE_SELECT_SIMPLE
COMMLOOP_SOURCE = comm_select_simple.c
endif
if USE_SELECT_WIN32
COMMLOOP_SOURCE = comm_select_win32.c
endif
if USE_KQUEUE
COMMLOOP_SOURCE = comm_kqueue.c
endif
if USE_DEVPOLL
COMMLOOP_SOURCE = comm_devpoll.c
endif
if USE_EVENTPORTS
COMMLOOP_SOURCE = comm_eventports.c
endif

libiapp_a_SOURCES = \
	fd.c \
	comm.c \
	disk.c \
	event.c \
	globals.c \
	pconn_hist.c \
	$(COMMLOOP_SOURCE)

noinst_LIBRARIES = \
	libiapp.a
