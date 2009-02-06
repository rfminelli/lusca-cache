<?PHP
//function.php
include ("diagram.php");
$xmin=(isset($_REQUEST['xmin'])) ? $_REQUEST['xmin'] : "";
$xmax=(isset($_REQUEST['xmax'])) ? $_REQUEST['xmax'] : "";
$ymin=(isset($_REQUEST['ymin'])) ? $_REQUEST['ymin'] : "";
$ymax=(isset($_REQUEST['ymax'])) ? $_REQUEST['ymax'] : "";
$fx=(isset($_REQUEST['fx'])) ? $_REQUEST['fx'] : "";
if (($xmin!="")&&($xmax!="")&&($ymin!="")&&($ymax!="")&&($fx!=""))
{ $D=new Diagram();
  $D->Img=@ImageCreate(720, 400) or die("Cannot create a new GD image."); 
  ImageColorAllocate ($D->Img, 255, 255, 255);
  $D->SetFrame(60, 40, 700, 340);
  $D->SetBorder($xmin, $xmax, $ymin, $ymax);
  $D->SetText("", "", "f(x)=".$fx);
  $D->SetGridColor("#FFFFFF", "#EEEEEE");
  $D->Draw("#DDDDDD", "#000000", false);
  $fx=strtolower($fx); 
  $fx=str_replace('exp','e*p',$fx);
  $fx=str_replace('x','($x)',$fx);
  $fx=str_replace('e*p','exp',$fx);
  $fx=str_replace('pi()','pi',$fx);    
  $fx=str_replace('pi','pi()',$fx);  
  $y=0;    
  for ($i=60; $i<=700; $i++)
  { $x = $D->RealX($i);
    eval ("\$y = ".$fx.";");
    if (($ymin<=$y)&&($y<=$ymax)) 
      $D->Pixel($i, $D->ScreenY($y), "#0000FF");
  }
  header("Content-type: image/png");
  ImagePng($D->Img);
  ImageDestroy($D->Img);
}
?>