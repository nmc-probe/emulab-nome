#!/usr/bin/perl -wT
#
# Copyright (c) 2013-2015 University of Utah and the Flux Group.
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
use English;
use Getopt::Std;

#
# Proxy for the blockstore server control program on boss.
#

sub usage()
{
    print STDERR "Usage: bscontrol [-hd] command args\n";
    print STDERR "   -h       This message\n";
    print STDERR "   -d       Print additional debug info\n";
    print STDERR "commands:\n";
    print STDERR "   pools    Print size info about pools\n";
    print STDERR "   volumes  Print info about volumes\n";
    print STDERR "   create <pool> <vol> <size> [ <fstype> ]\n";
    print STDERR "            Create <vol> in <pool> with <size> in MiB; optionally create a filesystem of type <fstype> on it\n";
    print STDERR "   snapshot <pool> <vol> <tstamp>\n";
    print STDERR "            Create a snapshot of <pool>/<vol> with timestamp <tstamp>\n";
    print STDERR "   clone    <pool> <ovol> <nvol> [ <tstamp> ]\n";
    print STDERR "            Create a clone of <pool>/<vol> called <nvol> from the snapshot at <tstamp> (most recent if not specified)\n";
    print STDERR "   destroy <pool> <vol>\n";
    print STDERR "            Destroy <vol> in <pool>\n";
    print STDERR "   desnapshot <pool> <vol> [ <tstamp> ]\n";
    print STDERR "            Destroy snapshot <vol>/<pool>@<tstamp>; if <tstamp> is not given, destroy all snapshots\n";
    print STDERR "   declone <pool> <vol>\n";
    print STDERR "            Destroy clone <vol> in <pool>; also destroys associated snapshot if this is the last clone\n";
    exit(-1);
}
my $optlist  = "hd";
my $debug = 0;

#
# Turn off line buffering on output
#
$| = 1;

# Drag in path stuff so we can find emulab stuff.
BEGIN { require "/etc/emulab/paths.pm"; import emulabpaths; }

# Libraries
use libfreenas;

# Protos
sub fatal($);

# Commands
my %cmds = (
    "pools"      => \&pools,
    "volumes"    => \&volumes,
    "create"     => \&create,
    "snapshot"   => \&snapshot,
    "clone"      => \&clone,
    "destroy"    => \&destroy,
    "desnapshot" => \&desnapshot,
    "declone"    => \&declone,
);

#
# Parse command arguments. Once we return from getopts, all that should be
# left are the required arguments.
#
my %options = ();
if (! getopts($optlist, \%options)) {
    usage();
}
if (defined($options{h})) {
    usage();
}
if (defined($options{d})) {
    $debug = 1;
}
if (@ARGV < 1) {
    usage();
}

my $cmd = shift;
if (!exists($cmds{$cmd})) {
    print STDERR "Unrecognized command '$cmd', should be one of:\n";
    print STDERR "  ", join(", ", keys %cmds), "\n";
    usage();
}

exit(&{$cmds{$cmd}}(@ARGV));

#
# Print all the available pools from which blockstores can be allocated
# along with size info.
#
sub pools()
{
    my $pref = freenasPoolList();
    foreach my $pool (keys %{$pref}) {
	my $size = int($pref->{$pool}->{'size'});
	my $avail = int($pref->{$pool}->{'avail'});
	print "pool=$pool size=$size avail=$avail\n";
    }

    return 0;
}

#
# Print uninterpreted volume info.
#
sub volumes()
{
    my $vref = freenasVolumeList(1,1);
    foreach my $vol (keys %{$vref}) {
	my $pool = $vref->{$vol}->{'pool'};
	my $iname = $vref->{$vol}->{'iname'};
	my $size = int($vref->{$vol}->{'size'});
	my $snapshots = $vref->{$vol}->{'snapshots'};
	my $cloneof = $vref->{$vol}->{'cloneof'};

	print "volume=$vol pool=$pool size=$size";
	if ($iname) {
	    print " iname=$iname";
	}
	if ($cloneof) {
	    print " cloneof=$cloneof";
	}
	if ($snapshots) {
	    print " snapshots=$snapshots";
	}
	print "\n";
    }

    return 0;
}

sub create($$$;$)
{
    my ($pool,$vol,$size,$fstype) = @_;

    if (defined($pool) && $pool =~ /^([-\w]+)$/) {
	$pool = $1;
    } else {
	print STDERR "bscontrol_proxy: bogus pool arg\n";
	return 1;
    }
    if (defined($vol) && $vol =~ /^([-\w]+)$/) {
	$vol = $1;
    } else {
	print STDERR "bscontrol_proxy: bogus volume arg\n";
	return 1;
    }
    if (defined($size) && $size =~ /^(\d+)$/) {
	$size = $1;
    } else {
	print STDERR "bscontrol_proxy: bogus size arg\n";
	return 1;
    }
    if (!defined($fstype)) {
	$fstype = "none";
    } elsif ($fstype =~ /^(ext2|ext3|ext4|ufs)$/) {
	$fstype = $1;
    } else {
	print STDERR "bscontrol_proxy: bogus fstype arg\n";
	return 1;
    }

    my $rv = freenasVolumeCreate($pool, $vol, $size);
    if ($rv == 0 && $fstype ne "none") {
	$rv = freenasFSCreate($pool, $vol, $fstype);
    }

    return $rv;
}

sub snapshot($$$)
{
    my ($pool,$vol,$tstamp) = @_;

    if (defined($pool) && $pool =~ /^([-\w]+)$/) {
	$pool = $1;
    } else {
	print STDERR "bscontrol_proxy: bogus pool arg\n";
	return 1;
    }
    if (defined($vol) && $vol =~ /^([-\w]+)$/) {
	$vol = $1;
    } else {
	print STDERR "bscontrol_proxy: bogus volume arg\n";
	return 1;
    }
    if (defined($tstamp) && $tstamp =~ /^(\d+)$/) {
	$tstamp = $1;
    } else {
	print STDERR "bscontrol_proxy: bogus tstamp arg\n";
	return 1;
    }

    return freenasVolumeSnapshot($pool, $vol, $tstamp);
}

sub desnapshot($$$)
{
    my ($pool,$vol,$tstamp) = @_;

    if (defined($pool) && $pool =~ /^([-\w]+)$/) {
	$pool = $1;
    } else {
	print STDERR "bscontrol_proxy: bogus pool arg\n";
	return 1;
    }
    if (defined($vol) && $vol =~ /^([-\w]+)$/) {
	$vol = $1;
    } else {
	print STDERR "bscontrol_proxy: bogus volume arg\n";
	return 1;
    }
    if (defined($tstamp)) {
	if ($tstamp =~ /^(\d+)$/) {
	    $tstamp = $1;
	} else {
	    print STDERR "bscontrol_proxy: bogus tstamp arg\n";
	    return 1;
	}
    }

    return freenasVolumeDesnapshot($pool, $vol, $tstamp);
}

sub clone($$$;$)
{
    my ($pool,$ovol,$nvol,$tstamp) = @_;

    if (defined($pool) && $pool =~ /^([-\w]+)$/) {
	$pool = $1;
    } else {
	print STDERR "bscontrol_proxy: bogus pool arg\n";
	return 1;
    }
    if (defined($ovol) && $ovol =~ /^([-\w]+)$/) {
	$ovol = $1;
    } else {
	print STDERR "bscontrol_proxy: bogus origin volume arg\n";
	return 1;
    }
    if (defined($nvol) && $nvol =~ /^([-\w]+)$/) {
	$nvol = $1;
    } else {
	print STDERR "bscontrol_proxy: bogus clone volume arg\n";
	return 1;
    }
    if (defined($tstamp)) {
	if ($tstamp =~ /^(\d+)$/) {
	    $tstamp = $1;
	} else {
	    print STDERR "bscontrol_proxy: bogus tstamp arg\n";
	    return 1;
	}
    } else {
	# zero means most recent
	$tstamp = 0;
    }

    return freenasVolumeClone($pool, $ovol, $nvol, $tstamp);
}

sub destroy($$$)
{
    my ($pool,$vol) = @_;

    if (defined($pool) && $pool =~ /^([-\w]+)$/) {
	$pool = $1;
    } else {
	print STDERR "bscontrol_proxy: bogus pool arg\n";
	return 1;
    }
    if (defined($vol) && $vol =~ /^([-\w]+)$/) {
	$vol = $1;
    } else {
	print STDERR "bscontrol_proxy: bogus volume arg\n";
	return 1;
    }

    return freenasVolumeDestroy($pool, $vol);
}

sub declone($$$)
{
    my ($pool,$vol) = @_;

    if (defined($pool) && $pool =~ /^([-\w]+)$/) {
	$pool = $1;
    } else {
	print STDERR "bscontrol_proxy: bogus pool arg\n";
	return 1;
    }
    if (defined($vol) && $vol =~ /^([-\w]+)$/) {
	$vol = $1;
    } else {
	print STDERR "bscontrol_proxy: bogus volume arg\n";
	return 1;
    }

    return freenasVolumeDeclone($pool, $vol);
}
