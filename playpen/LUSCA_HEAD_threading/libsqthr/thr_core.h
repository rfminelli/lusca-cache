#ifndef	__THR_CORE_H__
#define	__THR_CORE_H__

struct _thr_core_queue_t {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    dlink_list l;
    unsigned long requests;
    unsigned long blocked;      /* main failed to lock the queue */
};

struct _thr_core_thread_t {
    dlink_node n;
    pthread_t thread;
/*
    squidaio_thread_status status;
    struct squidaio_request_t *current_req;
    unsigned long requests;
*/
};

struct _thr_core_t {
	pthread_attr_t globattr;
	dlink_list thr;
};

#endif	/* __THR_CORE_H__ */
