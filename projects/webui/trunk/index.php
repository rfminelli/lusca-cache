<?php
/**
 * Squid Manager "hive"
 * @author dr4g0n
 * @copyright 2008
 */

require_once "core_init.php";
require_once "squidconfig.php";

$menu=$_REQUEST['menu'];
$submit=$_REQUEST['submit1'];

// main init/deinit
$body="SETTINGS NOT YET IMPLEMENTED!";		// Default message if not assigned anywere
unset($menu_override);						// Menus that are not settings - overriden
$sqm_version="1.0a";						// Squid manager version
$server=$_SERVER["SERVER_SOFTWARE"];		// On which OS is PHP run?

// detect necessay mods/plugins/addons
if (!extension_loaded('gd') || !function_exists("gd_info"))
	die("Your php don't have GD library enabled.<br>
	Find php.ini file and uncomment line \"extension=php_gd2\"");

// Other menus/override
switch ($menu) {
// Upper menus
	case "retrieve_config":
	unset($user_config);
	$user_config=$webfunc->retrieve_config();
	if ($user_config)
		$body="Config sucessfully retrieved"; else
		$body="Problem with retrieving config!";
	$menu_override=true;
	break;
	case "save_config":
	$res=$webfunc->save_config($user_config);
	if ($res)
		$body="Config sucessfully saved"; else
		$body="Problem with saving config!";
	$menu_override=true;
	break;
// Monitoring	
	case "monitor_requests":
	$body=$webfunc->generate_graphs("requests");
	$menu_override=true;
	break;
	case "monitor_hits":
	$body=$webfunc->generate_graphs("hits");
	$menu_override=true;
	break;
	case "monitor_errors":
	$body=$webfunc->generate_graphs("errors");
	$menu_override=true;
	break;
	case "monitor_dns":
	$body=$webfunc->generate_graphs("dns");
	$menu_override=true;
	break;
	case "monitor_memory":
	$body=$webfunc->generate_graphs("memory");
	$menu_override=true;
	break;
	case "monitor_storage":
	$body=$webfunc->generate_graphs("storage");
	$menu_override=true;
	break;
	case "monitor_cpu":
	$body=$webfunc->generate_graphs("cpu");
	$menu_override=true;
	break;
	case "monitor_system":
	$body=$webfunc->generate_graphs("system");
	$menu_override=true;
	break;
	case "monitor_other":
	$body=$webfunc->generate_graphs("other");
	$menu_override=true;
	break;
	default:
	}

// Settings are submitted
if ($submit)
	switch ($submit) {
		case "submit this one":
		$webfunc->submit($submit);
		$body="<i>Settings accepted!</i>";
		break;
		case "Search":
		$item=$_REQUEST['keywords'];
		if (empty($item))
			$body="Search term is empty!"; else
		$body=$webfunc->search($item,$squid_config);
		break;
		default:
		$body=$webfunc->submit($submit);
		}

// Generate content of body that depends on menu accessed, if not found, display no page message
if (!empty($menu) && !$menu_override) {
$body="<form action=\"index.php\" method=post>";
$body.=$webfunc->generate_settings($menu,$user_config);		// Generate settings for that menu
$body.="<table width='100%'><tr>";
$body.="<td align='left'><input type='reset' value='reset'></td><td align='center'><input type='submit' name='Revert' value='revert'></td><td align='right'><input type='submit' name='submit1' value='$menu'></td>";
$body.="</tr></table></form>";
}

// Page generation
$templ->get_page("index");	// Get the page template
$tags=array(		// Assign page tags
"BODY"=>$body,
"TEMPLATE_PATH"=>$template_path,
"MENU_CODE"=>$webfunc->generate_menu($menus),
"sqm_version"=>$sqm_version,
"server"=>$server,
// This should be the last to precisely measure the time
"generation_time"=>substr($bench->end(),0,6),
);

$templ->assign_var($tags);		// Match the tags on the page
$page=$templ->finalize_page();	// Finalize the page content
print $page;					// Output the page content

?>