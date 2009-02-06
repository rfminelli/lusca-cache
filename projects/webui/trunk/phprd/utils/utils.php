<?php

/**
 * @author dr4g0n
 * @copyright 2008
 */

include_once("string.php");
include_once("install.php");

class bench {
	private $start_time;
	
	public function start() {
		$this->start_time=microtime(true);
	}
	
	public function end() {
		return microtime(true)-$this->start_time;
	}
	
	public function bench_code ($code) {
	$start_time=microtime(true);
	eval($code);
	return microtime(true)-$start_time;
	}
}

?>