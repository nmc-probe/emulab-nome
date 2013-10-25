<?php
#
# Copyright (c) 2000-2013 University of Utah and the Flux Group.
# 
# {{{EMULAB-LICENSE
# 
# This file is part of the Emulab network testbed software.
# 
# This file is free software: you can redistribute it and/or modify it
# under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or (at
# your option) any later version.
# 
# This file is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
# License for more details.
# 
# You should have received a copy of the GNU Affero General Public License
# along with this file.  If not, see <http://www.gnu.org/licenses/>.
# 
# }}}
#
function SPITHEADER($thinheader = 0)
{
    $height = ($thinheader ? 150 : 250);
    
    echo "<html>
      <head>
        <title>AptLab</title>
        <link rel='stylesheet' href='bootstrap/css/bootstrap.css'>
        <link rel='stylesheet' href='quickvm.css'>
        <script src='quickvm_sup.js'></script>
        <script src='date.format.js'></script>
        <script src='/emulab_sup.js'></script>
        <script src='https://code.jquery.com/jquery.js'></script>
        <script src='bootstrap/js/bootstrap.js'></script>
      </head>
    <body>
    <!-- Container for body, needed for sticky footer -->
    <div id='wrap'>
      <div style='background-color: #ff6600'>
         <img class='align-center' style='width: ${height}px'
               src='aptlogo.png'/>
      </div>
     <!-- Page content -->
     <div class='container'>\n";

}

function SPITFOOTER()
{
    echo "</div>
      </div>
      <!--- Footer -->
      <div>
       <div id='footer'>
        <div class='pull-left'>Powered by
             <img src='emulab-whiteout.png' id='elabpower'></div>
        <div class='pull-right'>&copy; 2013 The University of Utah</div>
       </div>
      </div>
      <!-- Placed at the end of the document so the pages load faster -->
     </body></html>\n";
}

#
# Does not return; page exits.
#
function SPITAJAX_RESPONSE($value)
{
    $results = array(
	'code'  => 0,
	'value' => $value
	);
    echo json_encode($results);
}

function SPITAJAX_ERROR($code, $msg)
{
    $results = array(
	'code'  => code,
	'value' => $msg
	);
    echo json_encode($results);
}

#
# Spit out an info tooltip.
#
function SpitToolTip($info)
{
    echo "<a href='#' class='btn btn-xs' data-toggle='popover' ".
	"data-content='$info'> ".
        "<span class='glyphicon glyphicon-question-sign'></span> ".
        "</a>\n";
}

function GateOneAuthObject($uid)
{
    #
    # We need the secret that is shared with ops.
    #
    $fp = fopen("/usr/testbed/etc/gateone.key", "r");
    if (! $fp) {
	TBERROR("Error opening /usr/testbed/etc/gateone.key", 0);
	return null;
    }
    list($api_key,$secret) = preg_split('/:/', fread($fp, 128));
    fclose($fp);
    if (!($secret && $api_key)) {
	TBERROR("Could not get kets from gateone.key", 0);
	return null;
    }
    $secret = chop($secret);

    $authobj = array(
	'api_key' => $api_key,
	'upn' => $uid,
	'timestamp' => time() . '000',
	'signature_method' => 'HMAC-SHA1',
	'api_version' => '1.0'
    );
    $authobj['signature'] = hash_hmac('sha1',
				      $authobj['api_key'] . $authobj['upn'] .
				      $authobj['timestamp'], $secret);
    $valid_json_auth_object = json_encode($authobj);

    return $valid_json_auth_object;
}

#
# This is a little odd; since we are using our local CM to create
# the experiment, we can just ask for the graphic directly.
#
function GetTopoMap($uid, $pid, $eid)
{
    global $TBSUEXEC_PATH;
    $xmlstuff = "";
    
    if ($fp = popen("$TBSUEXEC_PATH nobody nobody webvistopology ".
		    "-x -s $uid $pid $eid", "r")) {

	while (!feof($fp) && connection_status() == 0) {
	    $string = fgets($fp);
	    if ($string) {
		$xmlstuff .= $string;
	    }
	}
	return $xmlstuff;
    }
    else {
	return "";
    }
}

?>
