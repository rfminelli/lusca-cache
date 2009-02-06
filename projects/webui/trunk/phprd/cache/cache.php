<?php

/**
 * @author dr4g0n
 * @copyright 2008
 */

class SQLCache {
	private $cache_path;

/**
 * Set path for storing cache
 * 
 * @path path where the files will be stored
 * @returns nothing  
 *   
 */
 	public function set_path($path) {
		$this->cache_path=$path;
	}

/**
 * Write the cache to the file (private function)
 * 
 * @filename name of the file where the cache will be written
 * @content content of the cache
 * @ttl how long the cache will be pulled from the file  
 * @returns false if failed (can't create file, for example)  
 *   
 */	
	private function cache_write($filename,$content,$ttl=60) {
		if (file_exists($fn)) {
		$diff = (time() - filemtime($fn));
		if ($diff>$ttl) unlink($fn); else return false;
		}
		if (!$fh=@fopen($this->cache_path.$filename.".sqlcache","w")) return false;
		$export=@serialize($content);
		fwrite($fh,$export,strlen($export));
		fclose($fh);
		}

/**
 * Read the cache from the file (private function)
 * 
 * @filename name of the file to get the cache from
 * @ttl how long the cache will be pulled from the file  
 * @returns false if no file or the file has expired  
 *   
 */	
	private function cache_read($filename,$ttl=60) {
		$fn=$this->cache_path.$filename.".sqlcache";
		if (!file_exists($fn)) return false;
		if ((time() - filemtime($fn))>$ttl) return false;
		if (!$fh=@fopen($fn,"r")) return false;
		$content=fread($fh,1000000);
		$import=@unserialize($content);
		fclose($fh);
		return $import;
		}
/**
 * Write the query result to cache
 * 
 * @query query to cache
 * @result result of the query
 * @ttl how long to keep in the cache (default 60)   
 * @returns true if cached sucessfully  
 *   
 */
	public function cache_query($query,$result,$ttl=60) {
		$md5=md5($query);
		if ($this->cache_write($md5,$result,$ttl)) return true;
		}

/**
 * Read the query result from cache 
 * 
 * @query query to cache
 * @ttl how long to keep in the cache (default 60)   
 * @returns cache content  
 *   
 */		
	public function get_cached_query($query,$ttl=60) {
		$md5=md5($query);
		return $this->cache_read($md5,$ttl);
		}
}

class WEBCache {
	private $cache_path;

/**
 * Set the path to web cache 
 * 
 * @path query to cache
 * @returns nothing  
 *   
 */	
	public function set_path($path) {
		$this->cache_path=$path;
	}

/**
 * Write web content to cache (private function)
 * 
 * @filename name of the file to be written
 * @content content of the cache   
 * @returns cache content  
 *   
 */	
	private function cache_write($filename,$content) {
		if (!$fh=@fopen($this->cache_path.$filename.".webcache","w")) return false;
		$export=@serialize($content);
		fwrite($fh,$export,strlen($export));
		fclose($fh);
		}
/**
 * Read web content from cache (private function)
 * 
 * @filename name of the file to be written
 * @returns cached web content, false if failed
 *   
 */
	private function cache_read($filename) {
		if (!$fh=@fopen($this->cache_path.$filename.".webcache","r")) return false;
		$content=fread($fh,1000000);
		$import=@unserialize($content);
		fclose($fh);
		return $import;
		}

/**
 * Cache web content (url)
 * 
 * @url write url to cache
 * @result the result of the url (web content) 
 * @ttl how long the cache will live (default 60)
 * @returns true if succeed
 *   
 */
	public function cache_url($url,$result,$ttl=60) {
		$md5=md5($query);
		if (self::cache_write($md5,$result)) return true;
		}

/**
 * Get web content from cache
 * 
 * @url read url from cache
 * @ttl how long the cache will live (default 60)
 * @returns cache content or false if there's none
 * 
 */	
	public function get_cached_url($url,$ttl=60) {
		$md5=md5($query);
		return self::cache_read($md5);
		}
}

?>