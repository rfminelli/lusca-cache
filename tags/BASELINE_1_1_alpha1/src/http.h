

extern int httpCachable _PARAMS((char *, int));
extern int proxyhttpStart _PARAMS((edge *, char *, StoreEntry *));
extern int httpStart _PARAMS((int, char *, request_t *, char *, StoreEntry *));

#define HTTP_REPLY_FIELD_SZ 128
struct _http_reply {
    double version;
    int code;
    int content_length;
    int hdr_sz;
    char content_type[HTTP_REPLY_FIELD_SZ];
    char date[HTTP_REPLY_FIELD_SZ];
    char expires[HTTP_REPLY_FIELD_SZ];
    char last_modified[HTTP_REPLY_FIELD_SZ];
    char user_agent[HTTP_REPLY_FIELD_SZ << 2];
};
