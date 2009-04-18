#ifndef	__LIBSQMOD_MODULE_H__
#define	__LIBSQMOD_MODULE_H__

struct _module;
typedef struct _module module_t;

typedef int MOD_INIT_FUNCTION(module_t *m);

typedef enum {
	MOD_NONE,
	MOD_LOADED,
	MOD_READY,
	MOD_INACTIVE,
	MOD_ERROR,
	MOD_DYING
} module_state_t;

/*
 * A configured module.
 * For now there must not be >1 module with the same name.
 */
struct _module {
	dlink_node n;
	const char *mod_name;
	const char *mod_path;
	void *dl_handle;
	MOD_INIT_FUNCTION *mod_init;
	module_state_t state;
};

extern int module_init(void);
extern module_t * module_register(const char *path);
extern void module_setup(void);

#endif
