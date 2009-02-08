<?php

/**
 * @author 
 * @copyright 2008
 */

DEFINE('PAGE_EXPIRATION_NONE',0);
DEFINE('PAGE_EXPIRATION_PAST',1);
DEFINE('PAGE_EXPIRATION_FUTURE',2);
DEFINE('PAGE_EXPIRATION_NOW',3);

class template {

	private $template_path;
	private $page_buffer;
	private $assignedvars;
	private $page_expiration=false;

/**
 * Get file from template (private function)
 * 
 * @filename the name of the file
 * @chunk the size of the chunk in which the files will be readed (dfefault 1024)
 * @returns file content or false if file does not exists
 * 
 */
	private function getfile($filename,$chunk=1024) {
		$buffer='';$cbuf='';
		if (!file_exists($filename)) return false;
		$fhandle=fopen($filename,'r');
		$cbuf.=fread($fhandle,$chunk); $buffer.=$cbuf;
		while (strlen($cbuf)==1024) { $cbuf=fread($fhandle,$chunk); $buffer.=$cbuf; }
		fclose($fhandle);
		return $buffer;
	}

/**
 * Set template path
 * 
 * @path the path to the templates
 * @returns nothing
 * 
 */	
	public function set_template_path($path){
		$this->template_path=$path;
	}

/**
 * Get page from the template
 * 
 * @page the page to get from the template
 * @returns true if suceed, false if not
 * 
 */
	public function get_page($page) {
		$this->assignedvars=array();	//init, delete previous content
		if ($this->page_buffer) return false;	//already loaded buffer, not freed!
		$this->page_buffer=$this->getfile($this->template_path.$page.'.tpl');
		if ($this->page_buffer) return true; else return false;
	}

	public function generate_table(array $column,array $data) {
		
	}
	
	public function generate_progressbar(array $empty,array $filled,$percent,$resize) {
		
	}

/**
 * Generate the multistep page content
 * 
 * @steps array of steps
 * @activated_step step that is currently active
 * @mark mark of the step to show the user where he is  
 * @returns marked content
 * 
 */
	public function generate_steps(array $steps,$activated_step,$mark="*") {
		$res=''; $i=1;
		foreach ($steps as $step) {
			if ($i==$activated_step) $res.=$mark." ";
			$res.=$step.'<br>';
			$i++;
		}
		return $res;
	}

/**
 * Assign the vars on the page
 * 
 * @vars array of variables
 * @returns true if suceed
 * 
 */	
	public function assign_var(array $vars) {
		if (empty($vars)) return false;
		foreach ($vars as $var => $value) {
			$this->assignedvars[$var]=$value;
		}
		return true;
	}

/**
 * Replace tags in the web page
 * with vars/values (private)
 * 
 * @var variable
 * @value value
 * @tag the character used to tag 
 * @returns true if suceed
 * 
 */		
	private function replace_tags($var,$value,$tag='%') {
		if (empty($var)) return false;
		$pbuffer=$this->page_buffer;
		$this->page_buffer=str_replace($tag.$var.$tag,$value,$pbuffer);
		return true;
	}

/**
 * Execute directive on the page
 * 
 * @directive name of the directive
 * @returns true if suceed
 * 
 */		
	private function directive_execute($directive) {
		switch ($directive) {
			case "FORWARD_POST":
			$replace='';
			foreach ($_POST as $var=>$value) $replace.="<input type='hidden' name='$var' value='$value' />";
			$this->page_buffer=str_replace("%^FORWARD_POST^%",$replace,$this->page_buffer);
			case "ALL":
		}
	}
	
	public function generate_form(array $fields,$action,$formname='FORM',$method="POST") {
		$form="<form name='form' action='$action' method='$method'>\n";
		foreach ($fields as $field_var=>$field_val) {
			$form.="<input type='text' name='$field_var' value='$field_val'><br>\n";
		}
		$form.="<input type='submit' /><br>\n";
		$form.="</form><br>\n";
		return $form;
	}

/**
 * Finish the page
 * 
 * @tag character for tagging
 * @returns processed page
 * 
 */	
	public function finalize_page($tag='%') {
		foreach($this->assignedvars as $var => $value) {
			$this->replace_tags($var,$value,$tag);
		}
		$dir_offset=0;
		while ($dir_start=strpos($this->page_buffer,"%^",$dir_offset)) {
			$dir_end=strpos($this->page_buffer,"^%",$dir_start+1);
			$dir=substr($this->page_buffer,$dir_start+2,$dir_end-$dir_start-2);
			self::directive_execute($dir);
			$dir_offset=$dir_end+1;
			
		}
		$this->page_expiration();
		return $this->page_buffer;
	}

/**
 * Quick render the page
 * 
 * @vars vars to be replaced
 * @returns true if rendered ok, false if not
 * 
 */
	public function quickrender(array $vars) {
		$this->assign_var($vars);
		$page=$this->finalize_page();
		print $page;
		if (!empty($page))
			return true; else
			return false;
	}

/**
 * Return all block names and their content
 * 
 * @blocksign block sign for execution
 * @returns array with vars and values in blocks
 * 
 */

	private function get_allblocks($blocksign) {
		$result=array();
		$buffer=$this->page_buffer;
		while ($blockvar_start=strpos($buffer,"%{$blocksign}")) {
 			$blockvar_start_e=strpos($buffer,"%",$blockvar_start+1)+1;
 			if ($blockvar_start_e<1)
 				return false;
			$blockvar=substr($buffer,$blockvar_start+2,$blockvar_start_e-$blockvar_start-3);
			$blockvar_end=strpos($buffer,"%/{$blocksign}".$blockvar,$blockvar_start_e+1)+1;
 			if ($blockvar_start_e<1)
 				return false;
			$block=substr($buffer,$blockvar_start_e,$blockvar_end-($blockvar_start_e+1));
			$result[$blockvar]=$block;
			$buffer=str_replace("%?{$blockvar}%{$block}%/?{$blockvar}%","",$buffer);
			}
		$result=(empty($result))?false:$result;
		return $result;
		}

/**
 * Process blocks
 * 
 * 
 */
 	public function blocks() {
 		global $template_path;
 		$compiled_hash=md5($this->page_buffer);
		$compiled_page=$this->page_buffer;
 		
 		if ($block=$this->get_allblocks("?"))
 			foreach ($block as $var=>$value) {
 				global $$var;
 				$blockvar_value=$$var;
 				if ($blockvar_value)
				$this->page_buffer=str_replace("%?{$var}%{$value}%/?{$var}%",$value,$this->page_buffer);
				else
				$this->page_buffer=str_replace("%?{$var}%{$value}%/?{$var}%","",$this->page_buffer);
				$compiled_page=str_replace("%?{$var}%{$value}%/?{$var}%","<?php if(\${$var}) { ?>{$value}<?php } ?>",$compiled_page);
			}
		while (strstr($this->page_buffer,"%!")) {
			$block_s=strpos($this->page_buffer,"%!");
			$block_e=strpos($this->page_buffer,"%",$block_s+1) or dir("Bad template, no ending sign!");
			$block=substr($this->page_buffer,$block_s+2,$block_e-$block_s-2);
			$param_s=strpos($block,"(");
			$param_e=strpos($block,")");
			$func=substr($block,0,$param_s);
			$param=substr($block,$param_s+1,$param_e-$param_s-1);
			$param_array=explode("\",\"",$param);
			$param_array_cleaned=array();
			foreach ($param_array as $param_array_one) {
				$param_array_one=str_replace("\"","",$param_array_one);
				$param_array_cleaned[]=$param_array_one;
				}
			$result=call_user_func_array($func,$param_array_cleaned);
			$this->page_buffer=str_replace("%!".$block."%",$result,$this->page_buffer);
			$compiled_page=str_replace("%!".$block."%","<?php echo {$func} ({$param}); ?>",$compiled_page);
			}
		file_put_contents($template_path.$compiled_hash.".tpc",$compiled_page);
	}

/**
 * Internal page expiration function
 * 
 * 
 */

	private function page_expiration() {
		if (!$this->page_expiration)
			return false;
		header('Pragma: public');
		header($this->page_expiration);
		header('Last-Modified: '.gmdate('D, d M Y H:i:s') . ' GMT'); 
		header('Cache-Control: no-store, no-cache, must-revalidate');     // HTTP/1.1 
		header('Cache-Control: pre-check=0, post-check=0, max-age=0');    // HTTP/1.1
		header ("Pragma: no-cache");
		}


/**
 * Process blocks
 * 
 * 
 */

	public function set_pageexpiration($expiration) {
		switch ($expiration) {
			case PAGE_EXPIRATION_PAST:
			$this->page_expiration="Expires: Sat, 26 Jul 1997 05:00:00 GMT";
			break;
			case PAGE_EXPIRATION_FUTURE:
			$this->page_expiration="Expires: Sat, 26 Jul 2097 05:00:00 GMT";
			break;
			case PAGE_EXPIRATION_NOW:
			$d=date("D, d M Y H:00:00");
			$this->page_expiration="Expires: {$d} GMT";
			break;
			case PAGE_EXPIRATION_NONE:
			default:
			$this->page_expiration=false;
			}
		}

}	// template class

?>