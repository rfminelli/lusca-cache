#ifndef	__LIBCB_CBDATA_H__
#define	__LIBCB_CBDATA_H__

#define CREATE_CBDATA(type) cbdataInitType(CBDATA_##type, #type, sizeof(type), NULL)
#define CREATE_CBDATA_FREE(type, free_func) cbdataInitType(CBDATA_##type, #type, sizeof(type), free_func)
#define CBDATA_COOKIE(p) ((void *)((unsigned long)(p) ^ 0xDEADBEEF))

/* cbdata macros */
#define cbdataAlloc(type) ((type *)cbdataInternalAlloc(CBDATA_##type))
#define cbdataFree(var) (var = (var != NULL ? cbdataInternalFree(var): NULL))
#define CBDATA_TYPE(type)       static cbdata_type CBDATA_##type = 0
#define CBDATA_GLOBAL_TYPE(type)        cbdata_type CBDATA_##type
#define CBDATA_INIT_TYPE(type)  (CBDATA_##type ? 0 : (CBDATA_##type = cbdataAddType(CBDATA_##type, #type, sizeof(type), NULL)))
#define CBDATA_INIT_TYPE_FREECB(type, free_func)        (CBDATA_##type ? 0 : (CBDATA_##type = cbdataAddType(CBDATA_##type, #type, sizeof(type), free_func)))

/*  
 * cbdata types. similar to the MEM_* types above, but managed
 * in cbdata.c. A big difference is that these types are dynamically
 * allocated. This list is only a list of predefined types. Other types
 * are added runtime
 */ 
typedef enum {
    CBDATA_UNKNOWN = 0,
    CBDATA_UNDEF = 0,
    CBDATA_FIRST = 1,
    CBDATA_FIRST_CUSTOM_TYPE = 1000
} cbdata_type;

extern void cbdataInit(void);
#if CBDATA_DEBUG
extern void *cbdataInternalAllocDbg(cbdata_type type, int, const char *);
extern void cbdataLockDbg(const void *p, const char *, int);
extern void cbdataUnlockDbg(const void *p, const char *, int);
#else
extern void *cbdataInternalAlloc(cbdata_type type);
extern void cbdataLock(const void *p); 
extern void cbdataUnlock(const void *p);
#endif
/* Note: Allocations is done using the cbdataAlloc macro */
extern void *cbdataInternalFree(void *p);
extern int cbdataValid(const void *p);
extern void cbdataInitType(cbdata_type type, const char *label, int size, FREE * free_func);
extern cbdata_type cbdataAddType(cbdata_type type, const char *label, int size, FREE * free_func);
extern int cbdataLocked(const void *p);

extern int cbdataCount;

#if CBDATA_DEBUG 
#define cbdataAlloc(a,b)        cbdataAllocDbg(a,b,__FILE__,__LINE__)
#define cbdataLock(a)           cbdataLockDbg(a,__FILE__,__LINE__)
#define cbdataUnlock(a)         cbdataUnlockDbg(a,__FILE__,__LINE__)
#endif

#endif
