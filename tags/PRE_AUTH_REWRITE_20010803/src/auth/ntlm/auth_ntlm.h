/*
 * auth_ntlm.h
 * Internal declarations for the ntlm auth module
 */

#ifndef __AUTH_NTLM_H__
#define __AUTH_NTLM_H__

#define DefaultAuthenticateChildrenMax  32	/* 32 processes */

/* Generic */
typedef struct {
    void *data;
    auth_user_request_t *auth_user_request;
    RH *handler;
} authenticateStateData;

struct _ntlm_user {
    /* what username did this connection get? */
    char *username;
    dlink_list proxy_auth_list;
};

struct _ntlm_request {
    /* what negotiate string did the client use? */
    char *ntlmnegotiate;
    /* what challenge did we give the client? */
    char *authchallenge;
    /* what authenticate string did we get? */
    char *ntlmauthenticate;
    /*we need to store the NTLM helper between requests */
    helper_stateful_server *authhelper;
    /* how far through the authentication process are we? */
    auth_state_t auth_state;
};

struct _ntlm_helper_state_t {
    char *challenge;		/* the challenge to use with this helper */
    int starve;			/* 0= normal operation. 1=don't hand out any more challenges */
    int challengeuses;		/* the number of times this challenge has been issued */
    time_t renewed;
};

/* configuration runtime data */
struct _auth_ntlm_config {
    int authenticateChildren;
    wordlist *authenticate;
    int challengeuses;
    time_t challengelifetime;
};

typedef struct _ntlm_user ntlm_user_t;
typedef struct _ntlm_request ntlm_request_t;
typedef struct _ntlm_helper_state_t ntlm_helper_state_t;
typedef struct _auth_ntlm_config auth_ntlm_config;

extern MemPool *ntlm_helper_state_pool;
extern MemPool *ntlm_user_pool;
extern MemPool *ntlm_request_pool;

#endif
