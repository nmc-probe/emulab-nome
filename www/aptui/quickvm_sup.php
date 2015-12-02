<?php
#
# Copyright (c) 2000-2015 University of Utah and the Flux Group.
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
$APTHOST	= "$WWWHOST";
# No sure why tbauth uses WWWHOST for the login cookies, but it
# causes confusion in geni-login.ajax. 
$COOKDIEDOMAIN  = "$WWWHOST";
$APTBASE	= "$TBBASE/apt";
$APTMAIL        = $TBMAIL_OPS;
$EXTENSIONS     = $TBMAIL_OPS;
$APTTITLE       = "APT";
$FAVICON        = "aptlab.ico";
$APTLOGO        = "aptlogo.png";
$APTSTYLE       = "apt.css";
$ISAPT		= 1;
$ISCLOUD        = 0;
$ISPNET         = 0;
$ISVSERVER      = 0;
$GOOGLEUA       = 'UA-45161989-1';
# See tbauth.php3
$CHANGEPSWD_PAGE= "changepswd.php";
$MAXGUESTINSTANCES = 10;
$WITHPUBLISHING = 0;

include_once("instance_defs.php");

#
# Global flag to disable accounts. We do this on some pages which
# should not display login/account info.
#
$disable_accounts = 0;

#
# Global flag for page embedded. We look directly into page arguments
# for this, rather then using standard argument processing in each page.
# Page embedding is used to contain an apt pages withing Emulab. 
#
$embedded = 0;
if (isset($_REQUEST["embedded"]) && $_REQUEST["embedded"]) {
    $embedded = 1;
}

# Flag to signal that a requires was spit. For errors.
$spatrequired = 0;

#
# So, we could be coming in on the alternate APT address (virtual server)
# which causes cookie problems. I need to look into avoiding this problem
# but for now, just change the global value of the TBAUTHDOMAIN when we do.
# The downside is that users will have to log in twice if they switch back
# and forth.
#
if ($TBMAINSITE && $_SERVER["SERVER_NAME"] == "www.aptlab.net") {
    $ISVSERVER    = 1;
    $TBAUTHDOMAIN = ".aptlab.net";
    $COOKDIEDOMAIN= ".aptlab.net";
    $APTHOST      = "www.aptlab.net";
    $WWWHOST      = "www.aptlab.net";
    $APTBASE      = "https://www.aptlab.net";
    $APTMAIL      = "APT Operations <portal-ops@aptlab.net>";
    $GOOGLEUA     = 'UA-42844769-3';
    $TBMAILTAG    = "aptlab.net";
    $EXTENSIONS   = "portal-extensions@aptlab.net";
    $TBAUTHTIMEOUT= (24 * 3600 * 7);
    # For devel trees
    if (preg_match("/\/([\w\/]+)$/", $WWW, $matches)) {
	$APTBASE .= "/" . $matches[1];
    }
}
elseif (0 || ($TBMAINSITE && $_SERVER["SERVER_NAME"] == "www.cloudlab.us")) {
    $ISVSERVER    = 1;
    $TBAUTHDOMAIN = ".cloudlab.us";
    $COOKDIEDOMAIN= "www.cloudlab.us";
    $APTHOST      = "www.cloudlab.us";
    $WWWHOST      = "www.cloudlab.us";
    $APTBASE      = "https://www.cloudlab.us";
    $APTMAIL      = "CloudLab Operations <portal-ops@cloudlab.us>";
    $APTTITLE     = "CloudLab";
    $FAVICON      = "cloudlab.ico";
    $APTLOGO      = "cloudlogo.png";
    $APTSTYLE     = "cloudlab.css";
    $ISAPT	  = 0;
    $ISCLOUD      = 1;
    $GOOGLEUA     = 'UA-42844769-2';
    $TBMAILTAG    = "cloudlab.us";
    $EXTENSIONS   = "portal-extensions@cloudlab.us";
    $TBAUTHTIMEOUT= (24 * 3600 * 14);
    # For devel trees
    if (preg_match("/\/([\w\/]+)$/", $WWW, $matches)) {
	$APTBASE .= "/" . $matches[1];
    }
}
elseif ($TBMAINSITE && $_SERVER["SERVER_NAME"] == "www.phantomnet.org") {
    $ISVSERVER    = 1;
    $TBAUTHDOMAIN = ".phantomnet.org";
    $COOKDIEDOMAIN= "www.phantomnet.org";
    $APTHOST      = "www.phantomnet.org";
    $WWWHOST      = "www.phantomnet.org";
    $APTBASE      = "https://www.phantomnet.org";
    $APTMAIL      = "PhantomNet Operations <portal-ops@phantomnet.org>";
    $APTTITLE     = "PhantomNet";
    $FAVICON      = "phantomnet.ico";
    $APTLOGO      = "phantomlogo.png";
    $APTSTYLE     = "phantomnet.css";
    $ISAPT	  = 0;
    $ISPNET       = 1;
    #$GOOGLEUA     = 'UA-42844769-2';
    $TBMAILTAG    = "phantomnet.org";
    $EXTENSIONS   = "portal-extensions@phantomnet.org";
    $TBAUTHTIMEOUT= (24 * 3600 * 14);
    # For devel trees
    if (preg_match("/\/([\w\/]+)$/", $WWW, $matches)) {
	$APTBASE .= "/" . $matches[1];
    }
}

# For backend scripts to know how they were invoked.
if (isset($_SERVER['SERVER_NAME'])) { 
    putenv("SERVER_NAME=" . $_SERVER['SERVER_NAME']);
}

#
# Redefine this so APT errors are styled properly. Called by PAGEERROR();.
#
$PAGEERROR_HANDLER = function($msg, $status_code = 0) {
    global $drewheader, $ISCLOUD, $ISPNET, $spatrequired;

    if (! $drewheader) {
	SPITHEADER();
    }
    echo $msg;
    echo "<script type='text/javascript'>\n";
    echo "    window.ISCLOUD = " . ($ISCLOUD ? "1" : "0") . ";\n";
    echo "    window.ISPNET  = " . ($ISPNET  ? "1" : "0") . ";\n";
    echo "</script>\n";
    if (!$spatrequired) {
	echo "<script src='js/lib/jquery-2.0.3.min.js'></script>\n";
	echo "<script src='js/lib/bootstrap.js'></script>\n";
	echo "<script src='js/lib/require.js' data-main='js/null.js'>
                 </script>\n";
    }
    SPITFOOTER();
    die("");
};

$PAGEHEADER_FUNCTION = function($thinheader = 0, $ignore1 = NULL,
				 $ignore2 = NULL, $ignore3 = NULL)
{
    global $TBMAINSITE, $APTTITLE, $FAVICON, $APTLOGO, $APTSTYLE, $ISAPT;
    global $GOOGLEUA, $ISCLOUD, $ISPNET;
    global $login_user, $login_status;
    global $disable_accounts, $page_title, $drewheader, $embedded;
    $title = $APTTITLE;
    if (isset($page_title)) {
	$title .= " - $page_title";
    }
    $height = ($thinheader ? 150 : 250);
    $drewheader = 1;

    #
    # Figure out who is logged in, if anyone.
    #
    if (($login_user = CheckLogin($status)) != null) {
	$login_status = $status;
	$login_uid    = $login_user->uid();
    }

    echo "<html>
      <head>
        <title>$title</title>
        <link rel='shortcut icon' href='$FAVICON'
              type='image/vnd.microsoft.icon'>
        <link rel='stylesheet' href='css/bootstrap.css'>
        <link rel='stylesheet' href='css/quickvm.css'>
        <link rel='stylesheet' href='css/$APTSTYLE'>";
    echo "<script src='js/lib/jquery.min.js'></script>\n";
    echo "<script>APT_CACHE_TOKEN='" . Instance::CacheToken() . "';</script>";
    echo "<script src='js/common.js?nocache=asdfasdf'></script>
        <link rel='stylesheet' href='css/jquery-steps.css'>
        <script src='https://www.emulab.net/emulab_sup.js'></script>
      </head>
    <body style='display: none;'>\n";

    $manual = ($ISAPT ? "http://docs.aptlab.net" : 
	       ($ISCLOUD ? "http://docs.cloudlab.us" : 
		($ISPNET ? "http://wiki.phantomnet.org" : "")));
    echo "<script type='text/javascript'>\n";
    echo "    window.ISCLOUD  = " . ($ISCLOUD ? "1" : "0") . ";\n";
    echo "    window.ISPNET   = " . ($ISPNET  ? "1" : "0") . ";\n";
    echo "    window.MANUAL   = '$manual';\n";
    echo "    window.EMBEDDED = $embedded;\n";
    echo "</script>\n";
    
    if ($TBMAINSITE && !$embedded && file_exists("../google-analytics.php")) {
	readfile("../google-analytics.php");
	echo "<script type='text/javascript'>
                ga('create', '$GOOGLEUA', 'auto');
                ga('send', 'pageview');
              </script>";
    }

    echo "
    <!-- Container for body, needed for sticky footer -->
    <div id='wrap'>\n";

    if ($embedded) {
	goto embed;
    }

    echo "
         <div class='navbar navbar-static-top' style='margin-bottom: 10px'
              role='navigation'>
           <div class='navbar-inner'>
             <div class='brand'>
                 <img src='images/$APTLOGO'/>
             </div>
             <ul class='nav navbar-nav navbar-right apt-right'>";
    if (!$disable_accounts) {
	if ($login_user && ISADMINISTRATOR()) {
	    # Extra top margin to align with the rest of the buttons.
	    echo "<li class='apt-left' style='margin-top:7px'>\n";
	    if (ISADMIN()) {
		$url = CreateURL("toggle", $login_user,
				 "type", "adminon", "value", 0);
		
		echo "<a href='/$url'>
                             <img src='images/redball.gif'
                                  style='height: 10px;'
                                  border='0' alt='Admin On'></a>\n";
	    }
	    else {
		$url = CreateURL("toggle", $login_user,
				 "type", "adminon", "value", 1);

		echo "<a href='/$url'>
                              <img src='images/greenball.gif'
                                   style='height: 10px;'
                                   border='0' alt='Admin Off'></a>\n";
	    }
	    echo "</li>\n";
	}
        # Extra top margin to align with the rest of the buttons.
	echo "<li id='loginstatus' class='apt-left' style='margin-top:7px'>".
	    ($login_user ? "<p class='navbar-text'>".
	     "$login_uid logged in</p>" : "") . "</li>\n";

	if (!NOLOGINS()) {
	    if (!$login_user) {
		echo "<li id='signupitem' class='apt-left'>" .
                        "<form><a class='btn btn-primary navbar-btn'
                                id='signupbutton'
                                href='signup.php'>
                              Sign Up</a></form></li>
                     \n";
		if ($page_title != "Login") {
		    echo "<li id='loginitem' class='apt-left'>" .
			   "<form><a class='btn btn-primary navbar-btn'
                              id='loginbutton'>
                            Login</a></form></li>
                          \n";
		}
	    }
	    else {
		echo "<li class='apt-left'>" .
		         "<form><a class='btn btn-primary navbar-btn'
                              href='logout.php'>
                            Logout</a></form></li>
                      \n";
	    }
	}
    }
    echo "   </ul>
             <ul class='nav navbar-nav navbar-left apt-left'>
                <li class='apt-left'><form><a class='btn btn-quickvm-home navbar-btn'
                       href='landing.php'>Home</a></form></li>\n";
    if ($ISAPT) {
	echo "  <li class='apt-left'><form><a class='btn btn-quickvm-home navbar-btn'
                       href='http://docs.aptlab.net' target='_blank'>Manual</a></form></li>\n";
    }
    elseif ($ISCLOUD) {
	echo "  <li class='apt-left'><form><a class='btn btn-quickvm-home navbar-btn'
                       href='http://docs.cloudlab.us' target='_blank'>Manual</a></form></li>\n";
    }
    elseif ($ISPNET) {
	echo "  <li class='apt-left'><form><a class='btn btn-quickvm-home navbar-btn'
                       href='http://wiki.phantomnet.org' target='_blank'>Wiki</a></form></li>\n";
    }
    if ($login_user && !($login_status & CHECKLOGIN_WEBONLY)) {
	echo "  <li id='quickvm_actions_menu' class='dropdown apt-left'> ".
	         "<a href='#' class='dropdown-toggle' data-toggle='dropdown'>
                    Actions <b class='caret'></b></a>
                  <ul class='dropdown-menu'>
                   <li><a href='myprofiles.php'>My Profiles</a></li>
                   <li><a href='myexperiments.php'>My Experiments</a></li>
                   <li><a href='manage_profile.php'>Create Profile</a></li>
                   <li><a href='instantiate.php'>Start Experiment</a></li>
                   <li class='divider'></li>
                   <li><a href='getcreds.php'>Download Credentials</a></li>
                   <li><a href='ssh-keys.php'>Manage SSH Keys</a></li>
                   <li><a href='signup.php'>Start/Join Project</a></li>\n";
       if (!$login_user->IsNonLocal()) {
           echo "  <li><a href='changepswd.php'>Change Password</a></li>";
       }
       echo "      <li><a href='logout.php'>Logout</a></li>
                   <li class='divider'></li>
	           <li><a href='list-datasets.php?all=1'>List Datasets</a></li>
	           <li><a href='create-dataset.php'>Create Dataset</a></li>";
       echo "  <li class='divider'></li>\n";
       $then = time() - (90 * 3600 * 24);
       echo "  <li><a href='activity.php?user=$login_uid&min=$then'>
                            My History</a></li>\n";
       if (ISADMIN() || ISFOREIGN_ADMIN()) {
           echo "  <li class='divider'></li>\n";
           echo "  <li><a href='dashboard.php'>DashBoard</a></li>";
           $then = time() - (30 * 3600 * 24);
           echo "  <li><a href='activity.php?min=$then'>
                            History Data</a></li>
	           <li><a href='sumstats.php?min=$then'>Summary Stats</a></li>";
           echo "<li><a href='myexperiments.php?all=1'>
                            All Experiments</a></li>
	             <li><a href='myprofiles.php?all=1'>
                            All Profiles</a></li>";
       }
       echo "     </ul>
                </li>\n";
    }
    echo "   </ul>
           </div>
         </div>\n";
    
    # Put the special message, if any, right below the header. Note that the
    # negative margin is to put it flush below the navbar without having to
    # permanently remove the bottom margin on the navbar
    if ($ISAPT) {
        $message = TBGetSiteVar("aptlab/message");
    }
    elseif ($ISCLOUD) {
        $message = TBGetSiteVar("cloudlab/message");
    } 
    elseif ($ISPNET) {
        $message = TBGetSiteVar("phantomnet/message");
    }
    if ($message != "") {
        echo "<div class='alert alert-warning alert-dismissible'
                 role='alert' style='margin-top: -10px'>
                <center>$message</center>
          </div>";
    }

    if ($login_user) {
        list($pcount, $phours) = Instance::CurrentUsage($login_user);
        $weeksusage = Instance::WeeksUsage($login_user);
        $monthsusage = Instance::MonthsUsage($login_user);
        if ($phours || $weeksusage || $monthsusage) {
            echo "<center style='margin-bottom: 5px; margin-top: -8px'>";
            if ($phours) 
                $phours = sprintf("%.2f", $phours);
            echo "<span class='text-info'>
                       Current Usage: $phours Node Hours</span>";
            if ($weeksusage) {
                $weeksusage = sprintf("%.0f", $weeksusage);
                echo ", ";
                echo "<span class='text-warning'>
                       Prev Week: $weeksusage</span>";
            }
            if ($monthsusage) {
                $monthsusage = sprintf("%.0f", $monthsusage);
                echo ", ";
                echo "<span class='text-danger'>
                       Prev Month: $monthsusage</span>";
            }
            echo "<a href='#' class='btn btn-xs' data-toggle='modal' ".
                "data-target='#myusage_modal'> ".
                "<span class='glyphicon glyphicon-question-sign' ".
                "      style='margin-bottom: 4px;'></span> ".
                "</a>\n";
            echo "</center>\n";
        }
        readfile("template/myusage.html");
    }

    if (!NOLOGINS() && !$login_user && $page_title != "Login") {
	SpitLoginModal("quickvm_login_modal");
	SpitWaitModal("waitwait-modal");
    }
embed:
    echo " <!-- Page content -->
           <div class='container-fluid'>\n";
};

function SPITHEADER($thinheader = 0,
		    $ignore1 = NULL, $ignore2 = NULL, $ignore3 = NULL)
{
    global $PAGEHEADER_FUNCTION;

    $PAGEHEADER_FUNCTION($thinheader, $ignore1, $ignore2, $ignore3);
}

$PAGEFOOTER_FUNCTION = function($ignored = NULL) {
    global $ISAPT, $ISCLOUD, $ISPNET, $embedded;
    $groupname = ($ISAPT ? "apt-users" : 
		  ($ISCLOUD ? "cloudlab-users" : 
		   ($ISPNET ? "phantomnet-users" : "")));
    
    echo "</div>
      </div>\n";
    if ($embedded) {
	return;
    }
    SpitNSFModal();
    echo "
      <!--- Footer -->
      <div>
       <div id='footer'>
        <div class='pull-left'>
          <a href='http://www.emulab.net' target='_blank'>
             Powered by
             <img src='images/emulab-whiteout.png' id='elabpower'></a>
        </div>
	<span>Question or comment? Join the
           <a href='https://groups.google.com/forum/#!forum/${groupname}'
              target='_blank'>Help Forum</a></span>
           <div class='pull-right'>\n";
    echo " <a data-toggle='modal' style='margin-right: 10px;'
              href='#nsf_supported_modal'
	      data-target='#nsf_supported_modal'>Supported by NSF</a>\n";
    echo "&copy; 2015
          <a href='http://www.utah.edu' target='_blank'>
             The University of Utah</a>
        </div>
       </div>
      </div>
      <!-- Placed at the end of the document so the pages load faster -->
     </body></html>\n";
};

function SPITFOOTER($ignored = null)
{
    global $PAGEFOOTER_FUNCTION;

    $PAGEFOOTER_FUNCTION($ignored);
}

function SPITUSERERROR($msg)
{
    PAGEERROR($msg, 0);
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
	'code'  => $code,
	'value' => $msg
	);
    echo json_encode($results);
}

function SPITREQUIRE($main, $extras = "")
{
    global $spatrequired;
    
    echo $extras;
    echo "<script src='js/lib/bootstrap.js'></script>\n";
    echo "<script src='js/lib/require.js' data-main='js/$main'></script>\n";
    $spatrequired = 1;
}

function SPITNULLREQUIRE()
{
    SPITREQUIRE("main");
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

#
# Spit out the verify modal. We are not using real password authentication
# like the rest of the Emulab website. Assumed to be inside of a form
# that handles a create button.
#
function SpitVerifyModal($id, $label)
{
    echo "<!-- This is the user verify modal -->
          <div id='$id' class='modal fade'>
            <div class='modal-dialog'>
            <div class='modal-content'>
               <div class='modal-header'>
                <button type='button' class='close' data-dismiss='modal'
                   aria-hidden='true'>&times;</button>
                <h3>Important</h3>
               </div>
               <div class='modal-body'>
                    <p>Check your email for a verification code, and
                       enter it here:</p>
                       <div class='form-group'>
                        <input name='verify' class='form-control'
                               placeholder='Verification code'
                               autofocus type='text' />
                       </div>
                       <div class='form-group'>
                        <button class='btn btn-primary form-control'
                            id='verify_modal_submit'
                            type='submit' name='create'>
                            $label</button>
                       </div>
               </div>
            </div>
            </div>
         </div>\n";
}

#
# Spit out the login modal. 
#
function SpitLoginModal($id)
{
    global $APTTITLE, $ISAPT, $ISCLOUD, $ISPNET;
    $pwlab = ($ISAPT ? "Aptlab.net" : 
	      ($ISCLOUD ? "CloudLab.us" :
	       ($ISPNET ? "PhantomNet.org" : ""))) .
	" or Emulab.net Username";
    $pwlab = "$pwlab";
    $referrer = CleanString($_SERVER['REQUEST_URI']);
?>
    <!-- This is the login modal -->
    <div id='<?php echo $id ?>' class='modal fade' role='dialog'>
        <div class='modal-dialog'>
        <div id='quickvm_login_form_error'
             class='align-center'></div>
        <div class='modal-content'>
           <div class='modal-header'>
            <button type='button' class='close' data-dismiss='modal'
               aria-hidden='true'>&times;</button>
               <h4 class='modal-title'>Log in to <?php echo $APTTITLE ?></h4>
           </div>
           <form id='quickvm_login_form'
                 role='form'
                 method='post' action='login.php'>
           <input type=hidden name=referrer value='<?php echo $referrer ?>'>
           <div class='modal-body form-horizontal'>
             <div class='form-group'>
                <label for='uid' class='col-sm-2 control-label'>Username</label>
                <div class='col-sm-10'>
                    <input name='uid' class='form-control'
                           placeholder='<?php echo $pwlab ?>'
                           autofocus type='text'>
                </div>
             </div>
             <div class='form-group'>
                <label for='password' class='col-sm-2 control-label'>Password
					  </label>
                <div class='col-sm-10'>
                   <input name='password' class='form-control'
                          placeholder='Password'
                          type='password'>
                </div>
             </div>
             <div class='form-group'>
               <div class='col-sm-offset-2 col-sm-10'>
<?php
    if ($ISCLOUD) {
	?>
                 <button class='btn btn-info btn-sm pull-left' disabled
		    type='button'
                    data-toggle="tooltip" data-placement="left"
		    title="You can use your geni credentials to login"
                    id='quickvm_geni_login_button'>Geni User?</button>
        <?php
    }
?>
                 <button class='btn btn-primary btn-sm pull-right'
                         id='quickvm_login_modal_button'
                         type='submit' name='login'>Login</button>
               </div>
             </div>
           </div>
           </form>
        </div>
        </div>
     </div>
<?php
}

#
# Please Wait.
#
function SpitWaitModal($id)
{
    echo "<!-- This is the Please Wait modal -->
          <div id='$id' class='modal fade'>
            <div class='modal-dialog'>
            <div class='modal-content'>
               <div class='modal-header'>
                <center><h3>Please Wait</h3></center>
               </div>
               <div class='modal-body'>
                 <center><img src='images/spinner.gif' /></center>
               </div>
            </div>
            </div>
         </div>\n";
    ?>
	<script>
	function ShowWaitModal(name) { $('#' + name).modal('show'); }
	function HideWaitModal(name) { $('#' + name).modal('hide'); }
	</script>
    <?php
}

#
# Oops modal.
#
function SpitOopsModal($id)
{
    echo "<!-- This is the Oops modal -->
          <div id='${id}_modal' class='modal fade'>
            <div class='modal-dialog'>
            <div class='modal-content'>
               <div class='modal-header'>
                 <button type='button'
                      class='btn btn-default btn-sm pull-right' 
                      data-dismiss='modal' aria-hidden='true'>
                   Close</button>
                 <center><h3>Oops!</h3></center>
               </div>
               <div class='modal-body'>
                 <div id='${id}_text'></div>
               </div>
            </div>
            </div>
         </div>\n";
}

function SpitNSFModal()
{
    global $ISAPT, $ISCLOUD, $ISPNET;
    $nsfnumber = ($ISAPT ? "CNS-1338155" : 
		  ($ISCLOUD ? "CNS-1302688" :
		   ($ISPNET ? "CNS-XXXXXXX" : "")));
    
    echo "<!-- This is the NSF Supported modal -->
          <div id='nsf_supported_modal' class='modal fade'>
            <div class='modal-dialog'>
             <div class='modal-content'>
              <div class='modal-body'>
                This material is based upon work supported by the
                National Science Foundation under Grant
                No. ${nsfnumber}. Any opinions, findings, and
                conclusions or recommendations expressed in this
                material are those of the author(s) and do not
                necessarily reflect the views of the National Science
                Foundation.
                <br><br>
                <center>
                <button type='button'
                     class='btn btn-default btn-sm' 
                     data-dismiss='modal' aria-hidden='true'>
                  Close</button>
                </center>
              </div>
             </div>
            </div>
         </div>\n";
}

#
# Generate an authentication object to pass to the browser that
# is passed to the web server on boss. This is used to grant
# permission to the user to invoke ssh to a local node using their
# emulab generated (no passphrase) key. This is basically a clone
# of what GateOne does, but that code was a mess. 
#
function SSHAuthObject($uid, $nodeid)
{
    global $USERNODE;
	
    $file = "/usr/testbed/etc/sshauth.key";
    
    #
    # We need the secret that is shared with ops.
    #
    $fp = fopen($file, "r");
    if (! $fp) {
	TBERROR("Error opening $file", 0);
	return null;
    }
    $key = fread($fp, 128);
    fclose($fp);
    if (!$key) {
	TBERROR("Could not get key from $file", 0);
	return null;
    }
    $key   = chop($key);
    $stuff = GENHASH();
    $now   = time();


    $authobj = array('uid'       => $uid,
		     'stuff'     => $stuff,
		     'nodeid'    => $nodeid,
		     'timestamp' => $now,
		     'baseurl'   => "https://${USERNODE}",
		     'signature_method' => 'HMAC-SHA1',
		     'api_version' => '1.0',
		     'signature' => hash_hmac('sha1',
					      $uid . $stuff . $nodeid . $now,
					      $key),
    );
    return json_encode($authobj);
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

#
# Redirect request to https
#
function RedirectSecure()
{
    global $APTHOST;

    if (!isset($_SERVER["SSL_PROTOCOL"])) {
	header("Location: https://$APTHOST". $_SERVER['REQUEST_URI']);
	exit();
    }
}

#
# Redirect to the login page()
#
function RedirectLoginPage()
{
    # HTTP_REFERER will not work reliably when redirecting so
    # pass in the URI for this page as an argument
    header("Location: login.php?referrer=".
	   urlencode($_SERVER['REQUEST_URI']));
    exit(0);
}

#
# Check the login and redirect to login page. We use NONLOCAL modifier
# since the classic emulab interface refuses service to nonlocal users.
#
function CheckLoginOrRedirect($modifier = 0)
{
    RedirectSecure();
    
    $check_status = 0;
    $this_user    = CheckLogin($check_status);
    if (! ($check_status & CHECKLOGIN_LOGGEDIN)) {
	RedirectLoginPage();
    }
    CheckLoginConditions($check_status & ~($modifier|CHECKLOGIN_NONLOCAL));
    return $this_user;
}

?>
