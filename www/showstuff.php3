<?php
#
# EMULAB-COPYRIGHT
# Copyright (c) 2000-2007 University of Utah and the Flux Group.
# All rights reserved.
#
#
# This is an included file. No headers or footers.
#
# Functions to dump out various things.  
#
include_once("osinfo_defs.php");
include_once("node_defs.php");

function SHOWNODES($pid, $eid, $sortby, $showclass) {
    global $SCRIPT_NAME;
    global $TBOPSPID;
    
    #
    # If this is an expt in emulab-ops, we also want to see the reserved
    # time. Note that vname might not be useful, but show it anyway.
    #
    # XXX The above is not always true. There are also real experiments in
    # emulab-ops.
    #
    if (!isset($sortby)) {
	$sortclause = "n.type,n.priority";
    }
    elseif ($sortby == "vname") {
	$sortclause = "r.vname";
    }
    elseif ($sortby == "rsrvtime-up") {
	$sortclause = "rsrvtime asc";
    }
    elseif ($sortby == "rsrvtime-down") {
	$sortclause = "rsrvtime desc";
    }
    elseif ($sortby == "nodeid") {
	$sortclause = "n.node_id";
    }
    else {
	$sortclause = "n.type,n.priority";
    }

    # XXX
    if ($pid == "emulab-ops" && $eid == "hwdown") {
	$showlastlog = 1;
	if (empty($showclass)) {
	    $showclass = "no-pcplabphys";
	}
    }
    else {
	$showlastlog = 0;
    }	

    $classclause = "";
    $noclassclause = "";
    
    if (!empty($showclass)) {
	$opts = explode(",", $showclass);
	foreach ($opts as $opt) {
	    if (preg_match("/^no-([-\w]+)$/", $opt, $matches)) {
		if (!empty($noclassclause)) {
		    $noclassclause .= ",";
		}
		$noclassclause .= "'$matches[1]'";
	    } elseif ($opt == "all") {
		$classclause = "";
		$noclassclause = "";
	    } else {
		if (!empty($classclause)) {
		    $classclause .= ",";
		}
		$classclause .= "'$opt'";
	    }
	}
	if (!empty($classclause)) {
	    $classclause = "and nt.class in (" . $classclause . ")";
	}
	if (!empty($noclassclause)) {
	    $noclassclause = "and nt.class not in (" . $noclassclause . ")";
	}
    }

    #
    # Discover whether to show or hide certain columns
    #
    $colcheck_query_result = 
      DBQueryFatal("SELECT sum(oi.OS = 'Windows') as winoscount, ".
                   "       sum(nt.isplabdslice) as plabcount ".
                   "from reserved as r ".
                   "left join nodes as n on n.node_id=r.node_id ".
                   "left join os_info as oi on n.def_boot_osid=oi.osid ".
                   "left join node_types as nt on n.type = nt.type ".
                   "WHERE r.eid='$eid' and r.pid='$pid'");
    $colcheckrow = mysql_fetch_array($colcheck_query_result);
    $anywindows = $colcheckrow["winoscount"];
    $anyplab    = $colcheckrow["plabcount"];

    if ($showlastlog) {
	#
	# We need to extract, for each node, just the latest nodelog message.
	# I could not figure out how to do this in a single select so instead
	# create a temporary table of node_id and latest log message date
	# for all reserved nodes to re-join with nodelog to extract the latest
	# log message.
	#
	DBQueryFatal("CREATE TEMPORARY TABLE nodelogtemp ".
		     "SELECT r.node_id, MAX(reported) AS reported ".
		     "FROM reserved AS r ".
		     "LEFT JOIN nodelog AS l ON r.node_id=l.node_id ".
		     "WHERE r.eid='$eid' and r.pid='$pid' ".
		     "GROUP BY r.node_id");
	#
	# Now join this table and nodelog with the standard set of tables
	# to get all the info we need.  Note the inner join with the temp
	# table, this is faster and still safe since it has an entry for
	# every reserved node.
	#
	$query_result =
	    DBQueryFatal("SELECT r.*,n.*,nt.isvirtnode,nt.isplabdslice, ".
                         " oi.OS,tip.tipname,wa.site,wa.hostname, ".
		         " ns.status as nodestatus, ".
		         " date_format(rsrv_time,\"%Y-%m-%d&nbsp;%T\") as rsrvtime, ".
		         "nl.reported,nl.entry ".
		         "from reserved as r ".
		         "left join nodes as n on n.node_id=r.node_id ".
                         "left join widearea_nodeinfo as wa on wa.node_id=n.phys_nodeid ".
		         "left join node_types as nt on nt.type=n.type ".
		         "left join node_status as ns on ns.node_id=r.node_id ".
		         "left join os_info as oi on n.def_boot_osid=oi.osid ".
			 "left join tiplines as tip on tip.node_id=r.node_id ".
		         "inner join nodelogtemp as t on t.node_id=r.node_id ".
		         "left join nodelog as nl on nl.node_id=r.node_id and nl.reported=t.reported ".

		         "WHERE r.eid='$eid' and r.pid='$pid' ".
			 "$classclause $noclassclause".
		         "ORDER BY $sortclause");
	DBQueryFatal("DROP table nodelogtemp");
    }
    else {
	$query_result =
	    DBQueryFatal("SELECT r.*,n.*,nt.isvirtnode,nt.isplabdslice, ".
                         " oi.OS,tip.tipname,wa.site,wa.hostname, ".
		         " ns.status as nodestatus, ".
		         " date_format(rsrv_time,\"%Y-%m-%d&nbsp;%T\") as rsrvtime ".
		         "from reserved as r ".
		         "left join nodes as n on n.node_id=r.node_id ".
                         "left join widearea_nodeinfo as wa on wa.node_id=n.phys_nodeid ".
		         "left join node_types as nt on nt.type=n.type ".
		         "left join node_status as ns on ns.node_id=r.node_id ".
		         "left join os_info as oi on n.def_boot_osid=oi.osid ".
			 "left join tiplines as tip on tip.node_id=r.node_id ".
		         "WHERE r.eid='$eid' and r.pid='$pid' ".
			 "$classclause $noclassclause".
		         "ORDER BY $sortclause");
    }
    
    if (mysql_num_rows($query_result)) {
	echo "
              <script type='text/javascript' language='javascript' src='sorttable.js'></script>
              <center>
              <br>
              <a href=" . $_SERVER["REQUEST_URI"] . "#reserved_nodes>
                <font size=+1><b>Reserved Nodes</b></font></a>
              </center>
              <a NAME=reserved_nodes>
              <table class='sortable' id='nodetable' align=center border=1>
              <tr>
                <th>Node ID</th>
                <th>Name</th>\n";

        # Only show 'Site' column if there are plab nodes.
        if ($anyplab) {
            echo "  <th>Site</th>
                    <th>Widearea<br>Hostname</th>\n";
        }

	if ($pid == $TBOPSPID) {
	    echo "<th>Reserved<br>
                      <a href=\"$SCRIPT_NAME?pid=$pid&eid=$eid".
		         "&sortby=rsrvtime-up&showclass=$showclass\">Up</a> or 
                      <a href=\"$SCRIPT_NAME?pid=$pid&eid=$eid".
		         "&sortby=rsrvtime-down&showclass=$showclass\">Down</a>
                  </th>\n";
	}
	echo "  <th>Type</th>
                <th>Default OSID</th>
                <th>Node<br>Status</th>
                <th>Hours<br>Idle[<b>1</b>]</th>
                <th>Startup<br>Status[<b>2</b>]</th>\n";
	if ($showlastlog) {
	    echo "  <th>Last Log<br>Time</th>
		    <th>Last Log Message</th>\n";
	}

        echo "  <th><a href=\"docwrapper.php3?docname=ssh-mime.html\">SSH</a></th>
                <th><a href=\"kb-show.php3?xref_tag=tiptunnel\">Console</a></th> .
                <th>Log</th>";

	# Only put out a RDP column header if there are any Windows nodes.
	if ($anywindows) {
            echo "  <th>
                        <a href=\"docwrapper.php3?docname=rdp-mime.html\">RDP</a>
                    </th>\n";
	}
	echo "  </tr>\n";

	$stalemark = "<b>?</b>";
	$count = 0;

	while ($row = mysql_fetch_array($query_result)) {
	    $node_id = $row["node_id"];
	    $vname   = $row["vname"];
	    $rsrvtime= $row["rsrvtime"];
	    $type    = $row["type"];
            $wasite  = $row["site"];
            $wahost  = $row["hostname"];
	    $def_boot_osid = $row["def_boot_osid"];
	    $startstatus   = $row["startstatus"];
	    $status        = $row["nodestatus"];
	    $bootstate     = $row["eventstate"];
	    $isvirtnode    = $row["isvirtnode"];
            $isplabdslice  = $row["isplabdslice"];
	    $tipname       = $row["tipname"];
	    $iswindowsnode = $row["OS"]=='Windows';

	    if (! ($node = Node::Lookup($node_id))) {
		TBERROR("SHOWNODES: Could not map $node_id to its object", 1);
	    }
	    $idlehours = $node->IdleTime();
	    $stale     = $node->IdleStale();

	    $idlestr = $idlehours;
	    if ($idlehours > 0) {
		if ($stale) {
		    $idlestr .= $stalemark;
		}
	    }
	    elseif ($idlehours == -1) {
		$idlestr = "&nbsp;";
	    }

	    if (!$vname)
		$vname = "--";

	    if ($count & 1) {
		echo "<tr></tr>\n";
	    }
	    $count++;

	    echo "<tr>
                    <td><a href='shownode.php3?node_id=$node_id'>$node_id</a></td>
                    <td>$vname</td>\n";

            if ($isplabdslice) {
              echo "  <td>$wasite</td>
                      <td>$wahost</td>\n";
            }
            elseif ($anyplab) {
              echo "  <td>&nbsp;</td>
                      <td>&nbsp;</td>\n";
            }

	    if ($pid == $TBOPSPID)
		echo "<td>$rsrvtime</td>\n";
            echo "  <td>$type</td>\n";
	    if ($def_boot_osid) {
		echo "<td>";
		SPITOSINFOLINK($def_boot_osid);
		echo "</td>\n";
	    }
	    else
		echo "<td>&nbsp;</td>\n";

	    if ($bootstate != "ISUP") {
		echo "  <td>$status ($bootstate)</td>\n";
	    } else {
		echo "  <td>$status</td>\n";
	    }
	    
	    echo "  <td>$idlestr</td>
                    <td align=center>$startstatus</td>\n";

	    if ($showlastlog) {
		echo "  <td>$row[reported]</td>\n";
		echo "  <td>$row[entry] 
                            (<a href='shownodelog.php3?node_id=$node_id'>LOG</a>)
                        </td>\n";
	    }

	    echo "  <td align=center>
                        <a href='nodessh.php3?node_id=$node_id'>
                        <img src=\"ssh.gif\" alt=s></a>
                    </td>\n";

	    if ($isvirtnode || !isset($tipname) || $tipname = '') {
		echo "<td>&nbsp;</td>\n";
	    }
	    else {
		echo "  <td align=center>
                            <a href='nodetipacl.php3?node_id=$node_id'>
                            <img src=\"console.gif\" alt=c></a>
                        </td>\n";

		echo "  <td align=center>
                            <a href='showconlog.php3?node_id=$node_id".
		                  "&linecount=200'>
                            <img src=\"/icons/f.gif\" alt='console log'></a>
                        </td>\n";
	    }

	    if ($iswindowsnode) {
		echo "  <td align=center>
                            <a href='noderdp.php3?node_id=$node_id'>
                            <img src=\"rdp.gif\" alt=r></a>
                        </td>\n";
	    }
	    elseif ($anywindows) {
		echo "  <td>&nbsp;</td>\n";
	    }

	    echo "</tr>\n";
	}
	echo "</table>\n";
	echo "<h4><blockquote><blockquote><blockquote>
              <ol>
	        <li>A $stalemark indicates that the data is stale, and
	            the node has not reported on its proper schedule.</li>
                <li>Exit value of the node startup command. A value of
                        666 indicates a testbed internal error.</li>
              </ol>
              </blockquote></blockquote></blockquote></h4>\n";
    }
}

#
# Spit out an OSID link in user format.
#
function SPITOSINFOLINK($osid)
{
    if (! ($osinfo = OSinfo::Lookup($osid)))
	return;

    $osname = $osinfo->osname();
    
    echo "<a href='showosinfo.php3?osid=$osid'>$osname</a>\n";
}

#
# A list of widearea accounts.
#
function SHOWWIDEAREAACCOUNTS($webid) {
    $none = TBDB_TRUSTSTRING_NONE;

    if (! ($user = User::Lookup($webid))) {
	return;
    }
    $uid = $user->uid();
    
    $query_result =
	DBQueryFatal("SELECT * FROM widearea_accounts ".
		     "WHERE uid='$uid' and trust!='$none' ".
		     "order by node_id");
    
    if (! mysql_num_rows($query_result)) {
	return;
    }

    echo "<center>
          <h3>Widearea Accounts</h3>
          </center>
          <table align=center border=1 cellpadding=1 cellspacing=2>\n";

    echo "<tr>
              <th>Node ID</th>
              <th>Approved</th>
              <th>Privs</th>
          </tr>\n";

    while ($row = mysql_fetch_array($query_result)) {
	$node_id   = $row[node_id];
	$approved  = $row[date_approved];
	$trust     = $row[trust];

	if (TBTrustConvert($trust) != $TBDB_TRUST_NONE) {
	    echo "<tr>
                      <td>$node_id</td>
                      <td>$approved</td>
                      <td>$trust</td>\n";
	    echo "</tr>\n";
	}
    }
    echo "</table>\n";
}

#
# Add a LED like applet that turns on/off based on the output of a URL.
#
# @param uid The logged-in user ID.
# @param auth The value of the user's authentication cookie.
# @param pipeurl The url the applet should connect to to get LED status.  This
# string must include any parameters, or if there are none, end with a '?'.
#
# Example:
#   SHOWBLINKENLICHTEN($uid,
#                      $HTTP_COOKIE_VARS[$TBAUTHCOOKIE],
#                      "ledpipe.php3?node=em1");
#
function SHOWBLINKENLICHTEN($uid, $auth, $pipeurl, $width = 40, $height = 10) {
	echo "
          <applet code='BlinkenLichten.class' width='$width' height='$height'>
            <param name='pipeurl' value='$pipeurl'>
            <param name='uid' value='$uid'>
            <param name='auth' value='$auth'>
          </applet>\n";
}

#
# This is an included file.
# 
?>
