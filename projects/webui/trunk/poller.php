<?php
/*
SQUID Web Management config file
@author dr4g0nsr
*/

require_once "core_init.php";
require_once "poller_config.php";

function logmessages($message) {
	$time=time();
	$message="[{$time}] $message\n";
	$fh=fopen("poller.log","w+");
	if (!$fh)
		return;
	fwrite($fw,$message);
	fclose($fh);
	if ($_SERVER["HTTP_USER_AGENT"])	// is poller runned from the web or standalone?
		print "$message<br>"; else		// web
		print "$message\n";				// standalone
	}

print "Start polling in $store_data ...\n";

if ($store_data=="file") {
print "Retrieving history data ...\n";
$poll=$squid_interface->get_local_storage_file();
//var_dump($pool);die;
if (time()<$poll["last_poll"]+$poller_interval) {
	print "Scheduler polled too quick, exiting!\n";
	die;
	}
print "Getting data from squid manager ...\n";
if (!$poll)		// in the case that file is empty/not created
	$poll=array();
foreach ($poller_config as $manager_section=>$manager_var_array)
	foreach ($manager_var_array as $manager_var) {
		$value=$squid_interface->poll_manager($manager_section,$manager_var);
		while (count($poll[$manager_var])>=$poller_history)
			array_shift($poll[$manager_var]);
		$poll[$manager_var][]=$value;
}
$poll["last_poll"]=time();
print "Saving data\n";
if (!empty($poll))
	$squid_interface->save_local_storage_file($poll);
} else
if ($store_data=="mysql") {
	}

print "Poller has finished!\n";
?>