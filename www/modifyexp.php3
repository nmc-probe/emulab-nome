<?php
#
# EMULAB-COPYRIGHT
# Copyright (c) 2000-2003 University of Utah and the Flux Group.
# All rights reserved.
#
include("defs.php3");
include("showstuff.php3");

$parser   = "$TB/libexec/ns2ir/parse-ns";

#
# Standard Testbed Header
#
PAGEHEADER("Modify Experiment");

# the following hack is a page for Netbuild to
# point the users browser at after a successful modify.
# this does no error checking.
if ($justsuccess) {
    echo "<br /><br />";
    echo "<font size=+1>
	  <p>Experiment
          <a href='showexp.php3?pid=$pid&eid=$eid'>$eid</a>
          in project <A href='showproject.php3?pid=$pid'>$pid</A>
          is being modified!</p><br />
	  <p>You will be notified via email when the operation is complete.
          This could take one to ten minutes, depending on
          whether nodes were added to the experiments, and whether
          disk images had to be loaded.</p>
          <p>While you are waiting, you can watch the log 
          in <a target=_blank href=spewlogfile.php3?pid=$pid&eid=$eid>
          realtime</a>.</p></font>\n";
    PAGEFOOTER();
    return;
}

#
# Only known and logged in users can modify experiments.
#
$uid = GETLOGIN();
LOGGEDINORDIE($uid);

$isadmin = ISADMIN($uid);

#
# Verify page arguments.
# 
if (!isset($pid) ||
    strcmp($pid, "") == 0) {
    USERERROR("You must provide a Project ID.", 1);
}
if (!isset($eid) ||
    strcmp($eid, "") == 0) {
    USERERROR("You must provide an Experiment ID.", 1);
}

#
# Be paranoid.
#
$pid = addslashes($pid);
$eid = addslashes($eid);

#
# Check to make sure this is a valid PID/EID tuple.
#
$query_result =
    DBQueryFatal("SELECT * FROM experiments WHERE ".
		 "eid='$eid' and pid='$pid'");
if (mysql_num_rows($query_result) == 0) {
  # Netbuild requires the following line.
  echo "\n\n<!-- NetBuild! Experiment does not exist -->\n\n";	
  USERERROR("The experiment $eid is not a valid experiment ".
            "in project $pid.", 1);
}

$expstate = TBExptState($pid, $eid);

if (! TBExptAccessCheck($uid, $pid, $eid, $TB_EXPT_MODIFY ) ) {
  # Netbuild requires the following line.
  echo "\n\n<!-- NetBuild! No permission to modify -->\n\n";	
  USERERROR("You do not have permission to modify this experiment.", 1);
}

if (strcmp($expstate, $TB_EXPTSTATE_ACTIVE) &&
    strcmp($expstate, $TB_EXPTSTATE_SWAPPED)) {
  # Netbuild requires the following line.
  echo "\n\n<!-- NetBuild! Experiment cannot be modified while in transition. -->\n\n";	
  USERERROR("You cannot modify an experiment in transition.", 1);
}


if (! isset($go)) {
	echo "<h3>Modifying Experiment $pid/$eid:<br>";
	echo "<a href='faq.php3#UTT-Modify'>Modify Experiment Documentation (FAQ)</a></h3>";
	echo "<br>";

	if ($isadmin) {
	    echo "<font size='+1'>".
		 "You can <a href='buildui/bui.php3?action=modify&pid=$pid&eid=$eid'>".
		 "modify this experiment with NetBuild</a>, or edit the NS directly:</font>";
	    echo "<br>";
        }

	echo "<form action='modifyexp.php3' method='post'>";
	echo "<textarea cols='100' rows='40' name='nsdata'>";

	$query_result =
	    DBQueryFatal("SELECT nsfile from nsfiles ".
                         "where pid='$pid' and eid='$eid'");
	if (mysql_num_rows($query_result)) {
	    $row    = mysql_fetch_array($query_result);
	    $nsfile = stripslashes($row[nsfile]);
	    
	    echo "$nsfile";
	} else {
	    echo "# There was no stored NS file for $pid/$eid.\n";
	}
	
	echo "</textarea>";

	echo "<br>";
	if (0 == strcmp($expstate, $TB_EXPTSTATE_ACTIVE)) {
	    echo "<p><b>Note!</b> It is recommended that you 
	      reboot all nodes in your experiment by checking the box below.
	      This is especially important if changing your experiment topology 
              (adding or removing nodes, links, and LANs).
	      If adding/removing a delay to/from an existing link, or replacing 
	      a lost node <i>without modifying the experiment topology</i>,
	      this won't be necessary.</p>";
	    echo "<input type='checkbox' name='reboot' value='1' checked='1'>
	      Reboot nodes in experiment (Highly Recommended)</input>";
	}
	echo "<br>";
	echo "<input type='hidden' name='pid' value='$pid'>";
	echo "<input type='hidden' name='eid' value='$eid'>";
	echo "<input type='submit' name='go' value='Modify'>";
	echo "</form>\n";
    } else {
	if (! isset($nsdata)) {
	    USERERROR("NSdata CGI variable missing (How did that happen?)",1);
	}

	$nsfile = tempnam("/tmp", "$pid-$eid.nsfile.");
	if (! ($fp = fopen($nsfile, "w"))) {
	    TBERROR("Could not create temporary file $nsfile", 1);
	}
	$nsdata_string = $nsdata;
	fwrite($fp, $nsdata_string);
	fclose($fp);
	chmod($nsfile, 0666);	

	if (system("$parser -n -a $nsfile") != 0) {
	    USERERROR("Modified NS file contains syntax errors; aborting.", 1);
	}

	#
	# Get exp group.
	#
	TBExptGroup($pid,$eid,$gid);

	#	
	# We need the unix gid for the project for running the scripts below.
	#
	TBGroupUnixInfo($pid, $gid, $unix_gid, $unix_name);

        # Okay, start.
	echo "<font size=+2>Experiment <b>".
	     "<a href='showproject.php3?pid=$pid'>$pid</a>/".
	     "<a href='showexp.php3?pid=$pid&eid=$eid'>$eid</a></b></font>\n";
	echo "<br><br>\n";

	echo "<center>";
	echo "<h2>Starting experiment modify. Please wait a moment ...
              </h2></center>";

	flush();

 	#	
	# Avoid SIGPROF in child.
	# 
	set_time_limit(0);
	
	$output = array();
	$retval = 0;

	$rebootswitch = "";
	if ($reboot) { $rebootswitch = "-r"; }

	$result = exec("$TBSUEXEC_PATH $uid $unix_gid ".
		       "webswapexp $rebootswitch -s modify $pid $eid $nsfile",
		       $output, $retval);

        # It has been copied out by the program!
	unlink($nsfile);
	
	if ($retval) {
	    echo "<br /><br />\n".
		 "<h2>Modify failure($retval): Output as follows:</h2>\n".
		 "<br />".
		 "<xmp>\n";
	    for ($i = 0; $i < count($output); $i++) {
		echo "$output[$i]\n";
	    }
	    echo "</xmp>\n";
	    # the following line is required for Netbuild interaction.
	    echo "\n\n<!-- Netbuild! Modify failed -->\n\n";

	    PAGEFOOTER();
	    die("");
	} else {      
	    #
	    # Exit status 0 means the experiment is swapping, or will be.
	    #
	    
	    echo "<br>";
	    echo "<font size=+1>";
	    echo "Your experiment is being modified!<br><br>";

	    echo "You will be notified via email when the experiment has ".
		"finished modifying and you are able to proceed. This ".
		"typically takes less than 10 minutes, depending on the ".
		"number of nodes in the experiment. ".
		"If you do not receive email notification within a ".
		"reasonable amount time, please contact $TBMAILADDR. ".
		"<br><br>".
		"While you are waiting, you can watch the log of experiment ".
		"modification in ".
		"<a target=_blank href=spewlogfile.php3?pid=$pid&eid=$eid> ".
		"realtime</a>.\n";
	    echo "</font>";
	    
            # the following line is required for Netbuild.
	    echo "\n\n<!-- Netbuild! success -->\n\n";
	}
    }
#
# Standard Testbed Footer
# 
PAGEFOOTER();
?>


