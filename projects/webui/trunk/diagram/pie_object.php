<?php
include ("diagram.php");

$D=new Diagram();
$D->Img=@ImageCreate(300, 200) or die("Cannot create a new GD image."); 
ImageColorAllocate($D->Img, 255, 255, 255); //background color

$D->Pie(100,100,10,80,0*3.6,10*3.6,"#ff6060","10%","alert(\"Apples 10%\")");
$D->Pie(100,100,0,80,10*3.6,40*3.6,"#ffa000","30%","alert(\"Oranges 30%\")");
$D->Pie(100,100,0,80,40*3.6,100*3.6,"#f6f600","60%","alert(\"Bananas 60%\")");
$D->Bar(200,40,280,60,"#ff6060","Apples","#000000");
$D->Bar(200,80,280,100,"#ffa000","Oranges","#000000");
$D->Bar(200,120,280,140,"#f6f600","Bananas","#000000");

ImagePng($D->Img, "pie_object.png");
ImageDestroy($D->Img);
?>
<HTML><HEAD></HEAD>
<BODY bgcolor="#eeeeee">
<table border=1><tr><td><IMG src="pie_object.png" usemap="#map1" border=0></td></tr></table>
<map name="map1"> 
<?echo $D->ImgMapData?>
</map>
</BODY>
</HTML>