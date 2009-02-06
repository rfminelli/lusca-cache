<?php
/**
 * List of the items from manager that poller will poll
 * Same array wil be used for generating the graphs
 */

$poller_config=array(
//info section
"storedir"=>array(
"Store Entries",
"Current Store Swap Size",
"Current Capacity",
),
"5min"=>array(
"client_http.requests",
"client_http.hits",
"client_http.errors",
"client_http.kbytes_in",
"client_http.kbytes_out",
"client_http.all_median_svc_time",
"client_http.miss_median_svc_time",
"client_http.hit_median_svc_time",
"dns.median_svc_time",
"server.ftp.requests",
"server.other.requests",
"icp.pkts_sent",
"icp.pkts_recv",
"select_loops",
"select_fds",
"syscalls.polls",
"syscalls.disk.opens",
"syscalls.disk.reads",
"syscalls.disk.writes",
"cpu_usage",
),
"info"=>array(
"Number of HTTP requests received",
"Average HTTP requests per minute since start",
"HTTP Requests (All)",
"Cache Misses",
"Cache Hits",
"Near Hits",
"DNS Lookups",
"CPU Usage",
"CPU Usage, 5 minute avg",
"CPU Usage, 60 minute avg",
"Process Data Segment Size via sbrk()",
"Total space in arena",
"Total in use",
"Total free",
"Largest file desc currently in use",
"Number of file desc currently in use",
),
);

?>