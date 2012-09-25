#!/usr/bin/perl -w
#
# Copyright (c) 2000-2012 University of Utah and the Flux Group.
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
use strict;
use Getopt::Std;
use English;
use Data::Dumper;
use POSIX qw(setsid);

#
# Invoked by xmcreate script to configure the control network for a vnode.
#
# NOTE: vmid should be an integer ID.
#
sub usage()
{
    print "Usage: emulab-cnet ".
	"vmid host_ip vnode_name vnode_ip (online|offline)\n";
    exit(1);
}

#
# Turn off line buffering on output
#
$| = 1;

# Drag in path stuff so we can find emulab stuff.
BEGIN { require "/etc/emulab/paths.pm"; import emulabpaths; }

#
# Load the OS independent support library. It will load the OS dependent
# library and initialize itself. 
# 
use libsetup;
use libtmcc;
use libvnode;

#
# Configure.
#
my $TMCD_PORT	= 7777;
my $SLOTHD_PORT = 8509;
my $EVPROXY_PORT= 16505;
my $IPTABLES	= "/sbin/iptables";

usage()
    if (@ARGV < 4);

my $vmid     = shift(@ARGV);
my $host_ip  = shift(@ARGV);
my $vnode_id = shift(@ARGV);
my $vnode_ip = shift(@ARGV);

# The caller (xmcreate) puts this into the environment.
my $vif         = $ENV{'vif'};
my $XENBUS_PATH = $ENV{'XENBUS_PATH'};
my $bridge      = `xenstore-read "$XENBUS_PATH/bridge"`;
chomp($bridge);

# We need these IP addresses.
my $boss_ip = `host boss | grep 'has address'`;
if ($boss_ip =~ /has address ([0-9\.]*)$/) {
    $boss_ip = $1;
}
my $ops_ip = `host ops | grep 'has address'`;
if ($ops_ip =~ /has address ([0-9\.]*)$/) {
    $ops_ip = $1;
}
my $fs_ip = `host fs | grep 'has address'`;
if ($fs_ip =~ /has address ([0-9\.]*)$/) {
    $fs_ip = $1;
}

# Each container gets a tmcc proxy running on another port.
my $local_tmcd_port = $TMCD_PORT + $vmid;

# Need this too.
my $outer_controlif = `cat $BOOTDIR/controlif`;
chomp($outer_controlif);

#
# First run the xen script to setup the bridge interface.
#
mysystem2("/etc/xen/scripts/vif-bridge @ARGV");
exit(1)
    if ($?);

#
# We setup a bunch of iptables rules when a container goes online, and
# then clear them when it goes offline.
#
sub Online()
{
    # Prevent dhcp requests from leaving the physical host.
    mysystem2("$IPTABLES -A FORWARD -o $bridge -m pkttype ".
	      "--pkt-type broadcast " .
	      "-m physdev --physdev-in $vif --physdev-is-bridged ".
	      "--physdev-out $outer_controlif -j DROP");
    return -1
	if ($?);
   
    mysystem2("$IPTABLES -A FORWARD -m physdev --physdev-in $vif -j ACCEPT");
    return -1
	if ($?);
    
    # Start a tmcc proxy (handles both TCP and UDP)
    my $tmccpid = fork();
    if ($tmccpid) {
	# Give child a chance to react.
	sleep(1);
	mysystem2("echo $tmccpid > /var/run/tmccproxy-$vnode_id.pid");
    }
    else {
	POSIX::setsid();
	
	exec("$BINDIR/tmcc.bin -d -t 15 -n $vnode_id ".
	       "  -X $host_ip:$local_tmcd_port -s $boss_ip -p $TMCD_PORT ".
	       "  -o $LOGDIR/tmccproxy.$vnode_id.log");
	die("Failed to exec tmcc proxy"); 
    }

    # Reroute tmcd calls to the proxy on the physical host
    mysystem2("$IPTABLES -t nat -A PREROUTING -j DNAT -p tcp ".
	   "  --dport $TMCD_PORT -d $boss_ip -s $vnode_ip ".
	   "  --to-destination $host_ip:$local_tmcd_port");
    return -1
	if ($?);

    mysystem2("$IPTABLES -t nat -A PREROUTING -j DNAT -p udp ".
	   "  --dport $TMCD_PORT -d $boss_ip -s $vnode_ip ".
	   "  --to-destination $host_ip:$local_tmcd_port");
    return -1
	if ($?);

    # Reroute evproxy to use the local daemon.
    mysystem2("$IPTABLES -t nat -A PREROUTING -j DNAT -p tcp ".
	   "  --dport $EVPROXY_PORT -d $ops_ip -s $vnode_ip ".
	   "  --to-destination $host_ip:$EVPROXY_PORT");
    return -1
	if ($?);
    
    #
    # GROSS! source-nat all traffic destined the fs node, to come from the
    # vnode host, so that NFS mounts work. We do this for non-shared nodes.
    # Shared nodes do the mounts normally from inside the guest. Maybe this
    # distinction is pointless, but it lowers the number of exported mounts
    # on the file server.
    #
    if (!SHAREDHOST()) {
	mysystem2("$IPTABLES -t nat -A POSTROUTING -j SNAT ".
	       "  --to-source $host_ip -s $vnode_ip --destination $fs_ip ".
	       "  -o $bridge");
	return -1
	    if ($?);
    }
    return 0;
}

sub Offline()
{
    # dhcp
    mysystem2("$IPTABLES -D FORWARD -o $bridge -m pkttype ".
	      "--pkt-type broadcast " .
	      "-m physdev --physdev-in $vif --physdev-is-bridged ".
	      "--physdev-out $outer_controlif -j DROP");
    mysystem2("$IPTABLES -D FORWARD -m physdev --physdev-in $vif -j ACCEPT");

    # tmcc
    # Reroute tmcd calls to the proxy on the physical host
    mysystem2("$IPTABLES -t nat -D PREROUTING -j DNAT -p tcp ".
	   "  --dport $TMCD_PORT -d $boss_ip -s $vnode_ip ".
	   "  --to-destination $host_ip:$local_tmcd_port");
    mysystem2("$IPTABLES -t nat -D PREROUTING -j DNAT -p udp ".
	   "  --dport $TMCD_PORT -d $boss_ip -s $vnode_ip ".
	   "  --to-destination $host_ip:$local_tmcd_port");

    if (-e "/var/run/tmccproxy-$vnode_id.pid") {
	my $pid = `cat /var/run/tmccproxy-$vnode_id.pid`;
	chomp($pid);
	mysystem2("/bin/kill $pid");
    }

    if (!SHAREDHOST()) {
	mysystem2("$IPTABLES -t nat -D POSTROUTING -j SNAT ".
	       "  --to-source $host_ip -s $vnode_ip --destination $fs_ip ".
	       "  -o $bridge");
    }

    # evproxy
    mysystem2("$IPTABLES -t nat -D PREROUTING -j DNAT -p tcp ".
	   "  --dport $EVPROXY_PORT -d $ops_ip -s $vnode_ip ".
	   "  --to-destination $host_ip:$EVPROXY_PORT");

    return 0;
}

if (@ARGV) {
    my $op = shift(@ARGV);
    if ($op eq "online") {
	exit(Online());
    }
    elsif ($op eq "offline") {
	exit(Offline());
    }
}
exit(0);
