<?php
#
# EMULAB-COPYRIGHT
# Copyright (c) 2000-2003 University of Utah and the Flux Group.
# All rights reserved.
#
include("defs.php3");
include("showstuff.php3");

#
# Only known and logged in users can do this.
#
$uid = GETLOGIN();
LOGGEDINORDIE($uid, CHECKLOGIN_USERSTATUS|CHECKLOGIN_WEBONLY);
$isadmin = ISADMIN($uid);

#
# Verify form arguments.
# 
if (!isset($target_uid) ||
    strcmp($target_uid, "") == 0) {
    USERERROR("You must provide a User ID.", 1);
}

#
# Standard Testbed Header, now that we know what we want to say.
#
if (strcmp($uid, $target_uid)) {
    PAGEHEADER("Information for User: $target_uid");
}
else {
    PAGEHEADER("My Emulab.Net");
}

#
# Check to make sure thats this is a valid UID.
#
$query_result =
    DBQueryFatal("SELECT * FROM users WHERE uid='$target_uid'");
if (mysql_num_rows($query_result) == 0) {
  USERERROR("The user $target_uid is not a valid user", 1);
}

#
# Verify that this uid is a member of one of the projects that the
# target_uid is in. Must have proper permission in that group too. 
#
if (!$isadmin &&
    strcmp($uid, $target_uid)) {

    if (! TBUserInfoAccessCheck($uid, $target_uid, $TB_USERINFO_READINFO)) {
	USERERROR("You do not have permission to view this user's ".
		  "information!", 1);
    }
}

#
# Tell the user how many PCs he is using.
#
$yourpcs = TBUserPCs($target_uid);

if ($yourpcs) {
    echo "<center><font color=Red size=+2>\n";
    
    if (strcmp($uid, $target_uid))
	echo "$target_uid is using $yourpcs PCs!\n";
    else
	echo "You are using $yourpcs PCs!\n";
    
    echo "</font></center>\n";
}

#
# Lets show Experiments.
#
$query_result =
    DBQueryFatal("select e.*,count(r.node_id) from experiments as e ".
		 "left join reserved as r on e.pid=r.pid and e.eid=r.eid ".
		 "where expt_head_uid='$target_uid' ".
		 "group by e.pid,e.eid order by e.state,e.eid");

if (mysql_num_rows($query_result)) {
    echo "<center>
          <h3>Current Experiments</h3>
          </center>
          <table align=center border=1 cellpadding=2 cellspacing=2>\n";

    echo "<tr>
              <th>PID</th>
              <th>EID</th>
              <th>State</th>
              <th align=center>Nodes</th>
              <th align=center>Hours Idle</th>
              <th>Description</th>
          </tr>\n";

    while ($projrow = mysql_fetch_array($query_result)) {
	$pid  = $projrow[pid];
	$eid  = $projrow[eid];
	$state= $projrow[state];
	$nodes= $projrow["count(r.node_id)"];
	$idlehours = TBGetExptIdleTime($pid,$eid);
	if ($idlehours == -1) { $idlehours = "&nbsp;"; }
	$name = stripslashes($projrow[expt_name]);
	if ($projrow[swap_requests] > 0) {
	  $state .= "&nbsp;(idle)";
	}
	
        echo "<tr>
                 <td><A href='showproject.php3?pid=$pid'>$pid</A></td>
                 <td><A href='showexp.php3?pid=$pid&eid=$eid'>$eid</A></td>
		 <td>$state</td>
                 <td align=center>$nodes</td>
                 <td align=center>$idlehours</td>
                 <td>$name</td>
             </tr>\n";
    }
    echo "</table>\n";
}

#
# Lets show projects.
#
$query_result =
    DBQueryFatal("select distinct g.pid,g.trust,p.name ".
		 " from group_membership as g ".
		 "left join projects as p on p.pid=g.pid ".
		 "where uid='$target_uid' and g.pid=g.gid and ".
		 "trust!='" . TBDB_TRUSTSTRING_NONE . "' ".
		 "order by pid");

if (mysql_num_rows($query_result)) {
    echo "<center>
          <h3>Project Membership</h3>
          </center>
          <table align=center border=1 cellpadding=1 cellspacing=2>\n";

    echo "<tr>
              <th>PID</td>
              <th>Name</td>
              <th>MailTo</td>
          </tr>\n";

    while ($projrow = mysql_fetch_array($query_result)) {
	$pid   = $projrow[pid];
	$name  = $projrow[name];
	$trust = $projrow[trust];
	$mail  = $pid . "-users@" . $OURDOMAIN;

	if (TBTrustConvert($trust) != $TBDB_TRUST_NONE) {
	    echo "<tr>
                     <td><A href='showproject.php3?pid=$pid'>$pid</A></td>
                     <td>$name</td>
                     <td><a href=mailto:$mail>$mail</a></td>
                 </tr>\n";
        }
    }
    echo "</table>\n";
}

#
# Sub group membership too.
# 
SHOWGROUPMEMBERSHIP($target_uid);

#
# Widearea Accounts
# 
SHOWWIDEAREAACCOUNTS($target_uid);

#
# User Profile.
#
echo "<center>
      <h3>Profile</h3>
      </center>\n";

SUBPAGESTART();
SUBMENUSTART("User Options");
#
# Permission check not needed; if the user can view this page, they can
# generally access these subpages, but if not, the subpage will still whine.
# 
WRITESUBMENUBUTTON("Edit Profile",  "moduserinfo.php3?target_uid=$target_uid");

if ($isadmin || !strcmp($uid, $target_uid)) {
    WRITESUBMENUBUTTON("Edit SSH Keys",
		       "showpubkeys.php3?target_uid=$target_uid");
    WRITESUBMENUBUTTON("Edit SFS Keys",
		       "showsfskeys.php3?target_uid=$target_uid");
}

if ($isadmin) {
    if (!strcmp(TBUserStatus($target_uid), TBDB_USERSTATUS_FROZEN)) {
	WRITESUBMENUBUTTON("Thaw User",
		   "freezeuser.php3?target_uid=$target_uid&action=thaw");
    }
    else {
	WRITESUBMENUBUTTON("Freeze User",
		   "freezeuser.php3?target_uid=$target_uid&action=freeze");
    }
    WRITESUBMENUBUTTON("Delete User",
		       "deleteuser.php3?target_uid=$target_uid");
}
SUBMENUEND();

SHOWUSER($target_uid);
SUBPAGEEND();

#
# Standard Testbed Footer
# 
PAGEFOOTER();
?>



