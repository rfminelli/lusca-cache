<?php

/**
 * @author dr4g0n
 * @copyright 2008
 */

require_once "phprd/cache/cache.php";
require_once "phprd/error-log/error.php";

class sql {
	protected $dbhandle=NULL;
	protected $sqlcache=NULL;
	protected $sqlcaching=false;
	protected $sqlcaching_ttl=60;
	public $lasterror;
	public $lastsql;
	protected $sql_queries_counter=0;
	protected $method="mysql";
	public $rows_affected=0;

 /**
 * Construct sql class, open new cache instance
 *  
 * @path - path for cache files, default "cache/" 
 */
	public function __construct ($path="cache/") {
		$this->sqlcache=new SQLCache;
		$this->sqlcache->set_path($path);
		}

/**
 * Turn on/off the sql caching
 * (so some queries can be cached and some don't')
 * Also sets ttl for cache files
 * 
 * @caching turns on/off sql caching
 * @ttl time to live for the cache, default 60
 * @returns nothing  
 *   
 */
	public function sqlcaching($caching=false,$ttl=60) {
		$this->sqlcaching=$caching;
		$this->sqlcaching_ttl=$ttl;
		}

/**
 * Gets sql query counter (how much queries executed)
 *
 * @returns number of queries       
 *
 */
	public function get_counter() {
		return $this->sql_queries_counter;
		}

}

//**************************** Mysql implementation

class mysql extends sql {

/**
 * Destructor - close mysql open handles
 * 
 */

	public function __destruct() {
		if ($this->method=="mysqli" && $this->dbhandle) @mysqli_close($this->$dbhandle);
			else if ($this->dbhandle) @mysql_close($this->$dbhandle);
	}

/**
 * Connect to sql
 * 
 * @hostname server hostname
 * @username username for server access
 * @password password for server access
 * @db database
 * @caching turns on/off caching, can also be set with sqlcaching
 * @method mysql or mysqli method
 * @extras additional connection settings
 * @returns true if succeed, if not, returns false and lasterror var is the error       
 *   
 */
public function connect ($hostname='localhost',$username='root',$password='',$db="test",$caching=false,$method="mysql",$extras="") {
		if ($method=="mysqli") $this->method="mysqli"; else $this->method="mysql";
		if ($this->method=="mysqli")
			$this->dbhandle=@mysqli_connect($hostname,$username,$password); else
			$this->dbhandle=@mysql_connect($hostname,$username,$password,true);
		if (!$this->dbhandle) { $this->lasterror="Cannot connect to DB!"; return false; }
		if ($this->method=="mysqli")
			if (!@mysqli_select_db($this->dbhandle,$db)) { $this->lasterror="Cannot select DB!"; return false; }
		if ($this->method=="mysql")
				if (!@mysql_select_db($db,$this->dbhandle)) { $this->lasterror="Cannot select DB!"; return false; }
		$this->sqlcaching=$caching;
		return true;
	}

/**
 * Connect to sql using array
 * 
 * @connect_array - array of params used to connect
 * @connect_array['hostname'] - hostname 
 * @connect_array['username'] username for server access
 * @connect_array['password'] password for server access
 * @connect_array['db'] database
 * @connect_array['caching'] turns on/off caching, can also be set with sqlcaching
 * @connect_array['method'] mysql or mysqli method
 * @connect_array['extras'] additional connection settings
 * @returns true if succeed, if not, returns false and lasterror var is the error       
 *   
 */
	
	public function connect_array(array $connect_array) {
		array_change_key_case($connect_array,LOCASE);
		return $this->connect($connect_array['hostname'],$connect_array['username'],$connect_array['password'],$connect_array['db'],$connect_array['caching'],$connect_array['method'],$connect_array['extras']);
	}

/**
 * Create database
 * 
 * @database_name - name of the database to create
 * @returns true if succeed, false if not       
 *   
 */
 
	public function create_database ($database_name) {
		return mysql_create_db($database_name);
	}

/**
 * Create table
 * 
 * @table - table name
 * @definitions - table definition array
 * @engine - engine (default myisam)
 * @row_format - format of the db rows (default dynamic)   
 * @returns true if succeed, if not, returns false and lasterror var is the error       
 *   
 */
 
	public function create_table ($table,array $definitions=array(),$engine="myisam",$row_format="dynamic") {
		if (empty($definitions) || empty($table)) return false;
		$sql="CREATE TABLE $table (";
		foreach ($definitions as $var=>$def) $sql.="$var $def,";
		$sql=substr($sql,0,strlen($sql)-1);
		$sql.=") engine=$engine, row_format=$row_format";
		$this->lastsql=$sql;
		if ($this->method=="mysqli") $res=@mysqli_query($this->dbhandle,$sql);
			else $res=@mysql_query($sql,$this->dbhandle);
		$this->sql_queries_counter++;
		return true;
	}

/**
 * Delete database
 * 
 * @database - database name
 * @returns true if succeed       
 *   
 */

	public function delete_database ($tablename) {
		return @mysql_drop_db($tablename);
	}

/**
 * Delete table
 * 
 * @tablename - table name
 * @returns true if succeed, if not, returns false and lasterror var is the error       
 *   
 */

	public function delete_table ($tablename) {
		if (!empty($tablename)) {
			$sql="DROP TABLE $tablename";
			$this->lastsql=$sql;
			if ($this->method=="mysqli") $res=@mysqli_query($sql,$this->dbhandle);
				else $res=@mysql_query($sql,$this->dbhandle);
			$this->sql_queries_counter++;
			return true;
		} else return false;
	}

/**
 * Clear table
 * 
 * @tablename - table name
 * @returns true if succeed, if not, returns false and lasterror var is the error       
 *   
 */

	public function clear_table ($tablename) {
		if (!empty($tablename)) {
			$sql="TRUNCATE TABLE $tablename";
			$this->lastsql=$sql;
			if ($this->method=="mysqli") $res=@mysqli_query($sql,$this->dbhandle);
				else $res=@mysql_query($sql,$this->dbhandle);
			$this->sql_queries_counter++;
			return true;
		} else return false;
	}

/**
 * Get one row
 * 
 * @table - table name
 * @find - array of the rows to find
 * @extra - additions to the query  
 * @returns array of row if succeed, if not returns false       
 *   
 */

	public function get_row($table,$find=array(),$extra='') {
		$sql="SELECT * from $table";
		if (!empty($find)) {
			$sql.=" WHERE ";
			foreach ($find as $var => $value) {
			//no variable? shouldn't happen - return with error
			if (empty($var)) {
				$this->lasterror='find array misformed';
				return false;
				} 
			$sql.="`$var` = '$value' AND";
			}
			$sql=substr($sql,0,strlen($sql)-4);	//cut last AND
		}
		$sql.=" ".$extra;
		$sql.=" LIMIT 1";
		if ($this->sqlcaching && $cache=$this->sqlcache->get_cached_query($sql,$this->sqlcaching_ttl)) return $cache;
		$this->lastsql=$sql;
		if ($this->method=="mysqli") $res=@mysqli_query($sql,$this->dbhandle);
			else $res=@mysql_query($sql,$this->dbhandle);
		if (!$res) { $this->lasterror='Query returned null'; return false; }
		$row=array();
		if ($this->method=="mysqli") $row=@mysqli_fetch_assoc($res);
			else $row=@mysql_fetch_assoc($res);
		$this->sql_queries_counter++;
		if ($this->sqlcaching) $this->sqlcache->cache_query($sql,$rows,$this->sqlcaching_ttl);
		return $row;
	}

/**
 * Get rows
 * 
 * @table - table name
 * @find - array of the rows to find
 * @extra - additions to the query  
 * @returns array of rows if succeed, if not returns false       
 *   
 */

	public function get_rows($table,$find=array(),$extra='') {
		$sql="SELECT * from $table";
		if (!empty($find)) {
			$sql.=" WHERE ";
			foreach ($find as $var => $value) {
			//no variable? shouldn't happen - revert to original query
			if (empty($var)) {
				$this->lasterror='find array misformed';
				return false;
				} 
			$sql.="`$var` = '$value' AND";
			}
			$sql=substr($sql,0,strlen($sql)-4);	//cut last AND
		}
		$sql.=" ".$extra;
		if ($this->sqlcaching && $cache=$this->sqlcache->get_cached_query($sql,$this->sqlcaching_ttl)) return $cache;
		$this->lastsql=$sql;
		if ($this->method=="mysqli") $res=@mysqli_query($sql,$this->dbhandle);
			else $res=@mysql_query($sql,$this->dbhandle);
		if (!$res) { $this->lasterror='Query returned null'; return false; }
		$rows=array();
		while ($row=@mysql_fetch_assoc($res))
			$rows[]=$row;
		$this->sql_queries_counter++;
		if ($this->sqlcaching) $this->sqlcache->cache_query($sql,$rows,$this->sqlcaching_ttl);
		return $rows;
	}

/**
 * Get like row (rows that have similiar match)
 * 
 * @table - table name
 * @like - array of the rows to find
 * @extra - extra query 
 * @returns array of row if succeed, if not returns false       
 *   
 */

	public function get_likerows($table,$like=array(),$extra="") {
		$sql="SELECT * from $table";
		if (empty($like)) return false;
			$sql.=" WHERE ";
			foreach ($like as $var => $value) {
			//no variable? shouldn't happen - revert to original query
			if (empty($var)) {
				$this->lasterror='find array misformed';
				return false;
				}
			$sql.="`$var` LIKE '$value' AND";
			}
			$sql=substr($sql,0,strlen($sql)-4);	//cut last AND
			if (!empty($extra)) $sql.=" ".$extra;
		if ($this->sqlcaching && $cache=$this->sqlcache->get_cached_query($sql,$this->sqlcaching_ttl)) return $cache;
		$this->lastsql=$sql;
		if ($this->method=="mysqli") $res=@mysqli_query($sql,$this->dbhandle);
			else $res=@mysql_query($sql,$this->dbhandle);
		if (!$res) { $this->lasterror='Query returned null'; return false; }
		$rows=array();
		while ($row=@mysql_fetch_assoc($res)) {
			$rows[]=$row;
		}
		$this->sql_queries_counter++;
		if ($this->sqlcaching) $this->sqlcache->cache_query($sql,$rows,$this->sqlcaching_ttl);
		return $rows;
	}

/**
 * Simple sql query call
 * 
 * @query - table name
 * @returns array of row if succeed, if not returns false       
 * 
 */

	public function sql($query) {
		if (empty($query)) return false;
		if ($this->sqlcaching && $cache=$this->sqlcache->get_cached_query($sql,$this->sqlcaching_ttl)) return $cache;
		$this->lastsql=$query;
		if ($this->method=="mysqli") $res=mysqli_query($query,$this->dbhandle);
			else $res=mysql_query($query,$this->dbhandle);
		if (!$res) return array();
		$rows=array();
		while ($row=@mysql_fetch_assoc($res)) {
			$rows[]=$row;
		}
		$this->sql_queries_counter++;
		if ($this->sqlcaching) $this->sqlcache->cache_query($sql,$rows,$this->sqlcaching_ttl);
		$this->rows_affected=@mysql_affected_rows($this->dbhandle);
		return $rows;
	}

/**
 * Delete row from the base
 * 
 * @table - table name
 * @find - what to find, arrays
 * @extra - extra addition to query  
 * @returns array of row if succeed, if not returns false       
 *   
 */

	public function delete_row($table,$find=array(),$extra='') {
		$sql="DELETE FROM $table";
		if (!empty($find)) {
			$sql.=" WHERE ";
			foreach ($find as $var => $value) {
			//no variable? shouldn't happen - revert to original query
			if (empty($var)) {
				$this->lasterror='find array misformed';
				return false;
				}
			$sql.="`$var` = '$value' AND";
			}
			$sql=substr($sql,0,strlen($sql)-4);	//cut last AND
		}
		$sql.=" LIMIT 1";
		$sql.=" ".$extra;
		$this->lastsql=$sql;
		if ($this->method=="mysqli") $res=@mysqli_query($sql,$this->dbhandle);
			else $res=@mysql_query($sql,$this->dbhandle);
		$this->sql_queries_counter++;
		$this->rows_affected=@mysql_affected_rows($this->dbhandle);
		return $row;
	}

/**
 * Delete rows from the base
 * 
 * @table - table name
 * @find - what to find, arrays
 * @extra - extra addition to query  
 * @returns array of rows if succeed, if not returns false       
 *   
 */
	
	public function delete_rows($table,$find=array(),$extra='') {
		$sql="DELETE FROM $table";
		if (!empty($find)) {
			$sql.=" WHERE ";
			foreach ($find as $var => $value) {
			//no variable? shouldn't happen - revert to original query
			if (empty($var)) {
				$this->lasterror='find array misformed';
				return false;
				} 
			$sql.="`$var` = '$value' AND";
			}
			$sql=substr($sql,0,strlen($sql)-4);	//cut last AND
		}
		$sql.=" ".$extra;
		$this->lastsql=$sql;
		if ($this->method=="mysqli") $res=@mysqli_query($sql,$this->dbhandle);
			else $res=@mysql_query($sql,$this->dbhandle);
		$this->sql_queries_counter++;
		$this->rows_affected=@mysql_affected_rows($this->dbhandle);
		return $row;
	}

/**
 * Change rows in the base
 * 
 * @table - table name
 * @set - array of values to set
 * @where - array of rows to match the change  
 * @returns true if succeed, if not returns false       
 *   
 */

	public function change_rows($table,array $set,array $where=array(),$extra='') {
		$sql="UPDATE $table SET ";
		if (!is_array($set) || empty($set)) return false;
		foreach ($set as $var => $value) {
			$sql.="`$var` = '$value', ";
			}
		$sql=substr($sql,0,strlen($sql)-2);	//cut last AND
		if (!empty($where)) {
			$sql.=" WHERE ";
			foreach ($where as $var => $value) {
			//no variable? shouldn't happen - revert to original query
			if (empty($var)) {
				$this->lasterror='find array misformed';
				return false;
				}
			$sql.="`$var` = '$value' AND";
			}
			$sql=substr($sql,0,strlen($sql)-4);	//cut last AND
		}
		$sql.=" ".$extra;
		$this->lastsql=$sql;
		if ($this->method=="mysqli") $res=@mysqli_query($sql,$this->dbhandle);
			else $res=@mysql_query($sql,$this->dbhandle);
		$this->sql_queries_counter++;
		$this->rows_affected=@mysql_affected_rows($this->dbhandle);
		return true;		
		}

/**
 * Insert row in the base
 * 
 * @table - table name
 * @row - array of values to insert
 * @returns true if succeed, if not returns false       
 *   
 */

	public function insert_row($table,$row) {
		if (!is_array($row)) return false;
		$vars=''; $values='';
		foreach ($row as $var=>$value) {
			$vars.="`$var`,";
			if (strstr($value,"()"))
			$values.="$value,"; else			
			$values.="'$value',";
		}
		$vars=substr($vars,0,strlen($vars)-1);
		$values=substr($values,0,strlen($values)-1);
		$sql="insert into `$table` ($vars) values ($values)";
		$sql.=" ".$extra;
		$this->lastsql=$sql;
		if ($this->method=="mysqli") $res=@mysqli_query($this->dbhandle,$sql,$this->dbhandle);
			else $res=@mysql_query($sql,$this->dbhandle);
		$this->lastsql=$sql;
		$this->sql_queries_counter++;
		$this->rows_affected=@mysql_affected_rows($this->dbhandle);
		if (!$res)
			$this->lasterror="insert query failed, {$this->rows_affected} rows affected";
		return $res;
	}

/**
 * Insert packed rows in the base
 * 
 * @table - table name
 * @vars - vars to insert
 * @rows - array of values to insert
 * @returns true if succeed, if not returns false       
 *   
 */

	public function insert_rows($table,array $vars, array $values) {
		if (empty($table) || empty($values) || empty($vars)) return false;
		$insert="insert into `$table`(";
		foreach ($vars as $var) $insert.="`$var`,";
		$insert=substr($insert,0,strlen($insert)-1);
		$insert.=") values ";
		foreach ($values as $values_packed) {
			 $insert.="(";
			 foreach($values_packed as $value) $insert.="\"$value\",";
			 $insert=substr($insert,0,strlen($insert)-1);
			 $insert.="),";
			}
		$insert=substr($insert,0,strlen($insert)-1);
		$insert.=" ".$extra;
		if ($this->method=="mysqli") $res=@mysqli_query($this->dbhandle,$insert,$this->dbhandle);
			else $res=@mysql_query($insert,$this->dbhandle);
		$this->lastsql=$insert;
		$this->sql_queries_counter++;
		$this->rows_affected=@mysql_affected_rows($this->dbhandle);
		if (!$res)
			$this->lasterror="insert query failed, {$this->rows_affected} rows affected";
		return $res;
	}

/**
 * Available tables informations
 * 
 * @returns Tables with their info       
 *   
 */

	public function get_tables() {
		$sql="show table status";
		if ($this->method=="mysqli") $res=@mysqli_query($this->dbhandle,$sql,$this->dbhandle);
			else $res=@mysql_query($sql,$this->dbhandle);
		$this->lastsql=$sql;
		$this->sql_queries_counter++;
		$rows=array();
		while ($row=@mysql_fetch_assoc($res))
			$rows[]=$row;
		$this->sql_queries_counter++;
		if ($this->sqlcaching) $this->sqlcache->cache_query($sql,$rows,$this->sqlcaching_ttl);
		return $rows;
	}

}

//**************************** Postgre implementation

class postgre extends sql {
	
}

//**************************** MSSQL implementation

class mssql extends sql {
	
}

?>