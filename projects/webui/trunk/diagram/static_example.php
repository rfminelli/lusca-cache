<?php 
include ("diagram.php"); 
?> 
<HTML><HEAD></HEAD> 
<BODY bgcolor="#eeeeee"> 
<? 
//1st diagram
$D1=new Diagram();
$D1->Img=@ImageCreate(640, 280) or die("Cannot create a new GD image."); 
ImageColorAllocate($D1->Img, 255, 255, 255); //background color
$D1->SetFrame(60, 20, 470, 240);
$D1->SetBorder(
UTC(2001,12,1,8,0,0), UTC(2001,12,1,17,0,0),
UTC(2003,1,1,0,0,0), UTC(2001,12,1,0,0,0));
$D1->XScale=2;
$D1->YScale=2;
$D1->SetText("","", "Our Call Service 2002");
$D1->Font=2;
$D1->Draw("#C0C080", "#004080", false,"Click on a bar to get the phone number");
$Name=array("Peter", "Paul", "Mike");
$Job=array("Project Manager", "Assistant", "Developer");
$Color=array("#FF0000" ,"#00FF00" ,"#0000FF");
$BGColor=array("#000000" ,"#000000" ,"#FFFFFF");
$Phone=array("000-11-23", "123-45-67", "333-66-99");
$D1->BFont=3;
for ($i=0; $i<12; $i++) $D1->Bar( 
$D1->ScreenX(UTC(2001,12,1,8+rand(0,3),30*rand(0,1),0)), 
$D1->ScreenY(UTC(2002,$i+1,1,0,0,0))-8, 
$D1->ScreenX(UTC(2001,12,1,13+rand(0,3),30*rand(0,1),0)), 
$D1->ScreenY(UTC(2002,$i+1,1,0,0,0))+8,
$Color[$i%3], $Name[$i%3], $BGColor[$i%3], $Job[$i%3], "ShowPhoneNum(". $i%3 .")");
$D1->BFont=5;
$D1->Arrow(550,75,515,145,$Color[0],2);
$D1->Arrow(515,175,585,225,"#000000",1);
$D1->Arrow(560,75,600,225,$Color[2],3);
$D1->Box(520, 50, 590, 70, $Color[0], $Name[0], $BGColor[0], 2, "#000000", $Job[0], "ShowPhoneNum(0)");
$D1->Box(480, 150, 550, 170, $Color[1], $Name[1], $BGColor[1], 2, "#000000", $Job[1], "ShowPhoneNum(1)");
$D1->Box(560, 230, 630, 250, $Color[2], $Name[2], $BGColor[2], 2, "#000000", $Job[2], "ShowPhoneNum(2)");
ImagePng($D1->Img, "static_example1.png");
ImageDestroy($D1->Img);
?>
<table border=1><tr><td><IMG src="static_example1.png" usemap="#map1" border=0></td></tr></table>
<map name="map1"> 
<?echo $D1->ImgMapData?>
</map>
<script language="JavaScript">
Name=new Array("Peter", "Paul", "Mike");
Phone=new Array("000-11-23", "123-45-67", "333-66-99");
function ShowPhoneNum(ii){ alert("Dial "+Phone[ii]+" to speak with "+Name[ii]+"."); }
</script>
<?

//2nd diagram
$D2=new Diagram();
$D2->Img=@ImageCreate(640, 280) or die("Cannot create a new GD image."); 
ImageColorAllocate($D2->Img, 255, 255, 255); //background color
$D2->SetFrame(60, 30, 470, 240);
$D2->SetBorder(UTC(2001,12,1,0,0,0), UTC(2001,12,17,0,0,0), 0, 80);
$D2->XScale=2;
$D2->YScale=" $";
$D2->SetText("","", "prices of some selected goods");
$D2->SetGridColor("#DDDDFF", "");
$D2->Draw("#C0C0FF", "#004080", false);
$Color=array("#FF0000","#FF8000","#FFFF00");
$Price=array(3);
for ($i=0; $i<3; $i++) $Price[$i]=array(17);
for ($i=0; $i<3; $i++)
{ $Price[$i][0]=50-20*$i+rand(0,4);
  $D2->Dot($D2->ScreenX(UTC(2001,12,1,0,0,0)), $D2->ScreenY($Price[$i][0]),
    12, $i, $Color[$i], $Price[$i][0]." $");
  for ($j=1; $j<17; $j++)
  { $Price[$i][$j]=$Price[$i][$j-1]+rand(0,10)-4+2*$i-rand(0,2*$i+2);
    $D2->Dot($D2->ScreenX(UTC(2001,12,$j+1,0,0,0)), $D2->ScreenY($Price[$i][$j]),
      12, $i, $Color[$i], $Price[$i][$j]." $");
  }
}
$D2->Bar(490, 50, 570, 70, $Color[0], "apples", "#000000");
$D2->Bar(490, 90, 570, 110, $Color[1], "oranges", "#000000");
$D2->Bar(490, 130, 570, 150, $Color[2], "bananas", "#000000");
ImagePng($D2->Img, "static_example2.png");
ImageDestroy($D2->Img);
?>
<table border=1><tr><td><IMG src="static_example2.png" usemap="#map2" border=0></td></tr></table>
<map name="map2"> 
<?echo $D2->ImgMapData?>
</map>
<?

//3rd diagram
$nInterval=21;
$PriceCount=array(3);
for ($i=0; $i<3; $i++) $PriceCount[$i]=array($nInterval);
for ($i=0; $i<3; $i++)
{ for ($j=0; $j<$nInterval; $j++) $PriceCount[$i][$j]=0;
}
$xmin=$Price[0][0];
$xmax=$Price[0][0];
for ($i=0; $i<3; $i++)
{ for ($j=0; $j<17; $j++)
  { if ($xmin>$Price[$i][$j]) $xmin=$Price[$i][$j];
    if ($xmax<$Price[$i][$j]) $xmax=$Price[$i][$j];
  }
}
$i=$xmax-$xmin;
$xmin-=0.1*$i;
$xmax+=0.1*$i;
$D3=new Diagram();
$D3->Img=@ImageCreate(640, 280) or die("Cannot create a new GD image."); 
ImageColorAllocate($D3->Img, 255, 255, 255); //background color
$D3->SetFrame(60, 30, 470, 240);
$D3->SetBorder($xmin, $xmax, 0, 1);
$D3->XScale=" $";
$D3->GetXGrid();
for ($i=0; $i<3; $i++)
{ for ($j=0; $j<17; $j++)
    $PriceCount[$i][GetInterval($D3->XGrid[0],$D3->XGrid[1],$D3->XGrid[2],$Price[$i][$j])]++;
}
$ymin=0; $ymax=0; 
for ($i=0; $i<$nInterval; $i++)
{ if ($ymax<$PriceCount[0][$i]+$PriceCount[1][$i]+$PriceCount[2][$i])
    $ymax=$PriceCount[0][$i]+$PriceCount[1][$i]+$PriceCount[2][$i];
}
$ymax*=1.1;
$D3->SetBorder($xmin, $xmax, $ymin, $ymax);
$D3->SetText("","", "distribution of prices");
$D3->Draw("#A0C0A0", "#004080", false);
for ($j=0; $j<$nInterval; $j++)
{ $i=$D3->ScreenX($D3->XGrid[0]+$j*$D3->XGrid[1]/2);
  $ymin=$D3->ScreenY(0);
  $ymax=$D3->ScreenY($PriceCount[0][$j]);
  if ($PriceCount[0][$j]>0) 
    $D3->Box($i-10,$ymax,$i+10,$ymin,$Color[0],"","#000000",1,"#0000ff",$PriceCount[0][$j] ." days");
  $ymin=$ymax;
  $ymax=$D3->ScreenY($PriceCount[0][$j]+$PriceCount[1][$j]);
  if ($PriceCount[1][$j]>0) 
    $D3->Box($i-10,$ymax,$i+10,$ymin,$Color[1],"","#000000",1,"#0000ff",$PriceCount[1][$j] ." days");
  $ymin=$ymax;
  $ymax=$D3->ScreenY($PriceCount[0][$j]+$PriceCount[1][$j]+$PriceCount[2][$j]);
  if ($PriceCount[2][$j]>0) 
    $D3->Box($i-10,$ymax,$i+10,$ymin,$Color[2],"","#000000",1,"#0000ff",$PriceCount[2][$j] ." days");
}
$D3->Box(490, 50, 570, 70, $Color[0], "apples", "#000000", 1, "#0000ff");
$D3->Box(490, 90, 570, 110, $Color[1], "oranges", "#000000", 1, "#0000ff");
$D3->Box(490, 130, 570, 150, $Color[2], "bananas", "#000000", 1, "#0000ff");
function GetInterval($mmin,$ddelta,$mmax,$vvalue)
{ $nn=0;
  for ($ii=$mmin; $ii<$mmax; $ii+=$ddelta/2)
  { if ($vvalue<$ii+$ddelta/4) return($nn);
    $nn++;
  }
  return($nn);
}
ImagePng($D3->Img, "static_example3.png");
ImageDestroy($D3->Img);
?>
<table border=1><tr><td><IMG src="static_example3.png" usemap="#map3" border=0></td></tr></table>
<map name="map3"> 
<?echo $D3->ImgMapData?>
</map>
</BODY>
</HTML>