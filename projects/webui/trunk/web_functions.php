<?php
/**
 * Web manipulation functions
 *
 * @author dr4g0n
 * @copyright 2008
 */

class webfunc {

private $config_interface=NULL;
private $graph=NULL;

/**
 * Constructor method for web functions
 * Created new instance of class "squid_interface"
 * that is used later on in functions
 * 
 * @input none
 * @returns none
 * 
 */		

public function __construct() {
	if (!$this->config_interface) 
		$this->config_interface=new squid_interface;
	}

/**
 * Destructor method for web functions
 * Destroys instance of class "squid_interface"
 * 
 * @input none
 * @output none 
 * @returns none
 * 
 */	
 
public function __destruct() {
	if (!$this->config_interface) 
		$this->config_interface->free;
	}

/**
 * Searches specific item
 * and return that one as html
 * 
 * @item item to search
 * @returns html body with inserted JS message
 * 
 */
 public function search($item, array $squid_config) {
 	global $user_config;
	 foreach ($squid_config as $config=>$config_array) {
 		if (strstr($config,$item)) {
 			$body="<form action=\"index.php\" method=post>";
			$generated.=$this->generate_one_setting($config,$user_config);		// Generate settings for that menu
			$body.=$generated."<table width='100%'><tr>";
			$body.="<td align='left'><input type='reset' value='reset'></td><td align='center'><input type='submit' name='Revert' value='revert'></td><td align='right'><input type='submit' name='submit1' value='submit this one'></td>";
			$body.="</tr></table></form>";
			return $body;
 			}
		}
		return "<i>No settings found, try another term.</i>";
	}

/**
 * When user submits the form, the submits
 * are processed here 
 * 
 * @submit_group submit config group
 * @returns html body with inserted JS message
 * 
 */

public function submit($submit_group) {
	global $squid_config,$user_config;
	require_once "dependicies.php";
	$nondefault=0;$changes=0;$comment=false;
	foreach ($squid_config as $sc_var=>$sc_value) {
		foreach ($_REQUEST as $req_var=>$req_value) {
			if ($sc_var==$req_var) {
				if ($sc_value['default']!=$req_value)
					$nondefault++;
				if ($user_config[$req_var]!=$req_value)
					$changes++;
				$result=check_settings($req_var,$req_value);
				if ($result) {
					$comment.="$req_var: $result<br>";
					}
				$user_config[$req_var]=$req_value;
				}
			}
		}
	$this->config_interface->set_local_config($user_config);
	$body="<script type='text/javascript'>\n
	alert('Settings accepted - {$changes} changes, {$nondefault} non-default settings!');\n
	window.location = 'index.php?menu={$submit_group}&confirmed=1&comment=$comment';\n
	</script>\n";
	return $body;
	}

/**
 * This function generates one graphs
 * and return the link to image
 * It is called from generate_graphs function
 * in a block
 * 
 * @poll polling storage (history)
 * @poller_section_name name of the poller section
 * @var graph variable name
 * @returns html body with image links
 * 
 */

private function generate_one_graph($poll,$poller_section_name,$var) {
		$var_cleaned = ereg_replace("[^A-Za-z0-9]", "", $var);
		$graph_filename="graph_{$poller_section_name}_{$var_cleaned}.png";
		$data=$poll[$var];
		$this->graph->create_diagram($graph_filename,"bar","","",$var,$data);
		return "<IMG src='{$graph_filename}' usemap='#map1' border=0><br />\n";
	}

/**
 * This function generates graphs
 * The graphs are generated on
 * the config files (poller_config)
 * which holds all needed graphs
 * to be generated.
 * This function itterate them and
 * generates pictures and html code.
 *
 * If you want to add new graph
 * you need to place it here in
 * appropriate section!
 * 
 * @section section to be generated
 * @returns html body with image links
 * 
 */

public function generate_graphs($section) {
	global $browser_refresh;
	require_once "poller_config.php";
	if (!$this->graph)
		$this->graph=new graphs;
	$poll=$this->config_interface->get_local_storage_file();
	unset($body);
	switch ($section) {
		case "requests":
		$body.=$this->generate_one_graph($poll,"info","Number of HTTP requests received");
		$body.=$this->generate_one_graph($poll,"info","Average HTTP requests per minute since start");
		$body.=$this->generate_one_graph($poll,"info","HTTP Requests (All)");
		$body.=$this->generate_one_graph($poll,"5min","client_http.requests");
		$body.=$this->generate_one_graph($poll,"5min","client_http.kbytes_in");
		$body.=$this->generate_one_graph($poll,"5min","client_http.kbytes_out");
		$body.=$this->generate_one_graph($poll,"5min","client_http.all_median_svc_time");
		$body.=$this->generate_one_graph($poll,"5min","client_http.miss_median_svc_time");
		$body.=$this->generate_one_graph($poll,"5min","client_http.hit_median_svc_time");
		$body.=$this->generate_one_graph($poll,"5min","server.ftp.requests");
		$body.=$this->generate_one_graph($poll,"5min","server.other.requests");
		break;
		case "hits":
		$body.=$this->generate_one_graph($poll,"info","Cache Hits");
		$body.=$this->generate_one_graph($poll,"info","Near Hits");
		$body.=$this->generate_one_graph($poll,"info","Cache Misses");
		$body.=$this->generate_one_graph($poll,"5min","client_http.hits");
		break;
		case "errors":
		$body.=$this->generate_one_graph($poll,"5min","client_http.errors");
		break;
		case "dns":
		$body.=$this->generate_one_graph($poll,"info","DNS Lookups");
		$body.=$this->generate_one_graph($poll,"5min","dns.median_svc_time");
		break;
		case "memory":
		$body.=$this->generate_one_graph($poll,"info","Total space in arena");
		$body.=$this->generate_one_graph($poll,"info","Total in use");
		$body.=$this->generate_one_graph($poll,"info","Total free");
		$body.=$this->generate_one_graph($poll,"info","Process Data Segment Size via sbrk()");
		break;
		case "storage":
		$body.=$this->generate_one_graph($poll,"storedir","Store Entries");
		$body.=$this->generate_one_graph($poll,"storedir","Current Store Swap Size");
		$body.=$this->generate_one_graph($poll,"storedir","Current Capacity");
		break;
		case "cpu":
		$body.=$this->generate_one_graph($poll,"info","CPU Usage");
		$body.=$this->generate_one_graph($poll,"info","CPU Usage, 5 minute avg");
		$body.=$this->generate_one_graph($poll,"info","CPU Usage, 60 minute avg");
		break;
		case "system":
		$body.=$this->generate_one_graph($poll,"info","Largest file desc currently in use");
		$body.=$this->generate_one_graph($poll,"info","Number of file desc currently in use");
		$body.=$this->generate_one_graph($poll,"5min","select_loops");
		$body.=$this->generate_one_graph($poll,"5min","select_fds");
		$body.=$this->generate_one_graph($poll,"5min","syscalls.polls");
		$body.=$this->generate_one_graph($poll,"5min","syscalls.disk.opens");
		$body.=$this->generate_one_graph($poll,"5min","syscalls.disk.reads");
		$body.=$this->generate_one_graph($poll,"5min","syscalls.disk.writes");
		break;
		case "other":
		$body.=$this->generate_one_graph($poll,"5min","icp.pkts_sent");
		$body.=$this->generate_one_graph($poll,"5min","icp.pkts_recv");
		break;
		default:
		die($section);
		}
// Add refresh in meta tags
$body.= <<<REFRESH
<meta http-equiv="refresh" content="$browser_refresh"/>
REFRESH;
	return $body;
	}

/**
 * Generates html with settings
 * and their values.
 * It itterates trough all settings
 * defined in $squid_config
 * and generates the html to display
 * based on choosed group ($group)
 * 
 * @group settings group
 * @user_config user config (menus)
 * @returns html body
 * 
 */

public function generate_settings($group,$user_config=NULL) {
		global $squid_config;
		if (!is_array($squid_config) || empty($squid_config))	// if squid_config is invalid somehow, just return false
			return false;
		$links="<i>Quick variable access:</i><br>\n";
		$confirmed=$_REQUEST['confirmed'];
		$settings_comment=$_REQUEST['comment'];
		if ($confirmed=="1")
			$image="ok.gif";
			else
			$image="question.gif";
		foreach ($squid_config as $sc_var=>$sc_line) {		// process each config array
			$value=$this->config_interface->get_config($user_config,$sc_var);
			if ($sc_line["group"]==$group) {
				$comment=$sc_line["comment"];
				$group=$sc_line["group"];
				$multiline=$sc_line["multiline"];
				$more_link=$sc_line["more_link"];
				$default=$sc_line["default"];
				if (!$default)
					$default=$sc_line["default"];
				$box_type=$sc_line["boxtype"] or $box_type="box";
				$box_size=$sc_line["boxsize"] or $box_size="64";
				if (empty($value))
					$value=$default;
				$rows=count(explode("\n",$value));
				if (!empty($more_link))
					$more_link="<a href='' onclick=\"javascript:window.open('{$more_link}','','scrollbars=no,menubar=no,height=600,width=800,resizable=yes,toolbar=no,location=no,status=no');\"> <i>more</i> </a>";
				$body.="<h3><a href='#{$sc_var}'>{$sc_var}</a></h3><br>\n";
				$body.="<b>{$sc_var}</b>&nbsp";
				if ($box_type=="textarea")
					$body.="<textarea name='{$sc_var}' id='{$sc_var}' cols='{$box_size}' rows='$rows'>{$value}</textarea>"; else
					$body.="<input type='{$box_type}' name='{$sc_var}' id='{$sc_var}' size='{$box_size}' value='{$value}' />";
				$body.="&nbsp&nbsp<img src='%TEMPLATE_PATH%/images/{$image}' alt='aa' /><br><br>\n";
				$body.="<h4><i>Default:</i> {$default}<h4><br>\n";
				$body.="<h4>Usage: {$comment} {$more_link}<h4><br>\n";
				$links.="<a href='#{$sc_var}'>{$sc_var}</a><br>\n";
				}
			}
		if (!empty($settings_comment))
			$output=$links.'<br>'.$body."<br><table border=0 width='100%'><tr><td bgcolor='#00FF00'><b>Comments</b><br>".$settings_comment."</td></tr></table>"; else
			$output=$links.'<br>'.$body;
		return $output;
	}

/**
 * Retrieve squid config from file
 * 
 * @returns true if sucessful, false otherwise
 * 
 */
 
public function retrieve_config() {
	global $squid_config_method,$squid_config;
	if ($squid_config_method=="file") {
		$fh=@fopen("squid.conf","r");
		if (!$fh)
			return false;
		unset($user_config);	//reset config
		while (!feof($fh)) {
			$sbuf=fgets($fh,4096);
			if (strlen($sbuf)>3 && $sbuf[0]!="#") {	//not commented
				$sbuf_array=explode(" ",$sbuf);	//separate command from values
				$sbuf_command=trim($sbuf_array[0]);
				unset($sbuf_value);
				for ($c=1;$c<=count($sbuf_array);$c++)
					$sbuf_value.=$sbuf_array[$c]." ";
				$sbuf_value=trim($sbuf_value)."\n";
				if (isset($squid_config[$sbuf_command]) && !empty($sbuf_value))	//settings exist in the squidconfig?
					$user_config[$sbuf_command].=$sbuf_value;
				}
			}
		fclose($fh);
		}
	$this->config_interface->set_local_config($user_config);	//save config
	return true;
	}

/**
 * Save user config to squid config file
 * 
 * @user_config user config array
 * @returns true if sucessful, false otherwise
 * 
 */

public function save_config($user_config) {
	global $squid_config_method;
	if (empty($user_config) || !is_array($user_config))
		return false;
	unset($filebuffer);
	foreach($user_config as $uc_var=>$uc_value) {
		if (strstr($uc_value,chr(10))) {	// multiline command
			$uc_value_array=explode(chr(10),$uc_value);
			foreach ($uc_value_array as $uc_value_array_element)
				if (!empty($uc_value_array_element))
					$filebuffer.=$uc_var." ".$uc_value_array_element."\r";
			} else {	// not a multiline command
			$uc_value.="\r";
			$filebuffer.=$uc_var." ".$uc_value;
			}
		}
	if ($squid_config_method=="file") {
		$fh=@fopen("squid.conf.webmanager","w+") or die("cannot open file for writing");
		fwrite($fh,$filebuffer);
		fclose($fh);
		}
	return true;
	}

/**
 * Generates html with one setting
 * and values.
 * 
 * @setting_name settings name (variable name)
 * @user_config user config (menus)
 * @returns html body
 * 
 */

public function generate_one_setting($setting_name,$user_config=NULL) {
		global $squid_config;
		if (!is_array($squid_config) || empty($squid_config))	// if squid_config is invalid somehow, just return false
			return false;
		$links="<i>Quick variable access:</i><br>\n";
		$confirmed=$_REQUEST['confirmed'];
		$settings_comment=$_REQUEST['comment'];
		if ($confirmed=="1")
			$image="ok.gif";
			else
			$image="question.gif";
		foreach ($squid_config as $sc_var=>$sc_line) {		// process each config array
			$value=$this->config_interface->get_config($user_config,$sc_var);
			if ($sc_var==$setting_name) {
				$comment=$sc_line["comment"];
				$group=$sc_line["group"];
				$multiline=$sc_line["multiline"];
				$more_link=$sc_line["more_link"];
				$default=$sc_line["default"];
				if (!$default)
					$default=$sc_line["default"];
				$box_type=$sc_line["boxtype"] or $box_type="box";
				$box_size=$sc_line["boxsize"] or $box_size="64";
				if (empty($value))
					$value=$default;
				$rows=count(explode("\n",$value));
				if (!empty($more_link))
					$more_link="<a href='' onclick=\"javascript:window.open('{$more_link}','','scrollbars=no,menubar=no,height=600,width=800,resizable=yes,toolbar=no,location=no,status=no');\"> <i>more</i> </a>";
				$body.="<h3><a href='#{$sc_var}'>{$sc_var}</a></h3><br>\n";
				$body.="<b>{$sc_var}</b>&nbsp";
				if ($box_type=="textarea")
					$body.="<textarea name='{$sc_var}' id='{$sc_var}' cols='{$box_size}' rows='$rows'>{$value}</textarea>"; else
					$body.="<input type='{$box_type}' name='{$sc_var}' id='{$sc_var}' size='{$box_size}' value='{$value}' />";
				$body.="&nbsp&nbsp<img src='%TEMPLATE_PATH%/images/{$image}' alt='aa' /><br><br>\n";
				$body.="<h4><i>Default:</i> {$default}<h4><br>\n";
				$body.="<h4>Usage: {$comment} {$more_link}<h4><br>\n";
				$links.="<a href='#{$sc_var}'>{$sc_var}</a><br>\n";
				}
			}
		if (empty($body))
			return false;
			
		if (!empty($settings_comment))
			$output=$links.'<br>'.$body."<br><table border=0 width='100%'><tr><td bgcolor='#00FF00'><b>Comments</b><br>".$settings_comment."</td></tr></table>"; else
			$output=$links.'<br>'.$body;
		return $output;
	}

/**
 * Generates menu on the left
 * and their brnaches.
 * The menu/info is get from
 * menu config (array).
 * 
 * @menus array of menus to be generated
 * @returns html body
 * 
 */

public function generate_menu (array $menus) {
	$output="<ul id=\"verticalmenu\" class=\"glossymenu\">\n";
	foreach ($menus as $menu_title=>$menu_link) {
		if (!is_array($menu_link))
		 $output.="<li><a href=\"{$menu_link}\">{$menu_title}</a></li>\n"; else {
			  $output.="<li><a href=\"#\">{$menu_title}</a>\n<ul>";
			 foreach ($menu_link as $submenu_tile=>$submenu_link) {
		 		if (!is_array($submenu_link))
				 $output.="<li><a href=\"{$submenu_link}\">{$submenu_tile}</a></li>\n";
				}
				$output.="</ul>";
			}
		}
	$output.="</ul>";
	return $output;
	}

}

?>