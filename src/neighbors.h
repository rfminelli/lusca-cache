/*
 * $Id$
 *
 * AUTHOR: Harvest Derived
 *
 * SQUID Internet Object Cache  http://squid.nlanr.net/Squid/
 * --------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from the
 *  Internet community.  Development is led by Duane Wessels of the
 *  National Laboratory for Applied Network Research and funded by
 *  the National Science Foundation.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *  
 */

/*
 * Copyright (c) 1994, 1995.  All rights reserved.
 *  
 *   The Harvest software was developed by the Internet Research Task
 *   Force Research Group on Resource Discovery (IRTF-RD):
 *  
 *         Mic Bowman of Transarc Corporation.
 *         Peter Danzig of the University of Southern California.
 *         Darren R. Hardy of the University of Colorado at Boulder.
 *         Udi Manber of the University of Arizona.
 *         Michael F. Schwartz of the University of Colorado at Boulder.
 *         Duane Wessels of the University of Colorado at Boulder.
 *  
 *   This copyright notice applies to software in the Harvest
 *   ``src/'' directory only.  Users should consult the individual
 *   copyright notices in the ``components/'' subdirectories for
 *   copyright information about other software bundled with the
 *   Harvest source code distribution.
 *  
 * TERMS OF USE
 *   
 *   The Harvest software may be used and re-distributed without
 *   charge, provided that the software origin and research team are
 *   cited in any use of the system.  Most commonly this is
 *   accomplished by including a link to the Harvest Home Page
 *   (http://harvest.cs.colorado.edu/) from the query page of any
 *   Broker you deploy, as well as in the query result pages.  These
 *   links are generated automatically by the standard Broker
 *   software distribution.
 *   
 *   The Harvest software is provided ``as is'', without express or
 *   implied warranty, and with no support nor obligation to assist
 *   in its use, correction, modification or enhancement.  We assume
 *   no liability with respect to the infringement of copyrights,
 *   trade secrets, or any patents, and are not responsible for
 *   consequential damages.  Proper use of the Harvest software is
 *   entirely the responsibility of the user.
 *  
 * DERIVATIVE WORKS
 *  
 *   Users may make derivative works from the Harvest software, subject 
 *   to the following constraints:
 *  
 *     - You must include the above copyright notice and these 
 *       accompanying paragraphs in all forms of derivative works, 
 *       and any documentation and other materials related to such 
 *       distribution and use acknowledge that the software was 
 *       developed at the above institutions.
 *  
 *     - You must notify IRTF-RD regarding your distribution of 
 *       the derivative work.
 *  
 *     - You must clearly notify users that your are distributing 
 *       a modified version and not the original Harvest software.
 *  
 *     - Any derivative product is also subject to these copyright 
 *       and use restrictions.
 *  
 *   Note that the Harvest software is NOT in the public domain.  We
 *   retain copyright, as specified above.
 *  
 * HISTORY OF FREE SOFTWARE STATUS
 *  
 *   Originally we required sites to license the software in cases
 *   where they were going to build commercial products/services
 *   around Harvest.  In June 1995 we changed this policy.  We now
 *   allow people to use the core Harvest software (the code found in
 *   the Harvest ``src/'' directory) for free.  We made this change
 *   in the interest of encouraging the widest possible deployment of
 *   the technology.  The Harvest software is really a reference
 *   implementation of a set of protocols and formats, some of which
 *   we intend to standardize.  We encourage commercial
 *   re-implementations of code complying to this set of standards.  
 */

#ifndef NEIGHBORS_H
#define NEIGHBORS_H

/* Labels for hierachical log file */
/* put them all here for easier reference when writing a logfile analyzer */

typedef enum {
    HIER_NONE,
    HIER_DIRECT,
    HIER_SIBLING_HIT,
    HIER_PARENT_HIT,
    HIER_SINGLE_PARENT,
    HIER_FIRSTUP_PARENT,
    HIER_NO_PARENT_DIRECT,
    HIER_FIRST_PARENT_MISS,
    HIER_LOCAL_IP_DIRECT,
    HIER_FIREWALL_IP_DIRECT,
    HIER_NO_DIRECT_FAIL,
    HIER_SOURCE_FASTEST,
    HIER_SIBLING_UDP_HIT_OBJ,
    HIER_PARENT_UDP_HIT_OBJ,
    HIER_MAX
} hier_code;

typedef enum {
    EDGE_NONE,
    EDGE_SIBLING,
    EDGE_PARENT
} neighbor_t;

/* Mark a neighbor cache as dead if it doesn't answer this many pings */
#define HIER_MAX_DEFICIT  20

struct _domain_ping {
    char *domain;
    int do_ping;		/* boolean */
    struct _domain_ping *next;
};

struct _domain_type {
    char *domain;
    neighbor_t type;
    struct _domain_type *next;
};

/* bitfields for edge->options */
#define NEIGHBOR_PROXY_ONLY 0x01
#define NEIGHBOR_NO_QUERY   0x02

#define EDGE_MAX_ADDRESSES 10
#define RTT_AV_FACTOR      1000
struct _edge {
    char *host;
    neighbor_t type;
    struct sockaddr_in in_addr;
    int neighbor_up;		/* 0 if no, 1 if yes */
    struct {
	int pings_sent;
	int pings_acked;
	int ack_deficit;
	int fetches;
	int rtt;
	int counts[ICP_OP_END];
	int ignored_replies;
    } stats;
    u_short icp_port;
    u_short http_port;
    int icp_version;
    struct _domain_ping *pinglist;
    struct _domain_type *typelist;
    struct _acl_list *acls;
    int options;
    int weight;
    int mcast_ttl;
    time_t last_fail_time;	/* detect down dumb caches */
    struct in_addr addresses[10];
    int n_addresses;
    struct _edge *next;
};

typedef struct {
    int n;
    int n_parent;
    int n_sibling;
    edge *edges_head;
    edge *edges_tail;
    edge *first_ping;
} neighbors;

struct _hierarchyLogData {
    hier_code code;
    char *host;
    int timeout;
};

extern edge *getFirstEdge _PARAMS((void));
extern edge *getFirstUpParent _PARAMS((request_t *));
extern edge *getNextEdge _PARAMS((edge *));
extern edge *getSingleParent _PARAMS((request_t *, int *n));
extern int neighborsUdpPing _PARAMS((protodispatch_data *));
extern void neighborAddDomainPing _PARAMS((const char *, const char *));
extern void neighborAddDomainType _PARAMS((const char *, const char *, const char *));
extern void neighborAddAcl _PARAMS((const char *, const char *));
extern neighbors *neighbors_create _PARAMS((void));
extern void hierarchyNote _PARAMS((request_t *, hier_code, int, const char *));
extern void neighborsUdpAck _PARAMS((int, const char *, icp_common_t *, const struct sockaddr_in *, StoreEntry *, char *, int));
extern void neighborAdd _PARAMS((const char *, const char *, int, int, int, int, int));
extern void neighbors_open _PARAMS((int));
extern void neighborsDestroy _PARAMS((void));
extern edge *neighborFindByName _PARAMS((const char *));

extern const char *hier_strings[];

#endif /* NEIGHBORS_H */
