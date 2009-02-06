<?php
//dynamic_example.php
?>
<HTML>
<HEAD>
<TITLE></TITLE>
</HEAD>
<BODY bgcolor="#eeeeee">
<form name="inputform" method="post" action="dynamic_example.php" onSubmit="return checkForm()">
<TABLE BORDER=0 CELLPADDING=2 CELLSPACING=2 WIDTH=720>
<?
$xmin=(isset($_REQUEST['xmin'])) ? $_REQUEST['xmin'] : "";
$xmax=(isset($_REQUEST['xmax'])) ? $_REQUEST['xmax'] : "";
$ymin=(isset($_REQUEST['ymin'])) ? $_REQUEST['ymin'] : "";
$ymax=(isset($_REQUEST['ymax'])) ? $_REQUEST['ymax'] : "";
$fx=(isset($_REQUEST['fx'])) ? $_REQUEST['fx'] : "";
if (($xmin!="")&&($xmax!="")&&($ymin!="")&&($ymax!="")&&($fx!=""))
{
?>
<TR><TD width=40></TD>
    <TD width=50 align=right>x-min:</TD>
    <TD width=80 align=left><input name="xmin" value="<?echo $xmin?>" size=8></TD>
    <TD width=50 align=right>x-max:</TD>
    <TD width=80 align=left><input name="xmax" value="<?echo $xmax?>" size=8></TD>
    <TD width=50 align=right>y-min:</TD>
    <TD width=80 align=left><input name="ymin" value="<?echo $ymin?>" size=8></TD>
    <TD width=50 align=right>y-max:</TD>
    <TD width=80 align=left><input name="ymax" value="<?echo $ymax?>" size=8></TD>
    <TD></TD>
</TR>
<TR><TD align=right>y=</TD>
    <TD colspan=8 align=left><input name="fx" value="<?echo $fx?>" size=56></TD>
    <TD><input type=submit value="Draw"></TD>
</TR>
<TR><TD colspan=10>
<IMG src="function.php?xmin=<?echo $xmin?>&xmax=<?echo $xmax?>&ymin=<?echo $ymin?>&ymax=<?echo $ymax?>&fx=<?echo urlencode($fx)?>">
</TD></TR>
<?
}
else
{
?>
<TR><TD width=40></TD>
    <TD width=50 align=right>x-min:</TD>
    <TD width=80 align=left><input name="xmin" value="-4" size=8></TD>
    <TD width=50 align=right>x-max:</TD>
    <TD width=80 align=left><input name="xmax" value="4" size=8></TD>
    <TD width=50 align=right>y-min:</TD>
    <TD width=80 align=left><input name="ymin" value="-0.2" size=8></TD>
    <TD width=50 align=right>y-max:</TD>
    <TD width=80 align=left><input name="ymax" value="0.5" size=8></TD>
    <TD></TD>
</TR>
<TR><TD align=right>y=</TD>
    <TD colspan=8 align=left><input name="fx" value="1/sqrt(2*PI)*exp(-x*x/2)+sin(PI*x)/10" size=56></TD>
    <TD><input type=submit value="Draw"></TD>
</TR>
<?
}
?>
</TABLE>
</form>
<SCRIPT Language="JavaScript">
function checkForm()
{ var xmin=parseFloat(document.inputform.xmin.value);
  var xmax=parseFloat(document.inputform.xmax.value);
  var ymin=parseFloat(document.inputform.ymin.value);
  var ymax=parseFloat(document.inputform.ymax.value);
  if (isNaN(xmin)) { alert("x-min is not a number"); return(false); }
  if (isNaN(xmax)) { alert("x-max is not a number"); return(false); }
  if (isNaN(ymin)) { alert("y-min is not a number"); return(false); }
  if (isNaN(ymax)) { alert("y-max is not a number"); return(false); }
  return(true);
}
</SCRIPT>
</BODY>
</HTML>