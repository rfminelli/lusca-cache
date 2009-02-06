<?php
/**
 * Squid config processing/dependicies
 * This checks for sanity of the input
 * So if user did misconfigure something
 * like enter invalid config or
 * settings that collide with other settings
 * we process that sanity here and let user know
 * of possible problems
 * Only "cased" vars are checked, others are
 * assumed to be ok
 *
 * @author dr4g0n
 * @copyright 2008
 */

/**
 * Check the settings
 * If the settings depend on other one
 * it reports the message
 * If it's invalid/impossible, it reports that too
 * 
 * @input none
 * @returns none
 * 
 */	
function check_settings($var,$value) {
	$message=false;
	switch ($var) {
		case "auth_param":
		break;
		default:
		}
	return $message;
	}

?>