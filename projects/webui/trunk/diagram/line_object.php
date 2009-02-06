<?php
include ("diagram.php");

function Fahrenheit($vv){ return(round($vv*18+320)/10 ."° F"); }

$D=new Diagram();
$D->Img=@ImageCreate(600, 300) or die("Cannot create a new GD image."); 
ImageColorAllocate($D->Img, 255, 255, 255); //background color

$D->SetFrame(80, 40, 520, 240);
$D->SetBorder(6, 18, 20, 30);
$D->SetText("","", "temperature measured during the day");
$D->XScale=" h";
$D->YScale="° C";
$D->SetGridColor("#cccccc", "");
$D->Draw("#FFEECC", "#663300", false);
$D->GetYGrid();
$D->BFont=4;
for ($t=$D->YGrid[0]; $t<=$D->YGrid[2]; $t+=$D->YGrid[1])
  $D->Bar($D->right+6, $D->ScreenY($t)-9, "", "", "", Fahrenheit($t), "#663300");
$T1=22;
for ($t=6; $t<18; $t++)
{ $T0=$T1;
  $T1=23-4*cos($t/4)+rand(0,1000)/1000;
  $D->Line($D->ScreenX($t), $D->ScreenY($T0), $D->ScreenX($t+1), $D->ScreenY($T1), "#cc9966", 3, "temperature", "alert(\"".round($T0,1)." -> ".round($T1,1)."\")");
}

ImagePng($D->Img, "line_object.png");
ImageDestroy($D->Img);
?>
<HTML><HEAD></HEAD>
<BODY bgcolor="#eeeeee">
<table border=1><tr><td><IMG src="line_object.png" usemap="#map1" border=0></td></tr></table>
<map name="map1"> 
<?echo $D->ImgMapData?>
</map>
</BODY>
</HTML>