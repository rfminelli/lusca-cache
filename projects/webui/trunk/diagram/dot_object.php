<?php
include ("diagram.php");

$D=new Diagram();
$D->Img=@ImageCreate(630, 380) or die("Cannot create a new GD image."); 
ImageColorAllocate($D->Img, 255, 255, 255); //background color

$D->SetFrame(40, 20, 600, 340);
$D->SetBorder(6, 18, 0, 6);
$D->XGridDelta=1;
$D->SetGridColor("#808080","");
$D->Draw("#FF80FF", "#000000", false);
$Color=array("#000000","#FF0000","#0000FF","#000000","#FF0000","#0000FF");
for ($Size=6; $Size<=18; $Size++)
{ $x=$D->ScreenX($Size);
  $D->Dot($x, $D->ScreenY(0), $Size, "smile.gif", "", "Type: smile.gif, Size:".$Size, "alert(\"Type: smile.gif, Size:".$Size."\")");
  for ($Type=1; $Type<7; $Type++)
    $D->Dot($x, $D->ScreenY($Type), $Size, $Type, $Color[$Type-1], "Type:".$Type.", Size:".$Size, "alert(\"Type:".$Type.", Size:".$Size."\")");
}

ImagePng($D->Img, "dot_object.png");
ImageDestroy($D->Img);
?>
<HTML><HEAD></HEAD>
<BODY bgcolor="#eeeeee">
<table border=1><tr><td><IMG src="dot_object.png" usemap="#map1" border=0></td></tr></table>
<map name="map1"> 
<?echo $D->ImgMapData?>
</map>
</BODY>
</HTML>