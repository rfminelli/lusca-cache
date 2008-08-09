#ifndef	__LIBIAPP_COMM_H__
#define	__LIBIAPP_COMM_H__

/*
 * Macro to find file access mode
 */
#ifdef O_ACCMODE
#define FILE_MODE(x) ((x)&O_ACCMODE)
#else
#define FILE_MODE(x) ((x)&(O_RDONLY|O_WRONLY|O_RDWR))
#endif

#define DISK_OK                   (0)
#define DISK_ERROR               (-1)
#define DISK_EOF                 (-2)
#define DISK_NO_SPACE_LEFT       (-6)

/*
 * Hey dummy, don't be tempted to move this to lib/config.h.in
 * again.  O_NONBLOCK will not be defined there because you didn't
 * #include <fcntl.h> yet.
 */
#if defined(_SQUID_SUNOS_)
/*
 * We assume O_NONBLOCK is broken, or does not exist, on SunOS.
 */
#define SQUID_NONBLOCK O_NDELAY
#elif defined(O_NONBLOCK)
/*
 * We used to assume O_NONBLOCK was broken on Solaris, but evidence
 * now indicates that its fine on Solaris 8, and in fact required for
 * properly detecting EOF on FIFOs.  So now we assume that if  
 * its defined, it works correctly on all operating systems.
 */
#define SQUID_NONBLOCK O_NONBLOCK
/*
 * O_NDELAY is our fallback.
 */
#else
#define SQUID_NONBLOCK O_NDELAY
#endif

#define FD_READ_METHOD(fd, buf, len) (*fd_table[fd].read_method)(fd, buf, len)
#define FD_WRITE_METHOD(fd, buf, len) (*fd_table[fd].write_method)(fd, buf, len)

enum {
    FD_NONE,
    FD_LOG,
    FD_FILE,
    FD_SOCKET,
    FD_PIPE,
    FD_UNKNOWN
};

enum {
    FD_READ,
    FD_WRITE
};


typedef struct _close_handler close_handler;

typedef struct _dread_ctrl dread_ctrl;
typedef struct _dwrite_q dwrite_q;

typedef void PF(int, void *);
typedef void CWCB(int fd, char *, size_t size, int flag, void *data);
typedef void CRCB(int fd, int size, int flag, int xerrno, void *data);
typedef void CNCB(int fd, int status, void *);
typedef int DEFER(int fd, void *data);
typedef int READ_HANDLER(int, char *, int);
typedef int WRITE_HANDLER(int, const char *, int);
typedef void CBCB(char *buf, ssize_t size, void *data);

/* disk.c / diskd.c callback typedefs */
typedef void DRCB(int, const char *buf, int size, int errflag, void *data);
                                                        /* Disk read CB */
typedef void DWCB(int, int, size_t, void *);    /* disk write CB */
typedef void DOCB(int, int errflag, void *data);        /* disk open CB */
typedef void DCCB(int, int errflag, void *data);        /* disk close CB */
typedef void DUCB(int errflag, void *data);     /* disk unlink CB */
typedef void DTCB(int errflag, void *data);     /* disk trunc CB */

struct _close_handler {
    PF *handler;
    void *data;
    close_handler *next;
};

struct _dread_ctrl {
    int fd;
    off_t file_offset;
    size_t req_len;
    char *buf;
    int end_of_file;
    DRCB *handler;
    void *client_data;
};

struct _dwrite_q {
    off_t file_offset;
    char *buf;
    size_t len;
    size_t buf_offset;
    dwrite_q *next;
    FREE *free_func;
};

struct _CommWriteStateData {
    int valid;
    char *buf;
    size_t size;
    size_t offset;
    CWCB *handler;
    void *handler_data;
    FREE *free_func;
    char header[32];
    size_t header_size;
};
typedef struct _CommWriteStateData CommWriteStateData;

/* Special case pending filedescriptors. Set in fd_table[fd].read/write_pending
 */
typedef enum {
    COMM_PENDING_NORMAL,        /* No special processing required */
    COMM_PENDING_WANTS_READ,    /* need to read, no matter what commSetSelect indicates */
    COMM_PENDING_WANTS_WRITE,   /* need to write, no matter what commSetSelect indicates */
    COMM_PENDING_NOW            /* needs to be called again, without needing to wait for readiness
                                 * for example when data is already buffered etc */
} comm_pending;


#define COMM_OK           (0)
#define COMM_ERROR       (-1)
#define COMM_NOMESSAGE   (-3)
#define COMM_TIMEOUT     (-4)
#define COMM_SHUTDOWN    (-5)
#define COMM_INPROGRESS  (-6)
#define COMM_ERR_CONNECT (-7)
#define COMM_ERR_DNS     (-8)
#define COMM_ERR_CLOSING (-9)

/* Select types. */
#define COMM_SELECT_READ   (0x1)
#define COMM_SELECT_WRITE  (0x2)

#define COMM_NONBLOCKING        0x01
#define COMM_NOCLOEXEC          0x02
#define COMM_REUSEADDR          0x04
#define FD_DESC_SZ              64

struct _fde {
    unsigned int type;
    u_short local_port;
    u_short remote_port;
    sqaddr_t local_address;
    unsigned char tos;
    char ipaddrstr[MAX_IPSTRLEN]; /* dotted decimal address of peer - XXX should be MAX_IPSTRLEN */
    const char *desc;
    char descbuf[FD_DESC_SZ];
    struct {
        unsigned int open:1;
        unsigned int close_request:1;
        unsigned int write_daemon:1;
        unsigned int closing:1;
        unsigned int socket_eof:1;
        unsigned int nolinger:1;
        unsigned int nonblocking:1;
        unsigned int ipc:1;
        unsigned int called_connect:1;
        unsigned int nodelay:1;
        unsigned int close_on_exec:1;
        unsigned int backoff:1; /* keep track of whether the fd is backed off */
        unsigned int dnsfailed:1;       /* did the dns lookup fail */
    } flags;
    comm_pending read_pending;
    comm_pending write_pending;
    squid_off_t bytes_read;
    squid_off_t bytes_written;
    int uses;                   /* ie # req's over persistent conn */
    struct _fde_disk {
        DWCB *wrt_handle;
        void *wrt_handle_data;
        dwrite_q *write_q;
        dwrite_q *write_q_tail;
        off_t offset;
    } disk;
    struct {
    	struct {
		char *buf;
		int size;
		CRCB *cb;
		void *cbdata;
		int active;
    	} read;
    } comm;
    PF *read_handler;
    void *read_data;
    PF *write_handler;
    void *write_data;
    PF *timeout_handler;
    time_t timeout;
    void *timeout_data;
    void *lifetime_data;
    close_handler *close_handler;       /* linked list */
    DEFER *defer_check;         /* check if we should defer read */
    void *defer_data;
    struct _CommWriteStateData rwstate;         /* State data for comm_write */
    READ_HANDLER *read_method;
    WRITE_HANDLER *write_method;
#if USE_SSL
    SSL *ssl;
#endif
#ifdef _SQUID_MSWIN_
    struct {
        long handle;
    } win32;
#endif
#if DELAY_POOLS
    int slow_id;
#endif
};

typedef struct _fde fde;

/* .. XXX how the hell will this be threaded? */
struct _CommStatStruct {
    struct {
        struct {
            int opens;
            int closes;
            int reads;
            int writes;
            int seeks;
            int unlinks;
        } disk;
        struct {
            int accepts;
            int sockets;
            int connects;
            int binds;
            int closes;
            int reads;
            int writes;
            int recvfroms;
            int sendtos;
        } sock;
        int polls;
        int selects;
    } syscalls;
    int select_fds;
    int select_loops;
    int select_time;
};

typedef struct _CommStatStruct CommStatStruct;

extern void fd_init(void);
extern void fd_close(int fd);
extern void fd_open(int fd, unsigned int type, const char *);
extern void fd_note(int fd, const char *);
extern void fd_note_static(int fd, const char *);
extern void fd_bytes(int fd, int len, unsigned int type);
extern void fdFreeMemory(void);
extern void fdDumpOpen(void);
extern int fdNFree(void);
extern int fdUsageHigh(void);
extern void fdAdjustReserved(void);

extern int commSetNonBlocking(int fd);
extern int commUnsetNonBlocking(int fd);
extern void commSetCloseOnExec(int fd);
extern void commSetTcpKeepalive(int fd, int idle, int interval, int timeout);
extern int commSetTos(int fd, int tos);
extern int commSetSocketPriority(int fd, int prio);
extern int commSetIPOption(int fd, uint8_t option, void *value, size_t size);
extern int comm_accept(int fd, struct sockaddr_in *, struct sockaddr_in *);
extern void comm_close(int fd);
extern void comm_reset_close(int fd);
#if LINGERING_CLOSE
extern void comm_lingering_close(int fd);
#endif
extern void commConnectStart(int fd, const char *, u_short, CNCB *, void *, struct in_addr *addr);
extern int comm_connect_addr(int sock, const struct sockaddr_in *);
extern void comm_init(void);
extern int comm_listen(int sock);
extern int comm_open(int, int, struct in_addr, u_short port, int, const char *note);
extern int comm_openex(int, int, struct in_addr, u_short, int, unsigned char TOS, const char *);
extern int comm_fdopen(int, int, struct in_addr, u_short, int, const char *);
extern int comm_fdopenex(int, int, struct in_addr, u_short, int, unsigned char, const char *);
extern u_short comm_local_port(int fd);

extern void commDeferFD(int fd);
extern void commResumeFD(int fd);
extern void commSetSelect(int, unsigned int, PF *, void *, time_t);
extern void commRemoveSlow(int fd);
extern void comm_add_close_handler(int fd, PF *, void *);
extern void comm_remove_close_handler(int fd, PF *, void *);
extern int comm_udp_sendto(int, const struct sockaddr_in *, int, const void *, int);
extern void comm_write(int fd,
    const char *buf,
    int size,
    CWCB * handler,
    void *handler_data,
    FREE *);
extern void comm_write_mbuf(int fd, MemBuf mb, CWCB * handler, void *handler_data);
extern void comm_write_header(int fd,
    const char *buf,
    int size,
    const char *header,
    size_t header_size,
    CWCB * handler,
    void *handler_data,
    FREE *);
extern void comm_write_mbuf_header(int fd, MemBuf mb, const char *header, size_t header_size, CWCB * handler, void *handler_data);
/* comm_read / comm_read_cancel two functions are in testing and not to be used! */
extern void comm_read(int fd, char *buf, int size, CRCB *cb, void *data);
extern int comm_read_cancel(int fd);

extern void commCallCloseHandlers(int fd);
extern int commSetTimeout(int fd, int, PF *, void *);
extern void commSetDefer(int fd, DEFER * func, void *);
extern int ignoreErrno(int);
extern void commCloseAllSockets(void);
extern int commBind(int s, sqaddr_t *addr);
extern void commSetTcpNoDelay(int);
extern void commSetTcpRcvbuf(int, int);

/*
 * comm_select.c
 */
extern void comm_select_init(void);
extern void comm_select_postinit(void);
extern void comm_select_shutdown(void);
extern int comm_select(int);
extern void commUpdateEvents(int fd);
extern void commSetEvents(int fd, int need_read, int need_write);
extern void commClose(int fd);
extern void commOpen(int fd);
extern void commUpdateReadHandler(int, PF *, void *);
extern void commUpdateWriteHandler(int, PF *, void *);
extern void comm_quick_poll_required(void);
extern const char * comm_select_status(void);

/* disk.c */
extern int file_open(const char *path, int mode);
extern void file_close(int fd);
extern void file_write(int, off_t, void *, size_t len, DWCB *, void *, FREE *);
extern void file_write_mbuf(int fd, off_t, MemBuf mb, DWCB * handler, void *handler_data);
extern void file_read(int, char *, size_t, off_t, DRCB *, void *);
extern void disk_init(void);
extern void disk_init_mem(void);

extern fde *fd_table;
extern int Biggest_FD;          /* -1 */
extern int Number_FD;           /* 0 */
extern int Opening_FD;          /* 0 */
extern int Squid_MaxFD;         /* SQUID_MAXFD */
extern int RESERVED_FD;

extern struct in_addr any_addr;
extern struct in_addr local_addr;
extern struct in_addr no_addr;

extern CommStatStruct CommStats;


#endif
