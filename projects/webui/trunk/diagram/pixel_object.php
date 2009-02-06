<?php
include ("diagram.php");

function MyXScale($xx)
{ $vv=round($xx*4/pi());
  if ($vv==0) return(0);
  if ($vv==4) return("pi");
  if ($vv==8) return("2 pi");
  if ($vv%2==0) return(strval($vv/2) ."/2 pi");
  return($vv ."/4 pi");
}

$D=new Diagram();
$D->Img=@ImageCreate(700, 400) or die("Cannot create a new GD image."); 
ImageColorAllocate($D->Img, 255, 255, 255); //background color

$D->SetFrame(80, 40, 540, 340);
$D->SetBorder(0, 2*pi(), -1, 1);
$D->SetText("", "", "some functions");
$D->XGridDelta=pi()/4;
$D->XScale="function MyXScale";
$D->YGridDelta=0.2;
$D->YSubGrids=2;
$D->SetGridColor("#FFFFFF", "#EEEEEE");
$D->Draw("#DDDDDD", "#000000", false);
for ($i=80; $i<=540; $i++)
{ $x = $D->RealX($i);
  $j = $D->ScreenY(sin($x));
  $D->Pixel($i, $j, "#FF0000");
  $j= $D->ScreenY(cos($x));
  $D->Pixel($i, $j, "#0000FF");
}
$D->Bar(560, 100, 680, 120, "#0000FF", "f(x)=cos(x)", "#FFFFFF");
$D->Bar(560, 160, 680, 180, "#FF0000", "f(x)=sin(x)", "#000000");

ImagePng($D->Img, "pixel_object.png");
ImageDestroy($D->Img);
?>
<HTML><HEAD></HEAD>
<BODY bgcolor="#eeeeee">
<table border=1><tr><td><IMG src="pixel_object.png"></td></tr></table>
</BODY>
</HTML>