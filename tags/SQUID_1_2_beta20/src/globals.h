
/*
 * $Id$
 */

extern FILE *debug_log;		/* NULL */
extern FILE *cache_useragent_log;	/* NULL */
extern SquidConfig Config;
extern SquidConfig2 Config2;
extern char *ConfigFile;	/* NULL */
extern char *IcpOpcodeStr[];
extern char *dns_error_message;	/* NULL */
extern const char *log_tags[];
extern char tmp_error_buf[ERROR_BUF_SZ];
extern char *volatile debug_options;	/* NULL */
extern char ThisCache[SQUIDHOSTNAMELEN << 1];
extern char config_input_line[BUFSIZ];
extern const char *AclMatchedName;	/* NULL */
extern const char *DefaultConfigFile;	/* DEFAULT_CONFIG_FILE */
extern const char *RequestMethodStr[];
extern const char *ProtocolStr[];
extern const char *cfg_filename;	/* NULL */
extern const char *const appname;	/* "squid" */
extern const char *const dash_str;	/* "-" */
extern const char *const localhost;	/* "127.0.0.1" */
extern const char *const null_string;	/* "" */
extern const char *const version_string;	/* SQUID_VERSION */
extern const char *const full_appname_string;	/* "Squid/" SQUID_VERSION */
extern const char *const w_space;	/* " \t\n\r" */
extern const char *fdTypeStr[];
extern const char *hier_strings[];
extern const char *memStatusStr[];
extern const char *pingStatusStr[];
extern const char *storeStatusStr[];
extern const char *swapStatusStr[];
extern dnsStatData DnsStats;
extern fde *fd_table;		/* NULL */
extern int Biggest_FD;		/* -1 */
extern int Number_FD;		/* 0 */
extern int HttpSockets[MAXHTTPPORTS];
extern int NDnsServersAlloc;	/* 0 */
extern int NHttpSockets;	/* 0 */
extern int RESERVED_FD;
extern int Squid_MaxFD;		/* SQUID_MAXFD */
extern int config_lineno;	/* 0 */
extern int configured_once;	/* 0 */
extern int debugLevels[MAX_DEBUG_SECTIONS];
extern int do_mallinfo;		/* 0 */
extern int opt_reuseaddr;	/* 1 */
extern int icmp_sock;		/* -1 */
extern int neighbors_do_private_keys;	/* 1 */
extern int opt_accel_uses_host;	/* 0 */
extern int opt_catch_signals;	/* 1 */
extern int opt_debug_stderr;	/* -1 */
extern int opt_dns_tests;	/* 1 */
extern int opt_foreground_rebuild;	/* 0 */
extern int opt_forwarded_for;	/* 1 */
extern int opt_reload_hit_only;	/* 0 */
extern int opt_syslog_enable;	/* 0 */
extern int opt_udp_hit_obj;	/* 0 */
extern int opt_create_swap_dirs;	/* 0 */
extern int opt_store_doublecheck;	/* 0 */
extern int syslog_enable;	/* 0 */
extern int theInIcpConnection;	/* -1 */
extern int theOutIcpConnection;	/* -1 */
#ifdef SQUID_SNMP
extern int theInSnmpConnection;	/* -1 */
extern int theOutSnmpConnection;	/* -1 */
extern char *snmp_agentinfo;
#endif
extern int vhost_mode;		/* 0 */
extern int vizSock;		/* -1 */
extern iostats IOStats;
extern struct _acl_deny_info_list *DenyInfoList;	/* NULL */
extern struct in_addr any_addr;
extern struct in_addr local_addr;
extern struct in_addr no_addr;
extern struct in_addr theOutICPAddr;
extern struct in_addr theOutSNMPAddr;
extern struct timeval current_time;
extern struct timeval squid_start;
extern time_t squid_curtime;	/* 0 */
extern int shutting_down;	/* 0 */
extern int reconfiguring;	/* 0 */
extern int store_rebuilding;	/* 1 */
extern int store_swap_size;	/* 0 */
extern unsigned long store_mem_size;	/* 0 */
extern time_t hit_only_mode_until;	/* 0 */
extern StatCounters Counter;
extern char *err_type_str[];
extern char *icp_opcode_str[];
extern char *swap_log_op_str[];
extern char *lookup_t_str[];
extern double request_failure_ratio;	/* 0.0 */
extern int store_hash_buckets;	/* 0 */
extern hash_table *store_table;	/* NULL */
extern dlink_list store_list;
extern const String StringNull;	/* { 0, 0, NULL } */
extern int hot_obj_count;	/* 0 */
extern int _db_level;
extern const int CacheDigestHashFuncCount;	/* 4 */
extern CacheDigest *store_digest;	/* NULL */
extern const char *StoreDigestUrlPath;	/* "store_digest" */
extern const char *StoreDigestMimeStr;	/* "application/cache-digest" */
extern const Version CacheDigestVer;	/* { 2, 2 } */
