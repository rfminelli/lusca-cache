<?php
/**
 * Graphs class - used to create diagrams
 *
 * @author dr4g0n
 * @copyright 2008
 */

require "diagram/diagram.php";

class graphs {

/**
 * 
 * Creates diagram (pictures)
 * It takes arrays of data (among other things)
 * and generates the graph on the values
 * 
 * @graph_filename name of the file on disk for picture
 * @graph_type the graph type can be bar, box, line, dot, pie, etc... only bar is used so far
 * @text1 first text to draw
 * @text2 second text to draw
 * @text3 third text to draw
 * @data array of data to be drawn
 * @x_size the width of the picture
 * @y_size the height of the picture
 * @bk_color background color
 * @returns none
 * 
 */

	public function create_diagram($graph_filename,$graph_type="bar",$text1="",$text2="",$text3="",$data=array(),$x_size="560",$y_size="300",$bk_color="#FFFFCC") {
	$D=new Diagram();
	$D->Img=@ImageCreate($x_size, $y_size) or die("Cannot create a new GD image.");
	ImageColorAllocate($D->Img, 255, 255, 255); //background color
	if (empty($data)) {
		$textcolor = imagecolorallocate($D->Img, 0, 0, 255);
		imagestring($D->Img, 10, 150, 50, 'Data not yet populated!', $textcolor);
		ImagePng($D->Img, $graph_filename);
		ImageDestroy($D->Img);
		return;
		}
	$max_row=max($data);
	$count_rows=count($data);
	$D->SetFrame(80, 40, $x_size-40, $y_size-60);
	$D->SetBorder(-1, $count_rows, 0, $max_row+(($max_row/100)*10));
	$D->SetText($text1,$text2,$text3);
	$D->XScale="";
	$D->YScale="";
	$D->Draw("#FFFFCC", "#000000", false);
	$bar_colors=array("template/images/v_blue.gif","template/images/v_red.gif");
	$counter=0;$color_picker=0;$bar_width=(($x_size)/(count($data)+2))/1.5;
	foreach($data as $data_value) {
		if (empty($data_var))
			$data_var=$data_value;
		$D->$graph_type($D->ScreenX($counter)-($bar_width/2),$D->ScreenY($data_value),$D->ScreenX($counter)+($bar_width/2),$D->ScreenY(0),$bar_colors[$color_picker]);
		$counter++; $color_picker++;
		if ($color_picker>count($color_picker))
			$color_picker=0;
		}
	ImagePng($D->Img, $graph_filename);
	ImageDestroy($D->Img);
	}
}

?>