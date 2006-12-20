<?php
#
# EMULAB-COPYRIGHT
# Copyright (c) 2000-2003, 2006 University of Utah and the Flux Group.
# All rights reserved.
#
include("defs.php3");

#
# Standard Testbed Header
#
PAGEHEADER("Remove User");

#
# Only known and logged in users allowed.
#
$this_user = CheckLoginOrDie();
$uid       = $this_user->uid();
$isadmin   = ISADMIN();

#
# Verify arguments.
# 
if (!isset($user) || $user == "") {
    USERERROR("You must provide a User ID.", 1);
}
if (isset($target_pid) &&
    strcmp($target_pid, "") == 0) {
    USERERROR("You must provide a valid project ID.", 1);
}

#
# Confirm target is a real user.
#
if (! ($target_user = User::Lookup($user))) {
    USERERROR("The user $user is not a valid user", 1);
}
$target_dbuid = $target_user->uid();
$target_uid   = $target_user->uid();

#
# Requesting? Fire off email and we are done. 
# 
if (isset($request) && $request) {
    $uid_name  = $this_user->name();
    $uid_email = $this_user->email();

    TBMAIL($TBMAIL_OPS,
	   "Delete User Request: '$target_uid'",
	   "$uid is requesting that user account '$target_uid' be deleted\n".
	   "from the testbed since $target_uid is no longer a member of any ".
	   "projects.\n",
	   "From: $uid_name '$uid' <$uid_email>\n".
	   "Errors-To: $TBMAIL_WWW");

    echo "A request to remove user '$target_uid' has been sent to Testbed
          Operations. If you do not hear back within a reasonable amount
          of time, please contact $TBMAILADDR.\n";

    #
    # Standard Testbed Footer
    # 
    PAGEFOOTER();
    return;
}

#
# Confirm optional pid is a real pid and check permission
#
if (isset($target_pid)) {
    if (! ($target_project = Project::Lookup($target_pid))) {
	USERERROR("No such project '$target_pid'", 1);
    }
    if (! $isadmin &&
	! $target_project->AccessCheck($this_user, $TB_PROJECT_DELUSER)) {
	USERERROR("You do not have permission to remove user ".
		  "$target_uid from project $target_pid!", 1);
    }
}

#
# Must not be the head of the project being removed from, or any projects
# if being completely removed.
#
if (isset($target_pid)) {
    TBProjLeader($target_pid, $leader_uid);
    if (! strcmp($target_uid, $leader_uid)) {
	USERERROR("$target_uid is the leader of project $target_pid!", 1);
    }
}
else {
    $query_result =
	DBQueryFatal("select pid from projects ".
		     "where head_uid='$target_dbuid'");

    if (mysql_num_rows($query_result)) {
	USERERROR("$target_uid is still heading up projects!", 1);
    }
}

#
# Must not be the head of any groups in the project, or any groups if
# being deleted from the testbed.
#
if (isset($target_pid)) {
    $query_result =
	DBQueryFatal("select pid,gid from groups ".
		     "where leader='$target_uid' and pid='$target_pid'");
    
    if (mysql_num_rows($query_result)) {
	USERERROR("$target_uid is still leading groups in ".
		  "project '$target_pid'", 1);
    }
}
else {
    $query_result =
	DBQueryFatal("select pid,gid from groups where leader='$target_uid'");

    if (mysql_num_rows($query_result)) {
	USERERROR("$target_uid is still heading up groups!", 1);
    }
}

#
# User must not be heading up any experiments at all. If deleting from
# just a specific project, must not be heading up experiments in that
# project. 
# 
$query_result =
    DBQueryFatal("SELECT * FROM experiments ".
		 "where expt_head_uid='$target_dbuid' ".
		 (isset($target_pid) ? "and pid='$target_pid'" : ""));

if (mysql_num_rows($query_result)) {
    echo "<center><h3>
          User '$target_uid' is heading up the following experiments ".
	  (isset($target_pid) ? "in project '$target_pid' " : "") .
	  ":</h3></center>\n";

    echo "<table align=center border=1 cellpadding=2 cellspacing=2>\n";

    echo "<tr>
              <th align=center>PID</td>
              <th align=center>EID</td>
              <th align=center>State</td>
              <th align=center>Description</td>
          </tr>\n";

    while ($projrow = mysql_fetch_array($query_result)) {
	$pid  = $projrow[pid];
	$eid  = $projrow[eid];
	$state= $projrow[state];
	$name = stripslashes($projrow[expt_name]);
	if ($projrow[swap_requests] > 0) {
	  $state .= "&nbsp;(idle)";
	}
	
        echo "<tr>
                 <td><A href='showproject.php3?pid=$pid'>$pid</A></td>
                 <td><A href='showexp.php3?pid=$pid&eid=$eid'>$eid</A></td>
		 <td>$state</td>
                 <td>$name</td>
             </tr>\n";
    }
    echo "</table>\n";

    USERERROR("They must be terminated before you can remove the user!", 1);
}

#
# We do a double confirmation, running this script multiple times. 
#
if ($canceled) {
    echo "<center><h2><br>
          User Removal Canceled!
          </h2></center>\n";
    
    PAGEFOOTER();
    return;
}

if (!$confirmed) {
    echo "<center><br>\n";

    if (isset($target_pid)) {
	echo "Are you <b>REALLY</b> sure you want to remove user
              '$target_uid' from project '$target_pid'?\n";
    }
    else {
	echo "Are you <b>REALLY</b> sure you want to delete user 
              '$target_uid' from the testbed?\n";
    }
    
    echo "<form action=deleteuser.php3 method=post>";
    echo "<input type=hidden name=user value=\"$user\">\n";
    if (isset($target_pid)) {
	echo "<input type=hidden name=target_pid value=\"$target_pid\">\n";
    }
    echo "<b><input type=submit name=confirmed value=Confirm></b>\n";
    echo "<b><input type=submit name=canceled value=Cancel></b>\n";
    echo "</form>\n";
    echo "</center>\n";

    PAGEFOOTER();
    return;
}

if (!$confirmed_twice) {
    echo "<center><br>
	  Okay, let's be sure.<br>\n";

    if (isset($target_pid)) {
	echo "Are you <b>REALLY REALLY</b> sure you want to remove user
              '$target_uid' from project '$target_pid'?\n";
    }
    else {
	echo "Are you <b>REALLY REALLY</b> sure you want to delete user 
              '$target_uid' from the testbed?\n";
    }
    
    echo "<form action=deleteuser.php3 method=post>";
    echo "<input type=hidden name=user value=\"$user\">\n";
    if (isset($target_pid)) {
	echo "<input type=hidden name=target_pid value=\"$target_pid\">\n";
    }
    echo "<input type=hidden name=confirmed value=Confirm>\n";
    echo "<b><input type=submit name=confirmed_twice value=Confirm></b>\n";
    echo "<b><input type=submit name=canceled value=Cancel></b>\n";
    echo "</form>\n";
    echo "</center>\n";

    PAGEFOOTER();
    return;
}

STARTBUSY("User '$target_uid' is being removed!");

#
# All the real work is done in the script.
#
SUEXEC($uid, $TBADMINGROUP,
       "webrmuser " . (isset($target_pid) ? "-p $target_pid " : " ") .
       "$target_uid",
       SUEXEC_ACTION_DIE);

STOPBUSY();

#
# If a user was removed from a project, and that user no longer has
# any project membership, ask if they want the user deleted. Admin
# people can act on it immediately of couse, but mere users, even
# project leaders, must send us a request for it.
#
if (isset($target_pid)) {
    $projlist = $target_user->ProjectMembershipList();
    
    if (! count($projlist)) {
	echo "<b>User 'target_uid' is no longer a member of any projects.\n";

	$url = CreateURL("deleteuser", $target_user);
	    
	if ($isadmin) {
	    echo "Do you want to
                  <A href='$url'>delete this user from the testbed?</a>\n";
	}
	else {
	    echo "You can 
                  <A href='${url}&request=1'>request</a>
                     that we delete this user from the testbed</a></b>\n";
	}
    }
    else {
	if (isset($target_pid)) {
	    PAGEREPLACE(CreateURL("showgroup", URLARG_PID, $target_pid,
				  URLARG_GID, $target_pid));
	}
    }
}

#
# Standard Testbed Footer
# 
PAGEFOOTER();
?>
