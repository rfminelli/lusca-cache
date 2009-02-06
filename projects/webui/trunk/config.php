<?php
/*
SQUID Web Management config file
*/

// Path to the templates (tpl files) where the html part is located (shouldn't normally be changed)
$template_path="template/";
// Method of getting the data (command_port, ftp or squid_manager), command_port is preffered.
$retrieve_method="ftp";
// Local config file (serialized)
$local_config="user_config.cfg";
// Where to store polled data? file,mysql
$store_data="file";
// SQUID IP or hostname
$squid_host="10.0.0.3";
// SQUID Port
$squid_port="65432";
// Password for squid (either command_port or squid_manager), if needed
$squid_password="";
// Squid config file retrieval method (file-local file (ignores ftp), ftp-get from ftp)
$squid_config_method="file";
// FTP Port
$squid_ftp_port="21";
// FTP Username
$squid_ftp_username="root";
// FTP Password
$squid_ftp_password="root";
// Squid user config file
$squid_user_config="squid.conf";
// timeout, seconds
$squid_timeout=5;
// poller interval, seconds
$poller_interval=10;
// poller history
$poller_history=50;
// browser refresh interval
$browser_refresh=30;

?>