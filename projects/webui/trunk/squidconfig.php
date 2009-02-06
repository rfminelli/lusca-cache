<?php
/**
 * List of config items from squid.conf
 * It's sorted by groups and most settings have defaults
 */

$squid_config=array(
"auth_param"=>array(
	"default"=>"",	// setting defaults
	"multiline"=>true,		// single line settings or multiline allows?
	"group"=>"set_auth",	// tagged group to menu settings
	"input"=>"text",			// input validation types (integer,float,time,text,variable)
	"depends"=>"",			// on what other settings this depends on?
	"boxtype"=>"textarea",			// input box type (box or textarea)
	"boxsize"=>"64",		// width of the input box (so that it can hold the whole lone)
	"comment"=>"This is used to define parameters for the various authentication schemes supported by Squid.",		// commants/usage on this settings
	"more_link"=>"http://www.squid-cache.org/Versions/v2/2.7/cfgman/auth_param.html",		// read more info about this settings from here
),
"authenticate_cache_garbage_interval"=>array(
	"default"=>"1 hour",	// setting defaults
	"multiline"=>false,		// single line settings or multiline allows?
	"group"=>"set_auth",	// tagged group to menu settings
	"input"=>"text",		// input validation types (integer,decimal,text,etc...)
	"depends"=>"",			// on what other settings this depends on?
	"boxtype"=>"box",		// input box type (box or textarea)
	"boxsize"=>"16",		// width of the input box (so that it can hold the whole lone)
	"comment"=>"The time period between garbage collection across the username cache. This is a tradeoff between memory utilization (long intervals - say 2 days) and CPU (short intervals - say 1 minute). Only change if you have good reason to.",		// commants/usage on this settings
	"more_link"=>"http://www.squid-cache.org/Versions/v2/2.7/cfgman/authenticate_cache_garbage_interval.html",		// read more info about this settings from here
	),
"authenticate_ttl"=>array(
	"default"=>"1 hour",	// setting defaults
	"multiline"=>false,		// single line settings or multiline allows?
	"group"=>"set_auth",	// tagged group to menu settings
	"input"=>"text",		// input validation types (integer,decimal,text,etc...)
	"depends"=>"",			// on what other settings this depends on?
	"boxtype"=>"box",		// input box type (box or textarea)
	"boxsize"=>"16",		// width of the input box (so that it can hold the whole lone)
	"comment"=>"The time a user & their credentials stay in the logged in user cache since their last request. When the garbage interval passes, all user credentials that have passed their TTL are removed from memory.",		// commants/usage on this settings
	"more_link"=>"http://www.squid-cache.org/Versions/v2/2.7/cfgman/authenticate_ttl.html",		// read more info about this settings from here
	),
"authenticate_ip_ttl"=>array(
	"default"=>"0 seconds",	// setting defaults
	"multiline"=>false,		// single line settings or multiline allows?
	"group"=>"set_auth",	// tagged group to menu settings
	"input"=>"text",		// input validation types (integer,decimal,text,etc...)
	"depends"=>"",			// on what other settings this depends on?
	"boxtype"=>"box",		// input box type (box or textarea)
	"boxsize"=>"16",		// width of the input box (so that it can hold the whole lone)
	"comment"=>"If you use proxy authentication and the 'max_user_ip' ACL, this directive controls how long Squid remembers the IP addresses associated with each user.  Use a small value (e.g., 60 seconds) if your users might change addresses quickly, as is the case with dialups. You might be safe using a larger value (e.g., 2 hours) in a corporate LAN environment with relatively static address assignments.",		// commants/usage on this settings
	"more_link"=>"http://www.squid-cache.org/Versions/v2/2.7/cfgman/authenticate_ip_ttl.html",		// read more info about this settings from here
	),
"authenticate_ip_shortcircuit_ttl"=>array(
	"default"=>"0 seconds",	// setting defaults
	"multiline"=>false,		// single line settings or multiline allows?
	"group"=>"set_auth",	// tagged group to menu settings
	"input"=>"text",		// input validation types (integer,decimal,text,etc...)
	"depends"=>"",			// on what other settings this depends on?
	"boxtype"=>"box",		// input box type (box or textarea)
	"boxsize"=>"16",		// width of the input box (so that it can hold the whole lone)
	"comment"=>"Cache authentication credentials per client IP address for this long. Default is 0 seconds (disabled).",		// commants/usage on this settings
	"more_link"=>"http://www.squid-cache.org/Versions/v2/2.7/cfgman/authenticate_ip_shortcircuit_ttl.html",		// read more info about this settings from here
	),
);

?>