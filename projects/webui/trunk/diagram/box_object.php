<?php
include ("diagram.php");

$D=new Diagram();
$D->Img=@ImageCreate(560, 280) or die("Cannot create a new GD image."); 
ImageColorAllocate($D->Img, 255, 255, 255); //background color

$D->SetFrame(80, 40, 520, 240);
$D->SetBorder(UTC(2000,12,15,0,0,0), UTC(2001,12,15,0,0,0), 0, 1000);
$D->SetText("","", "Website Hits 2001");
$D->XScale=4;
$D->Draw("", "#004080", false);
$y0=$D->ScreenY(0);
for ($i=0; $i<12; $i++)
{ $v=round(500+rand(0,400));
  $y=$D->ScreenY($v);
  $j=$D->ScreenX(UTC(2001,$i+1,1,0,0,0));
  if ($i%2==0) $D->Box($j-12, $y, $j+12, $y0, "v_blue.gif", "", "#FFFFFF", 1, "#000000", "Hits per month", "alert(\"".$v." hits\")");
  else $D->Box($j-12, $y, $j+12, $y0, "v_red.gif", "", "#000000", 1, "#000000", "Hits per month", "alert(\"".$v." hits\")");
}

ImagePng($D->Img, "box_object.png");
ImageDestroy($D->Img);
?>
<HTML><HEAD></HEAD>
<BODY bgcolor="#eeeeee">
<table border=1><tr><td><IMG src="box_object.png" usemap="#map1" border=0></td></tr></table>
<map name="map1"> 
<?echo $D->ImgMapData?>
</map>
</BODY>
</HTML>