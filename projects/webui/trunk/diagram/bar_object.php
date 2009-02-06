<?php
include ("diagram.php");

$D=new Diagram();
$D->Img=@ImageCreate(560, 260) or die("Cannot create a new GD image."); 
ImageColorAllocate($D->Img, 255, 255, 255); //background color

$D->SetFrame(80, 40, 520, 240);
$D->SetBorder(-1, 13, 0, 1000);
$D->SetText("","", "Website Hits 2001");
$D->XScale=0;
$D->Draw("#FFFF80", "#004080", false);
$Month=array("Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec");
for ($i=0; $i<12; $i++)
{ $y=500+rand(0,400);
  $j=$D->ScreenX($i+0.5);
  if ($i%2==0) $D->Bar($j-15, $D->ScreenY($y), $j+15, $D->ScreenY(0), "#0000FF", $Month[$i], "#FFFFFF", "Hits per month", "alert(\"".$y." hits\")");
  else $D->Bar($j-15, $D->ScreenY($y), $j+15, $D->ScreenY(0), "#FF0000", $Month[$i], "#000000", "Hits per month", "alert(\"".$y." hits\")");
}

ImagePng($D->Img, "bar_object.png");
ImageDestroy($D->Img);
?>
<HTML><HEAD></HEAD>
<BODY bgcolor="#eeeeee">
<table border=1><tr><td><IMG src="bar_object.png" usemap="#map1" border=0></td></tr></table>
<map name="map1"> 
<?echo $D->ImgMapData?>
</map>
</BODY>
</HTML>