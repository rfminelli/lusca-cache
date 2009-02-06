<?php
include ("diagram.php");

$D=new Diagram();
$D->Img=@ImageCreate(320, 320) or die("Cannot create a new GD image."); 
ImageColorAllocate($D->Img, 255, 255, 255); //background color

$D->SetFrame(40, 40, 280, 280);
$D->SetBorder(-1, 1, -1, 1);
for ($i=0; $i<11; $i++)
{ $x=sin(($i+0.5)*2*pi()/11);
  $y=cos(($i+0.5)*2*pi()/11);
  $D->Arrow($D->ScreenX($x/3), $D->ScreenY($y/3), $D->ScreenX($x), $D->ScreenY($y), "#0000ff", $i%5+1, "Size: ".strval($i%5+1), "alert(\"Size: ".strval($i%5+1)."\")");
}

ImagePng($D->Img, "arrow_object.png");
ImageDestroy($D->Img);
?>
<HTML><HEAD></HEAD>
<BODY bgcolor="#eeeeee">
<table border=1><tr><td><IMG src="arrow_object.png" usemap="#map1" border=0></td></tr></table>
<map name="map1"> 
<?echo $D->ImgMapData?>
</map>
</BODY>
</HTML>