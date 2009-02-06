<?php
/**
 * The groups are defined here
 */

$menus=array(
'Monitoring'=>array(
	'requests'=>'?menu=monitor_requests',
	'hits'=>'?menu=monitor_hits',
	'errors'=>'?menu=monitor_errors',
	'dns'=>'?menu=monitor_dns',
	'memory'=>'?menu=monitor_memory',
	'storage'=>'?menu=monitor_storage',
	'cpu'=>'?menu=monitor_cpu',
	'system'=>'?menu=monitor_system',
	'other'=>'?menu=monitor_other',
	),
'Access'=>array(
	'authentication'=>'?menu=set_auth',
	'access controls'=>'?menu=set_access_controls',
	),
'Cache'=>array(
	'memory cache'=>'?menu=set_memory_cache',
	'disk cache'=>'?menu=set_disk_cache',
	'cache digest'=>'?menu=set_cache_digest',
	'tuning'=>'?menu=set_cache_tuning',
	),
'Options'=>array(
	'ftp gatewaying'=>'?menu=set_ftp_gatewaying',
	'external support app'=>'?menu=set_external_support_programs',
	'neighbor select'=>'?menu=set_neighbor_select_algo',
	'cache registration'=>'?menu=set_cache_registration_service',
	'x-forwarded-for'=>'?menu=set_x-forwarded_for',
	'logfile'=>'?menu=set_logfile_options',
	),
'Network'=>array(
	'timeouts'=>'?menu=set_timeouts',
	'http options'=>'?menu=set_http_options',
	'nework options'=>'?menu=set_network_options',
	'ssl options'=>'?menu=set_ssl_options',
	'httpd-accelerator options'=>'?menu=set_httpd_accel',
	'dns options'=>'?menu=set_dns_options',
	'advanced networking'=>'?menu=set_adv_network',
	'pconnect handling'=>'?menu=set_pconnect_handling',
	'delay pool'=>'?menu=set_delay_pool',
	),
'Neighbor cache'=>array(
	'wccp1 and wccp2 config'=>'?menu=set_wccp1_wccp2_config',
	'icp options'=>'?menu=set_icp_options',
	'multicast options'=>'?menu=set_mcast_options',
	),
'Misc'=>array(
	'snmp'=>'?menu=set_snmp',
	'internal icon'=>'?menu=set_internal icon',
	'error page'=>'?menu=set_error_page',
	'request forward'=>'?menu=set_request_forwarding',
	'admin parameters'=>'?menu=set_admin_params',
	'misc'=>'?menu=set_misc',
	),
);

?>