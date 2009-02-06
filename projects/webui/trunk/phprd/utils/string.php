<?php

/**
 * @author 
 * @copyright 2008
 */

class string_manipulation {

	public function get_between($string, $starts, $ends) {
        $st = strpos($string,$starts)+strlen($starts);
        $en = strpos($string,$ends);
        if ($st==0 || $en==0) return false;
        return substr($string,$st,$en-$st);
}

}

?>