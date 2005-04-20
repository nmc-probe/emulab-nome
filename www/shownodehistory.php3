<?php
#
# EMULAB-COPYRIGHT
# Copyright (c) 2000-2005 University of Utah and the Flux Group.
# All rights reserved.
#
include("defs.php3");
include("showstuff.php3");

#
# Standard Testbed Header
#
PAGEHEADER("Node History");

#
# Only known and logged in users can do this.
#
$uid = GETLOGIN();
LOGGEDINORDIE($uid);
$isadmin = ISADMIN($uid);

if (! ($isadmin || OPSGUY())) {
    USERERROR("Cannot view node history.", 1);
}

#
# Verify form arguments.
# 
if (!isset($showall)) {
    $showall = 0;
}
if (!isset($count)) {
    $count = 20;
}
if (!isset($reverse)) {
    $reverse = 1;
}

if (!isset($node_id) || strcmp($node_id, "") == 0) {
    $node_id = "";
} else {
    #
    # Check to make sure that this is a valid nodeid
    #
    if (!TBValidNodeName($node_id)) {
	USERERROR("$node_id is not a valid node name!", 1);
    }
}

$opts="node_id=$node_id&count=$count&reverse=$reverse";
echo "<b>Show records: ";
if ($showall) {
    echo "<a href='shownodehistory.php3?$opts'>allocated only</a>,
          all";
} else {
    echo "allocated only,
          <a href='shownodehistory.php3?$opts&showall=1'>all</a>";
}

$opts="node_id=$node_id&count=$count&showall=$showall";
echo "<br><b>Order by: ";
if ($reverse == 0) {
    echo "<a href='shownodehistory.php3?$opts&reverse=1'>lastest first</a>,
          earliest first";
} else {
    echo "lastest first,
          <a href='shownodehistory.php3?$opts&reverse=0'>earliest first</a>";
}

$opts="node_id=$node_id&showall=$showall&reverse=$reverse";
echo "<br><b>Show number: ";
if ($count != 20) {
    echo "<a href='shownodehistory.php3?$opts&count=20'>first 20</a>, ";
} else {
    echo "first 20, ";
}
if ($count != -20) {
    echo "<a href='shownodehistory.php3?$opts&count=-20'>last 20</a>, ";
} else {
    echo "last 20, ";
}
if ($count != 0) {
    echo "<a href='shownodehistory.php3?$opts&count=0'>all</a>";
} else {
    echo "all";
}

SHOWNODEHISTORY($node_id, $showall, $count, $reverse);

#
# Standard Testbed Footer
# 
PAGEFOOTER();
?>
