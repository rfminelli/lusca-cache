<?php

/**
 * @author dr4g0n
 * @copyright 2008
 */

class ajax {
	private $vars=array();
	private $events=array();

/**
 * Gets the info from returned ajax variables
 * and assembles it and return to two variables 
 * 
 * @input input data (from the element on the web)
 * @output output data (goes back to the element on the web) 
 * @returns the id if exists, if not returns false
 * 
 */		
	public function get_callback(&$input,&$output) {
		if (!$_REQUEST['ajax_request']) return false;
		$input=$_REQUEST['ajax_input_web_ids'];
		$output=$_REQUEST['ajax_output_web_ids'];
		if ($_REQUEST['ajax_request_id']) return $_REQUEST['ajax_request_id']; else return "";
	}

/**
 * Main register callback function
 * You shouldn't call this directly
 * except if you already have
 * a different callback 
 * 
 * @id callback id
 * @input_web_ids input web ids (the web id where to get the content from)
 * @output_web_ids output web ids (the web id where to put the results)
 * @returns the javascript callback code
 * 
 */	
	public function register_callback($id,$input_web_ids="",$output_web_ids="") {
		return "javascript:process_request(\"$id\",\"$input_web_ids\",\"$output_web_ids\");";
	}

/**
 * Register custom javascript code
 * 
 * @custom - custom js code
 * 
 */	
	public function register_custom($custom) {
		return "javascript:$custom";
	}

/**
 * Register callback on the click
 * 
 * @id callback id
 * @input_web_ids input web ids (the web id where to get the content from)
 * @output_web_ids output web ids (the web id where to put the results)
 * @returns the javascript callback code
 * 
 */	
	public function register_callback_onclick($id,$input_web_ids="",$output_web_ids="") {
		return "onclick=".$this->register_callback($id,$input_web_ids,$output_web_ids);
		}
/**
 * Register callback on the focus
 * 
 * @id callback id
 * @input_web_ids input web ids (the web id where to get the content from)
 * @output_web_ids output web ids (the web id where to put the results)
 * @returns the javascript callback code
 * 
 */	
	public function register_callback_onfocus($id,$input_web_ids="",$output_web_ids="") {
		return "onfocus=".$this->register_callback($id,$input_web_ids,$output_web_ids);
		}

/**
 * Register callback on the key press
 * 
 * @id callback id
 * @input_web_ids input web ids (the web id where to get the content from)
 * @output_web_ids output web ids (the web id where to put the results)
 * @returns the javascript callback code
 * 
 */	
	public function register_callback_onkeypress($id,$input_web_ids="",$output_web_ids="") {
		return "onkeypress=".$this->register_callback($id,$input_web_ids,$output_web_ids);
		}

/**
 * Register callback on the mouseover
 * 
 * @id callback id
 * @input_web_ids input web ids (the web id where to get the content from)
 * @output_web_ids output web ids (the web id where to put the results)
 * @returns the javascript callback code
 * 
 */	
	public function register_callback_onmouseover($id,$input_web_ids="",$output_web_ids="") {
		return "onmouseover=".$this->register_callback($id,$input_web_ids,$output_web_ids);
		}

/**
 * Register callback on the mouse over
 * 
 * @id callback id
 * @input_web_ids input web ids (the web id where to get the content from)
 * @output_web_ids output web ids (the web id where to put the results)
 * @returns the javascript callback code
 * 
 */	
	public function register_callback_onmouseout($id,$input_web_ids="",$output_web_ids="") {
		return "onmouseout=".$this->register_callback($id,$input_web_ids,$output_web_ids);
		}

/**
 * Register callback on the blur
 * 
 * @id callback id
 * @input_web_ids input web ids (the web id where to get the content from)
 * @output_web_ids output web ids (the web id where to put the results)
 * @returns the javascript callback code
 * 
 */	
	public function register_callback_onblur($id,$input_web_ids="",$output_web_ids="") {
		return "onblur=".$this->register_callback($id,$input_web_ids,$output_web_ids);
		}

/**
 * Register callback on the change
 * 
 * @id callback id
 * @input_web_ids input web ids (the web id where to get the content from)
 * @output_web_ids output web ids (the web id where to put the results)
 * @returns the javascript callback code
 * 
 */	
	public function register_callback_onchange($id,$input_web_ids="",$output_web_ids="") {
		return "onchange=".$this->register_callback($id,$input_web_ids,$output_web_ids);
		}

/**
 * Register callback on the submit
 * 
 * @id callback id
 * @input_web_ids input web ids (the web id where to get the content from)
 * @output_web_ids output web ids (the web id where to put the results)
 * @returns the javascript callback code
 * 
 */	
	public function register_callback_onsubmit($id,$input_web_ids="",$output_web_ids="") {
		return "onsubmit=".$this->register_callback($id,$input_web_ids,$output_web_ids);
		}

/**
 * Register callback on the load
 * 
 * @id callback id
 * @input_web_ids input web ids (the web id where to get the content from)
 * @output_web_ids output web ids (the web id where to put the results)
 * @returns the javascript callback code
 * 
 */	
	public function register_callback_onload($id,$input_web_ids="",$output_web_ids="") {
		return "onload=".$this->register_callback($id,$input_web_ids,$output_web_ids);
		}

/**
 * Register callback on the unload
 * 
 * @id callback id
 * @input_web_ids input web ids (the web id where to get the content from)
 * @output_web_ids output web ids (the web id where to put the results)
 * @returns the javascript callback code
 * 
 */	
	public function register_callback_onunload($id,$input_web_ids="",$output_web_ids="") {
		return "onunload=".$this->register_callback($id,$input_web_ids,$output_web_ids);
		}

/**
 * Generate main JS code which have
 * necessary functions (a must)
 * 
 * @callback_http_path path to web script where the calls will be placed (default index.php)
 * @returns JS code
 * 
 */	
	public function generate_js_code($callback_http_path="index.php") {
		$js_code = <<<JSCODE
		<script type="text/javascript">
		var http = create_object();
		var request_id='';

		function trim(str, chars) {
		    return ltrim(rtrim(str, chars), chars);
		}

		function ltrim(str, chars) {
		    chars = chars || "\\s";
		    return str.replace(new RegExp("^[" + chars + "]+", "g"), "");
		}

		function rtrim(str, chars) {
		    chars = chars || "\\s";
    		return str.replace(new RegExp("[" + chars + "]+$", "g"), "");
		}

		function create_object()
		{
  			var http;
		  try
		  {
		    http = new XMLHttpRequest();
		  }
		  catch(e)
		  {
		    // IE6 or older
		    var XmlHttpVersions = new Array("MSXML2.XMLHTTP.6.0",
		                                    "MSXML2.XMLHTTP.5.0",
		                                    "MSXML2.XMLHTTP.4.0",
		                                    "MSXML2.XMLHTTP.3.0",
		                                    "MSXML2.XMLHTTP",
		                                    "Microsoft.XMLHTTP");
		    // try every prog id until one works
		    for (var i=0; i<XmlHttpVersions.length && !http; i++)
		    {
		      try
		      {
		        // try to create XMLHttpRequest object
		        http = new ActiveXObject(XmlHttpVersions[i]);
		      }
		      catch (e) {}
		    }
		  }
		  // return the created object or display an error message
		  if (!http)
		    alert("Error creating the XMLHttpRequest object.");
		  else
		    return http;
		}
 
		function process_request(id,input_web_ids,output_web_ids)
		{
		request_id=output_web_ids;
		  if (http)
		  {
		    try
		    {
		      http.open("POST", "$callback_http_path", true);
		      http.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
		      http.onreadystatechange = handleRequestStateChange;
		      if (input_web_ids) {
				input_field_content=document.getElementById(input_web_ids).value;
				} else input_field_content='';
			  http.send("ajax_request=1&ajax_request_id="+id+"&ajax_input_web_ids="+input_field_content+"&ajax_output_web_ids="+output_web_ids);
		    }
		    catch (e)
		    {
		      alert('Cannot connect to server:' + e.toString());
		    }
		  }
		}
		
		function process_reponse(response)
		{
			if (!request_id) return;
			if (!response) return;
			if (!document.getElementById(request_id)) return;
			//alert(request_id);alert(response);
			response=ltrim(response," ");
			if (document.getElementById(request_id).innerHTML) document.getElementById(request_id).innerHTML=response;
			else document.getElementById(request_id).value = response;
		}
 
		function handleRequestStateChange()  
		{ 
		  if (http.readyState == 1)	//loading 
		  { 
		  } 
		  else if (http.readyState == 2) //loaded
		  { 
		  } 
		  else if (http.readyState == 3) //interactive
		  { 
		  } 
		  else if (http.readyState == 4)	//response  
		  { 
		    if (http.status == 200)  
		    { 
		      try 
		      { 
		        response = http.responseText; 
				process_reponse(response);
		      } 
		      catch(e) 
		      { 
		        alert("Error reading the response: " + e.toString()); 
		      } 
		    }  
		    else 
		    {
		      alert("There was a problem retrieving the data:" +http.statusText); 
		    } 
		  } 
		} 
		</script>
JSCODE;
	return $js_code;
	}
}
?>