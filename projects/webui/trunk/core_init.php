<?php
/**
 * This initializes engine, include all necessary php files and instances the classes
 *
 * @author dr4g0n
 * @copyright 2008
 */

// Required modules
require_once "phprd/utils/utils.php";
$bench=new bench; $bench->start();	//Bench first

// configs
require "config.php";
require_once "menus.php";

// phprd
require "phprd/template/template.php";
require_once "phprd/users/users.php";
require_once "phprd/ajax/ajax.php";

// diagram
require "graphs.php";
$graph=new graphs;

// internal functions
require_once "web_functions.php";
require_once "interface.php";

// Object instancing
$templ=new template;
$templ->set_template_path($template_path);
$ajax=new ajax;
$users=new user_sessions;
$webfunc=new webfunc;
$squid_interface=new squid_interface;
$user_config=$squid_interface->get_local_config();

// avoid page caching && expires in the past
header('Expires: Thu, 19 Nov 1981 08:52:00 GMT');
header('Cache-Control: no-store, no-cache, must-revalidate, post-check=0, pre-check=0 Pragma: no-cache');

?>