#ifndef	__LIBLOGHELPER_LOGHELPER_COMMANDS_H__
#define	__LIBLOGHELPER_LOGHELPER_COMMANDS_H__


typedef enum {
	LH_CMD_NONE,
	LH_CMD_ROTATE,
	LH_CMD_TRUNCATE,
	LH_CMD_REOPEN,
	LH_CMD_SET_ROTATE_COUNT,
	LH_CMD_WRITE,
	LH_CMD_ENABLE_BUFFERING,
	LH_CMD_DISABLE_BUFFERING,
	LH_CMD_LAST
} loghelper_command_t;


#endif
