<?php
include ("diagram.php");

$D=new Diagram();
$D->Img=@ImageCreate(480, 320) or die("Cannot create a new GD image.");
ImageColorAllocate($D->Img, 255, 255, 255); //background color

$D->SetFrame(80, 40, 450, 280);
$D->SetBorder(10, 50, 0, 4);
$D->SetText("X-Label","Y-Label","Title");
$D->SetGridColor("#44CC44","");
$D->Draw("#80FF80","#0000FF",true,"Click on me !","DiagramClick()");

ImagePng($D->Img, "diagram_object.png");
ImageDestroy($D->Img);
?>
<HTML><HEAD></HEAD>
<BODY bgcolor="#eeeeee">
<table border=1><tr><td><IMG src="diagram_object.png" usemap="#map1" border=0></td></tr></table>
<map name="map1"> 
<?echo $D->ImgMapData?>
</map>
<script language="JavaScript">
function DiagramClick(){ alert("Use your own function here."); }
</script>
</BODY>
</HTML>