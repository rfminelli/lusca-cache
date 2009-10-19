#ifndef	__LIBLOGHELPER_LOGHELPER_H__
#define	__LIBLOGHELPER_LOGHELPER_H__

struct _loghelper_buffer {
	char *buf;
	int size;
	int len;
	int written_len;
	dlink_node node;
};
typedef struct loghelper_buffer loghelper_buffer_t;

struct _loghelper_instance {
	int rfd, wfd;			/* read/write FDs to IPC helper */
	pid_t pid;			/* pid of helper */
	int flush_pending;		/* whether a flush command is pending */
	dlink_list bufs;		/* buffers to write */
	int nbufs;			/* number of buffers to write */
	int last_warned;
	struct {
		int closing:1;		/* whether we are closing - and further
					   queued messages should be rejected */
	} flags;
};
typedef struct _loghelper_instance loghelper_instance_t;

#endif
