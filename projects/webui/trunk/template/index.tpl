<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<!--
Design by Free CSS Templates
http://www.freecsstemplates.org
Released for free under a Creative Commons Attribution 2.5 License
-->
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="content-type" content="text/html; charset=utf-8" />
<title>squid : Optimising Web Delivery</title>
<meta name="keywords" content="" />
<meta name="description" content="" />
<link href="%TEMPLATE_PATH%css/default0.css" rel="stylesheet" type="text/css" />
<link rel="stylesheet" type="text/css" href="%TEMPLATE_PATH%css/cssverticalmenu.css" />
<div id="dhtmltooltip"></div>

<script type="text/javascript" src="%TEMPLATE_PATH%js/cssverticalmenu.js">
/***********************************************
* CSS Vertical List Menu- by JavaScript Kit (www.javascriptkit.com)
* Menu interface credits: http://www.dynamicdrive.com/style/csslibrary/item/glossy-vertical-menu/ 
* This notice must stay intact for usage
* Visit JavaScript Kit at http://www.javascriptkit.com/ for this script and 100s more
***********************************************/
</script>
<script type="text/javascript" src="%TEMPLATE_PATH%js/cool_dhtml.js">
/***********************************************
* Cool DHTML tooltip script- © Dynamic Drive DHTML code library (www.dynamicdrive.com)
* This notice MUST stay intact for legal use
* Visit Dynamic Drive at http://www.dynamicdrive.com/ for full source code
***********************************************/
</script>

</head>
<body>
<div id="header">
	<div id="logo">
		<h1><a href=""><span>squid-</span>manager</a></h1>
		<h2>Managing the web delivery</h2>
	</div>
	<div id="menu">
		<ul>
			<li class="first"><a href="javascript:window.open('http://www.squid-cache.org/Doc/')" accesskey="1" title="">squid docs</a></li>
			<li	><a href="doc/index.html" accesskey="1" title="">webmanager docs</a></li>
			<li><a href="index.php?menu=retrieve_config" accesskey="2" title="">retrieve config</a></li>
			<li><a href="index.php?menu=save_config" accesskey="3" title="">save config</a></li>
			<li><a href="index.php?menu=Logout" accesskey="4" title="">logout</a></li>
		</ul>
	</div>
</div>
<div id="splash"><img src="%TEMPLATE_PATH%images/img40000.jpg" alt="" style="width: 877px; height: 140px;" /></div>
<div id="content">
	<div id="colOne">


%BODY%

    </div>
	<div id="colTwo">
		<h3>Search</h3>
		<form id="searchform" method="post" action="index.php">
			<div>
				<input name="keywords" type="text" id="textfield1" />
				<input name="submit1" type="submit" id="submit1" value="Search" />
			</div>
		</form>
		<p>&nbsp;</p>

%MENU_CODE%

<p>&nbsp;</p>
		<h3>Introduction</h3>
		<ul>
			<li><a href="http://www.squid-cache.org/Intro/">About Squid</a></li>
			<li><a href="http://www.squid-cache.org/Intro/why.dyn">Why Squid?</a></li>
			<li><a href="http://www.squid-cache.org/Intro/who.dyn">Squid Developers</a></li>
			<li><a href="http://www.squid-cache.org/Intro/helping.dyn">How to Help Out</a></li>
			<li><a href="http://www.squid-cache.org/Download/">Getting Squid</a></li>
			<li><a href="http://www.squid-cache.org/Support/thankyou.dyn">Donate</a></li>
		</ul>
 
	</div>
	<div style="clear: both;">&nbsp;</div>
</div>
<div id="footer">
	<p>Generation time: %generation_time% sec</p>
	<p>Squid manager %sqm_version% running on %server%</p>
	<p>Design by <a href="http://www.freecsstemplates.org/">Free CSS Templates, Template customisation by <a href="http://theducks.org/">Alex Dawson</a> and <a href="http://www.creative.net.au/">Adrian Chadd, all web content licensed under <a href="http://creativecommons.org/licenses/by-sa/2.5/">Creative Commons Attribution Sharealike 2.5 License</a></p>
</div>
</body>
</html>