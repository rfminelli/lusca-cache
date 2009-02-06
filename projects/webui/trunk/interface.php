<?php
/**
 * Interface to squid
 *
 * @author dr4g0n
 * @copyright 2008
 */

class squid_interface {

	private $method="ftp";	//squid_manager/command_port/ftp
	private $last_buffer=NULL;
	private $last_buffer_item=NULL;

/**
 * 
 * Read squid config to local config file
 * Source can be ftp (other access methods may be in te future)
 * 
 * @input none
 * @returns none
 * 
 */

	public function get_local_config() {
		global $retrieve_method,$local_config;
			if (file_exists($local_config)) {
			$fh=@fopen($local_config,'r');
			while (!feof($fh))
				$file.=@fread($fh,1000);
			@fclose($fh);
			if (!empty($file))
				$user_config=unserialize($file);
				return $user_config;
			}
	return false;
	}

/**
 * 
 * Write local config file to squid
 * Source can be ftp (other access methods may be in the future)
 * 
 * @input none
 * @returns none
 * 
 */

	public function set_local_config($user_config) {
		global $retrieve_method,$local_config;
		$serialize_config=serialize($user_config);
		$fh=@fopen($local_config,'w+');
		@fwrite($fh,$serialize_config);
		@fclose($fh);
		return true;
		}

/**
 * 
 * Loads config from defined source
 * Source can be ftp, command_port or squid_manager
 * 
 * @user_config this is user custom config
 * @config_item specific item
 * @returns false if something goes wrong
 * 
 */

	public function get_config(&$user_config,$config_item) {
		global $retrieve_method,$squid_user_config;
		switch ($retrieve_method) {
			case "ftp":
			if (!empty($user_config)) {
					return $user_config[$config_item];
					}
			break;
			case "command_port":
			break;
			case "squid_manager":
				return $this->manager_get($config_item);
			break;
			default:
			}
		return false;
		}

/**
 * 
 * Stores config to defined source
 * Source can be ftp, command_port or squid_manager
 * 
 * @config_item specific item
 * @value value of the item
 * @user_config local user config
 * @returns false if something goes wrong
 * 
 */

	public function set_config($config_item,$value,$user_config=NULL) {
		global $retrieve_method;
		switch ($retrieve_method) {
			case "ftp":
				if (empty($user_config))
					return false;
				set_local_config($user_config);
				return true;
			break;
			case "command_port":
			return true;
			break;
			case "squid_manager":
				return $this->manager_set($config_item,$value);
			break;
			default:
			}
		}

/**
 * 
 * Polling manager
 * Polls the data from the squid manager
 * 
 * @item_group the group of the item
 * @item item itself
 * @returns false if there was no reply/empty one
 * 
 */

	public function poll_manager($item_group,$item) {
		$reply=$this->manager_get($item_group);
		$reply=$this->manager_getvar($reply,$item);
		if (empty($reply))
			$reply=false;
		return $reply;
		}

/**
 * 
 * Lower level polling function
 * Called from poll_manager
 * 
 * @config_item the group of item specified in squid manager
 * @returns reply from web
 * 
 */
	
	private function manager_get($config_item) {
		global $squid_host,$squid_port,$squid_password,$squid_timeout;
		// cache the item so we don't have to poll every time and we'll same some bw and resources
		if ($this->last_buffer_item==$config_item && empty($this->last_buffer))
			return $this->last_buffer;
		$socket=fsockopen($squid_host,$squid_port,$errno,$errstr,$squid_timeout);
		if (!$socket)
			return false;
		fwrite($socket,"GET cache_object://10.0.0.3/$config_item HTTP/1.0\nAccept: */*\n\n");
		$reply="";
		while (!feof($socket)) 
			$reply.=fread($socket,10000);
		return $reply;
		}

/**
 * 
 * Set vars to squid manager
 * Not implemented so far
 * 
 * @item item to be set
 * @value new value
 * @returns nothing
 * 
 */

	private function manager_set($item,$value) {
		global $squid_host,$squid_port,$squid_password;
		} 

/**
 * 
 * Extract the var's value from returned answer
 * from squid manager's web interface
 * 
 * @item item to get
 * @var var to get
 * @returns extracted value
 * 
 */

	public function manager_getvar($items,$var) {
		$lines=explode("\n",$items);
		foreach ($lines as $line) {
			if (strstr($line,$var)) {
				$value=substr($line,strlen($var)+2,strlen($line)-strlen($var)-2);
				$value=trim($value," %=:");
				$value_exp=explode(" ",$value);
				if (count($value_exp)>1)
					$value=$value_exp[0];
				return $value;
				}
			}
		}
		
	public function get_local_storage_file() {
		if (!file_exists("data_storage"))
			return;
		$fh=@fopen("data_storage","r");
		if (!$fh)
			die('Cannot open storage file!');
		unset($ser_storage);
		while (!feof($fh))
			$ser_storage.=@fread($fh,1000);
		$storage=unserialize($ser_storage);
		@fclose($fh);
		return $storage;
		}
	
	public function save_local_storage_file($storage) {
		$fh=@fopen("data_storage","w+");
		if (!$fh)
			die('Cannot create storage file!');
		$ser_storage=serialize($storage);
		@fwrite($fh,$ser_storage);
		@fclose($fh);
		}

}

?>