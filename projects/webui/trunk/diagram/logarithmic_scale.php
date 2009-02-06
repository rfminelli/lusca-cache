<?php
include ("diagram.php");

$Hosts=array(28174, 80000, 290000, 500000, 727000, 1200000, 2217000, 4852000, 9472000, 16146000, 29670000, 43230000, 72398092, 109574429, 147344723);
$log10=log(10);

function LogScale($vv)
{ if (($vv>3)||($vv<-3)) return("10^".$vv);
  if ($vv>=0) return(round(exp($vv*log(10))));
  else return(1/round(exp(-$vv*log(10))));
}

$D=new Diagram();
$D->Img=@ImageCreate(600, 400) or die("Cannot create a new GD image.");
ImageColorAllocate($D->Img, 255, 255, 255); //background color

$D->SetFrame(80, 40, 570, 360);
$D->SetBorder(1988, 2002, log10($Hosts[0]), log10($Hosts[14]));
$D->SetText("Year", "Hosts", "Internet growth");
$D->XGridDelta=2;
$D->XSubGrids=2;
$D->YGridDelta=1;
$D->YSubGrids=-1;
$D->YScale="function LogScale";
$D->SetGridColor("#FFFFFF", "#EEEEEE");
$D->Draw("#DDDDDD", "#000000", true);

for ($n=1; $n<count($Hosts); $n++)
  $D->Line($D->ScreenX(1987+$n), $D->ScreenY(log10($Hosts[$n-1])), $D->ScreenX(1988+$n), $D->ScreenY(log10($Hosts[$n])), "#0000ff", 2, "internet hosts");

for ($n=1; $n<count($Hosts); $n++)
  $D->Dot($D->ScreenX(1988+$n), $D->ScreenY(log10($Hosts[$n])), 10, 1, "#ff0000", (1988+$n).": ".$Hosts[$n]." hosts");

ImagePng($D->Img, "logarithmic_scale.png");
ImageDestroy($D->Img);
?>
<HTML><HEAD></HEAD>
<BODY bgcolor="#eeeeee">
<table border=1><tr><td><IMG src="logarithmic_scale.png" usemap="#map1" border=0></td></tr></table>
<map name="map1"> 
<?echo $D->ImgMapData?>
</map>
</BODY>
</HTML>