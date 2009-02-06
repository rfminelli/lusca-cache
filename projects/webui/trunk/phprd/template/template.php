<?php

/**
 * @author 
 * @copyright 2008
 */

class template {

	private $template_path;
	private $page_buffer;
	private $assignedvars;

/**
 * Get file from template (private)
 * 
 * @filename the name of the file
 * @chunk the size of the chunk in which the files will be readed (dfefault 1024)
 * @returns file content or false if file does not exists
 * 
 */	
	private function getfile($filename,$chunk=1024) {
		$buffer='';
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
			$this->page_buffer=str_replace("%&FORWARD_POST&%",$replace,$this->page_buffer);
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
		while ($dir_start=strpos($this->page_buffer,"%&",$dir_offset)) {
			$dir_end=strpos($this->page_buffer,"&%",$dir_start+1);
			$dir=substr($this->page_buffer,$dir_start+2,$dir_end-$dir_start-2);
			self::directive_execute($dir);
			$dir_offset=$dir_end+1;
			
		}
		return $this->page_buffer;
	}

}

?>