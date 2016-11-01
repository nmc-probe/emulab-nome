<?php
include("defs.php3");

$query_result =
    DBQueryFatal("select distinct n.node_id,n.phys_nodeid,n.type,ns.status, ".
                 "   n.def_boot_osid,n.reserved_pid, ".
                 "   if(r.pid is not null,r.pid,rp.pid) as pid, ".
                 "   if(r.pid is not null,r.eid,rp.eid) as eid, ".
                 "   nt.class, ".
                 "   if(r.pid is not null,r.vname,rp.vname) as vname ".
                 "from nodes as n ".
                 "left join node_types as nt on n.type=nt.type ".
                 "left join node_status as ns on n.node_id=ns.node_id ".
                 "left join reserved as r on n.node_id=r.node_id ".
                 "left join reserved as rp on n.phys_nodeid=rp.node_id ".
                 "$additionalLeftJoin ".
                 "where (role = 'testnode' or role = 'virtnode') ".
                 "ORDER BY priority");


#
# First count up free nodes as well as status counts.
#
$num_free = 0;
$num_up   = 0;
$num_pd   = 0;
$num_down = 0;
$num_unk  = 0;
$freetypes= array();

while ($row = mysql_fetch_array($query_result)) {
    $pid                = $row["pid"];
    $status             = $row["status"];
    $type               = $row["type"];

    if (! isset($freetypes[$type])) {
        $freetypes[$type] = 0;
    }
    if (!$pid) {
        $num_free++;
        $freetypes[$type]++;
        continue;
    }
    switch ($status) {
    case "up":
        $num_up++;
        break;
    case "possibly down":
    case "unpingable":
        $num_pd++;
        break;
    case "down":
        $num_down++;
        break;
    default:
        $num_unk++;
        break;
    }
}
$num_total = ($num_free + $num_up + $num_down + $num_pd + $num_unk);
mysql_data_seek($query_result, 0);

$node_numbers = array (
  'up'            => $num_up,
  'down'          => $num_down + $num_pd,
  'free'          => $num_free,
  'unknown'       => $num_unk,
  'total'         => $num_total);

echo json_encode($node_numbers), "\n";

?>
