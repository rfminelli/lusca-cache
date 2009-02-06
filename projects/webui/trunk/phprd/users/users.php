<?php

/**
 * @author dr4g0n
 * @copyright 2008
 */

class user_sessions {
	
	private $storage="UNDEFINED";
	
	public function __construct() {
		
	}
	
	public function set_storage($storage) {
		if (empty($storage)) return false;
		$this->storage=$storage;
		return true;
	}
	
	public function check_session($db,$table,$passwordmethod='md5') {
		switch ($this->storage) {
			case "file":
			
			break;
			case "mysql":
				return $this->check_session_mysql($db,$table,$passwordmethod='md5');
			break;
			default:
				return false;
		}
		}
		
	public function login_user($username,$password,$userdb) {
			switch ($this->storage) {
			case "file":
				return $this->login_file($username,$password,$userdb);
			break;
			case "mysql":
				return $this->login_mysql($db,$table,$passwordmethod='md5');
				break;
			default:
			return false;
		}
	}
	
	public function create_user($username,$password,$email,$group,$userdb) {
			switch ($this->storage) {
			case "file":
				return $this->create_user_file($username,$password,$email,$group,$userdb);
			break;
			case "mysql":
				break;
			default:
			return false;
		}
	}	
	private function check_session_mysql (mysql &$db,$table="users",$passwordmethod='md5') {
		if (!$db) die('No database set!');
		session_start();
		$s_username=$_SESSION['s_username'];
		$s_password=$_SESSION['s_password'];
		if (empty($s_username) || empty($s_password)) return false;
		if (empty($_SESSION['online'])) $_SESSION['online']=time();
		switch ($passwordmethod) {
			case "md5":
			$finduser=array("username"=>$s_username,"password"=>md5($s_password));
			break;
			case "sha1":
			$finduser=array("username"=>$s_username,"password"=>sha1($s_password));
			break;
			default:	//plaintext
			$finduser=array("username"=>$s_username,"password"=>$s_password);
		}
		$res=$db->get_row($table,$finduser);
		if (!$res) return false;
		return true;
	}

	private function login_mysql (mysql &$db,$table,$s_username,$s_password,$passwordmethod='md5') {
		if (empty($db) || empty($table) || empty($s_username) || empty($s_password)) return false;
		switch ($passwordmethod) {
			case "md5":
			$finduser=array("username"=>$s_username,"password"=>md5($s_password));
			break;
			case "sha1":
			$finduser=array("username"=>$s_username,"password"=>sha1($s_password));
			break;
			default:	//plaintext
			$finduser=array("username"=>$s_username,"password"=>$s_password);
		}
		$res=$db->get_row($table,$finduser);
		if (!$res) return false;
		$_SESSION['s_username']=$s_username;
		$_SESSION['s_password']=$s_password;		
		return true;
	}
	
	private function login_file($username,$password,$userdb) {
		if (empty($username) || empty($password) || empty($userdb))
			return false;
		if (!file_exists($userdb))
			return false;
		$fh=fopen($userdb,"r");			//open db in read-only mode
		while (!feof($fh))
			$users.=fread($fh,999999);	//read all accounts-warning, very big userdb can eat alot of mem
		fclose($fh);
		$users=explode("\r\n",$users);	//parse all users
		foreach ($users as $user) {
			if (!empty($user)) {
				$user=unserialize($user);
				if ($username==$user["username"] && md5($password)==$user["password"]) return true;
			}
		}
		return false;
	}
	
	private function create_user_file($username,$password,$email,$group,$userdb) {
		$fh=fopen($userdb,"a");
		$user=array("username"=>$username,"password"=>md5($password),"email"=>$email,"group"=>$group);
		fseek($fh,0,SEEK_END);
		$users=fwrite($fh,serialize($user)."\r\n");
		fclose($fh);
		return true;
	}
	
	public function logout () {
		$_SESSION['s_username']="";
		$_SESSION['s_password']="";
	}
	
	public function get_userinfo () {
		$online=(int) $_SESSION['online'];
		$return_array=array("online_time"=>time()-$online);
		return $return_array;
	}
	
}

?>