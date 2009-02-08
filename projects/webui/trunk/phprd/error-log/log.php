<?php

/**
 * @author 
 * @copyright 2008
 */

DEFINE ('NO_LOGGING',0);
DEFINE ('LOG_FILE',1);
DEFINE ('LOG_OUTPUT',2);
DEFINE ('LOG_SQL',4);

DEFINE ('SEVERITY_NOTICE',1);
DEFINE ('SEVERITY_INFO',2);
DEFINE ('SEVERITY_WARNING',3);
DEFINE ('SEVERITY_ERROR',4);
DEFINE ('SEVERITY_CRITICAL',5);

class Logging {
	
	private $log_mode=1;
	private $log_file="logging.log";
	
	private function addtofile($filename,$buffer) {
		$fhandle=fopen($filename,'a+');
		fwrite($fhandle,$buffer."\r\n");
		fclose($fhandle);
	}

	private function addtooutput($message) {
		print"<br><b>Log message: <font color=red>$message</font></b><br>";
	}

	public function setlogging($mode=LOG_FILE,$filepath="logging.log") {
		//if (!is_integer($mode))
		if ($mode<0 || $mode>3) return false;
		$this->log_mode=$mode;
		$this->log_file=$filepath;
		return true;
	}
	
	public function addtolog($severity,$message) {
		switch ($severity) {
			case SEVERITY_NOTICE:
			$severity_message='NOTICE';
			break;
			case SEVERITY_INFO:
			$severity_message='INFO';
			break;
			case SEVERITY_WARNING:
			$severity_message='WARNING';
			break;
			case SEVERITY_ERROR:
			$severity_message='ERROR';
			break;
			case SEVERITY_CRITICAL:
			$severity_message='CRITICAL';
			break;
			default:
			$severity_message='NONE';
		}
		$message="[".date('H:m:s d-m-Y')."] ($severity_message) ".$message;
		if ($this->log_mode & LOG_FILE) self::addtofile($this->log_file,$message);
		if ($this->log_mode & LOG_OUTPUT) self::addtooutput($message);
		//TODO: MYSQL LOGGING
		if ($this->log_mode & LOG_SQL) {}
		
	}
}

?>