/*
 * $Id$
 */

#ifdef SQUID_SNMP
#ifndef CACHE_SNMP_H
#define CACHE_SNMP_H

#if SIZEOF_LONG == 8
#define snint int
#else
#define snint long
#endif

#ifndef MIN
#define MIN(a,b) (a<b?a:b)
#endif

#include "snmp.h"
#include "snmp_impl.h"
#include "asn1.h"
#include "snmp_api.h"

#include "snmp_vars.h"

/* MIB definitions
 * SQUID-MIB
 * 	.iso.org.dod.internet.private.enterprises.nlanr.squid
 *	  1   3   6     1        4         1      3495    1
 *
 * PROXY-MIB
 *	.iso.org.dod.internet.experimental.nsfnet.proxy
 *	  1   3   6     1          3         25    17
 */

#define SQUIDMIB 1, 3, 6, 1, 4, 1, 3495, 1
#define LEN_SQUIDMIB 8

/* basic groups under .squid */

#define SQ_SYS  SQUIDMIB, 1
#define LEN_SQ_SYS LEN_SQUIDMIB+1
#define SQ_CONF SQUIDMIB, 2
#define LEN_SQ_CONF LEN_SQUIDMIB+1
#define SQ_PRF  SQUIDMIB, 3
#define LEN_SQ_PRF LEN_SQUIDMIB+1
#define SQ_NET  SQUIDMIB, 4
#define LEN_SQ_NET LEN_SQUIDMIB+1
#define SQ_MESH  SQUIDMIB, 5
#define LEN_SQ_MESH LEN_SQUIDMIB+1

/* 
	cacheSystem group 
*/

enum {
    SYS_START,
    SYSVMSIZ,
    SYSSTOR,
    SYS_UPTIME,
    SYS_END
};

/* 
	cacheConfig group 
*/

enum {
    CONF_START,
    CONF_ADMIN,
    CONF_VERSION,
    CONF_VERSION_ID,
    CONF_LOG_FAC,
    CONF_STORAGE,
    CONF_END
};

enum {
    CONF_ST_START,
    CONF_ST_MMAXSZ,
    CONF_ST_MHIWM,
    CONF_ST_MLOWM,
    CONF_ST_SWMAXSZ,
    CONF_ST_SWHIWM,
    CONF_ST_SWLOWM,
    CONF_ST_END
};

/* 
	cacheMesh group 
*/

enum {
    MESH_START,
    MESH_PTBL,
    MESH_CTBL,
    MESH_END
};

enum {				/* cachePeerTable */
    MESH_PTBL_START,
    MESH_PTBL_NAME,
    MESH_PTBL_IP,
    MESH_PTBL_HTTP,
    MESH_PTBL_ICP,
    MESH_PTBL_TYPE,
    MESH_PTBL_STATE,
    MESH_PTBL_SENT,
    MESH_PTBL_PACKED,
    MESH_PTBL_FETCHES,
    MESH_PTBL_RTT,
    MESH_PTBL_IGN,
    MESH_PTBL_KEEPAL_S,
    MESH_PTBL_KEEPAL_R,
    MESH_PTBL_END
};

enum {				/* cacheClientTable */
    MESH_CTBL_START,
    MESH_CTBL_ADDR,
    MESH_CTBL_HTREQ,
    MESH_CTBL_HTBYTES,
    MESH_CTBL_HTHITS,
    MESH_CTBL_HTHITBYTES,
    MESH_CTBL_ICPREQ,
    MESH_CTBL_ICPBYTES,
    MESH_CTBL_ICPHITS,
    MESH_CTBL_ICPHITBYTES,
    MESH_CTBL_END
};

/* 
	cacheNetwork group 
*/

enum {
    IP_START,
    IP_ENT,
    IP_REQ,
    IP_HITS,
    IP_PENDHIT,
    IP_NEGHIT,
    IP_MISS,
    IP_GHBN,
    IP_LOC,
    IP_END
};

enum {
    FQDN_START,
    FQDN_ENT,
    FQDN_REQ,
    FQDN_HITS,
    FQDN_PENDHIT,
    FQDN_NEGHIT,
    FQDN_MISS,
    FQDN_GHBN,
    FQDN_END
};

enum {
    DNS_START,
    DNS_REQ,
    DNS_REP,
    DNS_SERVERS,
    DNS_END
};

/* 
	Cache Performance Group 
*/

enum {
    PERF_START,
    PERF_SYS,
    PERF_PROTO,
    PERF_PEER,
    PERF_END
};

enum {
    PERF_SYS_START,
    PERF_SYS_PF,
    PERF_SYS_NUMR,
    PERF_SYS_DEFR,
    PERF_SYS_MEMUSAGE,
    PERF_SYS_CPUUSAGE,
    PERF_SYS_MAXRESSZ,
    PERF_SYS_NUMOBJCNT,
    PERF_SYS_CURLRUEXP,
    PERF_SYS_CURUNLREQ,
    PERF_SYS_CURUNUSED_FD,
    PERF_SYS_CURRESERVED_FD,
    PERF_SYS_END
};

enum {
    PERF_PROTOSTAT_START,
    PERF_PROTOSTAT_AGGR,
    PERF_PROTOSTAT_MEDIAN,
    PERF_PROTOSTAT_END
};

enum {
    PERF_PROTOSTAT_AGGR_START,
    PERF_PROTOSTAT_AGGR_HTTP_REQ,
    PERF_PROTOSTAT_AGGR_HTTP_HITS,
    PERF_PROTOSTAT_AGGR_HTTP_ERRORS,
    PERF_PROTOSTAT_AGGR_HTTP_KBYTES_IN,
    PERF_PROTOSTAT_AGGR_HTTP_KBYTES_OUT,
    PERF_PROTOSTAT_AGGR_ICP_S,
    PERF_PROTOSTAT_AGGR_ICP_R,
    PERF_PROTOSTAT_AGGR_ICP_SKB,
    PERF_PROTOSTAT_AGGR_ICP_RKB,
    PERF_PROTOSTAT_AGGR_REQ,
    PERF_PROTOSTAT_AGGR_ERRORS,
    PERF_PROTOSTAT_AGGR_KBYTES_IN,
    PERF_PROTOSTAT_AGGR_KBYTES_OUT,
    PERF_PROTOSTAT_AGGR_CURSWAP,
    PERF_PROTOSTAT_AGGR_END
};

enum {
    PERF_MEDIAN_START,
    PERF_MEDIAN_TIME,
    PERF_MEDIAN_HTTP_ALL,
    PERF_MEDIAN_HTTP_MISS,
    PERF_MEDIAN_HTTP_NM,
    PERF_MEDIAN_HTTP_HIT,
    PERF_MEDIAN_ICP_QUERY,
    PERF_MEDIAN_ICP_REPLY,
    PERF_MEDIAN_DNS,
    PERF_MEDIAN_END
};

#endif
#endif
