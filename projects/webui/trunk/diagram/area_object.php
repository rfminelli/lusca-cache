<?php
include ("diagram.php");

$D=new Diagram();
$D->Img=@ImageCreate(560, 300) or die("Cannot create a new GD image."); 
ImageColorAllocate($D->Img, 255, 255, 255); //background color

$D->SetFrame(80, 40, 520, 240);
$D->SetBorder(UTC(2000,12,15,0,0,0), UTC(2001,12,15,0,0,0), -300, 1100);
$D->SetText("","", "profit/loss per day during the last year");
$D->XScale=4;
$D->YScale=" $";
$D->Draw("#FFFFCC", "#000000", false);
$base=$D->ScreenY(0);
$t1=$D->ScreenX(UTC(2000,12,15,0,0,0));
$P1=0;
for ($i=0; $i<12; $i++)
{ $t0=$t1; $P0=$P1;
  $t1=$D->ScreenX(UTC(2001,$i+1,15,0,0,0));
  $P1+=$i*20-rand(0,100);
  $D->Area($t0, $D->ScreenY($P0), $t1, $D->ScreenY($P1), "ff0000", $base, "profit/loss per day", "alert(\"".$P0." -> ".$P1."\")");
}

ImagePng($D->Img, "area_object.png");
ImageDestroy($D->Img);
?>
<HTML><HEAD></HEAD>
<BODY bgcolor="#eeeeee">
<table border=1><tr><td><IMG src="area_object.png" usemap="#map1" border=0></td></tr></table>
<map name="map1"> 
<?echo $D->ImgMapData?>
</map>
</BODY>
</HTML>