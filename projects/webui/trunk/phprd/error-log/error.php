<?php

/**
 * @author 
 * @copyright 2008
 */

require_once "phprd/error-log/log.php";

class ErrorHandler {

	private $error_array=array();
	private $log;

 /**
 * Construct error class
 *  
 */

	public function __construct() {
		$this->log=new Logging or die("No log component");
	}

 /**
 * Destruct error class
 *  
 */

	public function __destruct() {
		$this->log->free;
	}

 /**
 * Add error message to error queue
 * 
 * @error error message
 * @return nothing 
 *   
 */

	public function adderror ($error) {
		$this->error_array[]=$error;
		$this->log->addtolog(SEVERITY_ERROR,$error);
	}

 /**
 * Add error, print it and halt execution
 * 
 * @error error message
 * @return nothing 
 *   
 */

	public function add_print_die($error) {
		$this->adderror($error);
		$this->print_die();
	}

 /**
 * Print errors already in queue and halt execution
 * 
 * @return nothing 
 *   
 */

	public function print_die() {
		if (is_array($this->error_array) && !empty($this->error_array))
		foreach ($this->error_array as $id=>$errors) {
			print "<b>Error</b> id <font color='red'><b>$id</b></font> message <font color='red'>$errors</font> <br>";
		}
		die('<br><b><font color="Red">Program terminated!</font></b>');
	}

 /**
 * Returns array with error list
 * 
 * @return error array 
 *   
 */

	public function get_errors() {
		return $this->$error_array;
	}


}

?>