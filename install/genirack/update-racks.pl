#!/usr/bin/perl
use strict;
use Getopt::Std;
use POSIX qw(strftime);
use lib "/usr/testbed/lib";
use emutil;

#  wap /usr/testbed/sbin/update_sitevars 
#  wap /usr/testbed/bin/editnodetype ../emulab-devel/install/ctrltype.xml 
#  wap /usr/testbed/sbin/addservers 
#  wap setsitevar general/arplockdown staticonly

sub usage {
    print "Usage: $0 [options] [rack]\n";
    print "Options:\n";
    print "-r      - Rsync software to rack.\n";
    print "-b      - Build the software (with reconfig).\n";
    print "-i      - Install on each rack.\n";
    print "-l      - List all racks.\n";
    print "-p arg  - Update shared pool on each rack.\n";
    print "          arg is type,func where type=xen|openvz\n";
    print "-u      - Do Utah rack.\n";
    print "-d      - Do DDC rack.\n";
    print "-a      - Do APT rack.\n";
    print "-U      - Skip Utah rack.\n";
    print "-D      - Skip DDC rack.\n";
    print "-A      - Skip APT rack.\n";
    print "-7      - Just G7 racks.\n";
    print "-8      - Just G8 racks.\n";
    print "-f      - Run function instead. Add -F to shutdown testbed\n";
    print "-s      - No parallelization in -r, -f, or -b.\n";
    print "-t      - Tag source with instageni-YYYYMMDD\n";
    print "rack    - Specific rack, or all racks\n";
    exit(1);
}
my $optlist    = "binuUdDhfFrlc78tsp:aA";
my $rebuild    = 0;
my $install    = 0;
my $rsync      = 0;
my $dofunc     = 0;
my $dotag      = 0;
my $nopar      = 0;
my $dopool;
my $rack;

my $TB       = "/usr/testbed";
my $UTAHBOSS = "boss.utah.geniracks.net";
my $UTAHCTRL = "control.utah.geniracks.net";
my $DDCBOSS  = "boss.utahddc.geniracks.net",
my $DDCCTRL  = "control.utahddc.geniracks.net",
my $APTBOSS  = "boss.apt.emulab.net",
my $APTCTRL  = "boss.apt.emulab.net",
my %G7RACKS  = ("bbn"       => [ "boss.instageni.gpolab.bbn.com",
				 "gpolab.control-nodes.geniracks.net" ],
		"nwu"       => [ "boss.instageni.northwestern.edu",
				 "nu.control-nodes.geniracks.net" ],
		"uky"       => [ "boss.lan.sdn.uky.edu",
				 "uky.control-nodes.geniracks.net" ],
		"kettering" => [ "boss.geni.kettering.edu",
				 "kettering.control-nodes.geniracks.net"],
		"gatech"    => [ "boss.instageni.rnoc.gatech.edu",
				 "gatech.control-nodes.geniracks.net" ],
		"princeton" => [ "boss.instageni.cs.princeton.edu",
				 "princeton.control-nodes.geniracks.net" ],
		"clemson"   => [ "boss.instageni.clemson.edu",
				 "clemson.control-nodes.geniracks.net" ],
		"kansas"    => [ "boss.instageni.ku.gpeni.net",
				 "kansas.control-nodes.geniracks.net" ],
		"nyu"       => [ "boss.genirack.nyu.edu",
				 "nyu.control-nodes.geniracks.net" ],
		"idaho"     => [ "boss.instageni.uidaho.edu",
				 "idaho.control-nodes.geniracks.net" ],
);
my %G8RACKS = ("max"        => [ "boss.instageni.maxgigapop.net",
				 "max.control-nodes.geniracks.net" ],
	       "nysernet"   => [ "boss.instageni.nysernet.org",
				 "nysernet.control-nodes.geniracks.net" ],
	       "sox"        => [ "boss.instageni.sox.net",
				 "sox.control-nodes.geniracks.net" ],
	       "urbana"     => [ "boss.instageni.illinois.edu",
				 "urbana.control-nodes.geniracks.net" ],
	       "missouri"   => [ "boss.instageni.rnet.missouri.edu",
				 "missouri.control-nodes.geniracks.net" ],
	       "wisc"       => [ "boss.instageni.wisc.edu",
				 "wisc.control-nodes.geniracks.net" ],
	       "rutgers"    => [ "boss.instageni.rutgers.edu",
				 "rutgers.control-nodes.geniracks.net" ],
	       "stanford"   => [ "boss.instageni.stanford.edu",
				 "stanford.control-nodes.geniracks.net" ],
	       "cornell"    => [ "boss.geni.it.cornell.edu",
				 "cornell.control-nodes.geniracks.net" ],
	       "lsu"	    => [ "boss.instageni.lsu.edu",
				 "lsu.control-nodes.geniracks.net" ],
	       "case"	    => [ "boss.geni.case.edu",
				 "casewestern.control-nodes.geniracks.net" ],
	       "moxi"	    => [ "boss.instageni.iu.edu",
				 "moxi.control-nodes.geniracks.net" ],
	       "chicago"    => [ "boss.geni.uchicago.edu",
				 "chicago.control-nodes.geniracks.net" ],
	       "metro"	    => [ "boss.instageni.metrodatacenter.com",
				 "dublin.control-nodes.geniracks.net" ],
	       "nps"        => [ "boss.instageni.nps.edu",
				 "nps.control-nodes.geniracks.net" ],
	       "ohio"       => [ "boss.instageni.osu.edu",
				 "ohio.control-nodes.geniracks.net" ],
	       "umkc"       => [ "boss.instageni.umkc.edu",
				 "umkc.control-nodes.geniracks.net" ],
	       "ucla"	    => [ "boss.instageni.idre.ucla.edu",
				 "ucla.control-nodes.geniracks.net" ],
);

sub fatal($)
{
    my ($msg) = @_;

    die("*** $0:\n".
	"    $msg\n");
}
sub SSH($$);
sub TagSchema();

#
# Turn off line buffering on output
#
$| = 1;

#
# Check args.
#
my %options = ();
if (! getopts($optlist, \%options)) {
    usage();
}
if (defined($options{"h"})) {
    usage();
}
# Look at "control" option before setting racks.
my $which    = (defined($options{"c"}) ? 1 : 0);
my $UTAHRACK = (defined($options{"c"}) ? $UTAHCTRL : $UTAHBOSS);
my $DDCRACK  = (defined($options{"c"}) ? $DDCCTRL  : $DDCBOSS);
my $APTRACK  = (defined($options{"c"}) ? $APTCTRL  : $APTBOSS);
my @G7RACKS  = map { $G7RACKS{$_}[$which] } keys(%G7RACKS);
my @G8RACKS  = map { $G8RACKS{$_}[$which] } keys(%G8RACKS);
my @ALLRACKS = (@G7RACKS, @G8RACKS);
my @TODO     = ($UTAHRACK, $DDCRACK, @ALLRACKS);
my %SKIP     = ();
my $HOME     = "/home/stoller";

if (defined($options{"l"})) {
    foreach my $name (@ALLRACKS) {
	print "$name\n";
    }
    exit(0);
}
if (defined($options{"i"})) {
    $install = 1;
}
if (defined($options{"s"})) {
    $nopar = 1;
}
if (defined($options{"f"})) {
    $dofunc = 1;
    if (defined($options{"F"})) {
	$dofunc++;
    }
}
if (defined($options{"b"})) {
    $rebuild = 1;
}
if (defined($options{"r"})) {
    $rsync = 1;
}
if (defined($options{"p"})) {
    $dopool = $options{"p"};
    $dofunc = 1;
}
if (defined($options{"u"}) || defined($options{"d"}) ||
    defined($options{"a"})) {
    @TODO = ();
    push(@TODO, $APTRACK)
	if (defined($options{"a"}));
    push(@TODO, $UTAHRACK)
	if (defined($options{"u"}));
    push(@TODO, $DDCRACK)
	if (defined($options{"d"}));
}
elsif (defined($options{"7"})) {
    @TODO = @G7RACKS;
    if (! defined($options{"U"})) {
	@TODO = ($UTAHRACK, @TODO);
    }
}
elsif (defined($options{"8"})) {
    @TODO = @G8RACKS;
    if (! defined($options{"D"})) {
	@TODO = ($DDCRACK, @TODO);
    }
}
elsif (@ARGV) {
    @TODO = ();
    
    foreach my $arg (@ARGV) {
	if ($arg =~ /\./) {
	    push(@TODO, $arg);
	}
	elsif (exists($G7RACKS{$arg})) {
	    push(@TODO, $G7RACKS{$arg}[$which]);
	}
	elsif (exists($G8RACKS{$arg})) {
	    push(@TODO, $G8RACKS{$arg}[$which]);
	}
	else {
	    fatal("No such rack: $arg");
	}
    }
}
else {
    @TODO = @ALLRACKS;
    if (! defined($options{"D"})) {
	@TODO = ($DDCRACK, @TODO);
    }
    if (! defined($options{"U"})) {
	@TODO = ($UTAHRACK, @TODO);
    }
}

if (defined($options{"t"})) {
    TagSchema();
}

#
# Just a way to run some little bits of code on the target. 
#
if ($dofunc && !$install) {
    my $coderef = sub {
	my ($rack) = @_;

	print "Running function on $rack ...\n";

	if ($dofunc > 1) {
	    print "-> Shutting down testbed ...\n";
	    if (SSH($rack,
		  "(sudo $TB/sbin/testbed-control shutdown >& shutdown.log)")) {
		print STDERR "** could not shutdown!\n";
		return 1;
	    }
	}
	my $devel = "emulab-devel/emulab-devel";
	
	print "-> Running function ...\n";
	my $command = "";
	if (defined($dopool)) {
	    my ($type,$func,$limit) = split(',', $dopool);
	    usage()
		if (!(defined($type) && defined($func)));
	    
	    $command = "$devel/update-shared.pl -t $type -f $func ".
		(defined($limit) ? "-l $limit" : "");
	}
	elsif (1) {
	    $command = "cd /tmp; wget http://www.emulab.net/downloads/ops-monitoring.tar.gz && cd /usr/local && sudo tar xf /tmp/ops-monitoring.tar.gz; mysqladmin create monitoring; cd /usr/local/ops-monitoring/local/unit-tests && python ./local_table_reset.py; sudo /usr/testbed/sbin/protogeni/mondbd";
	}
	elsif (0) {
	    $command = "mkdir /home/elabman/openvpn"
	}
	elsif (1) {
	    $command =
		"cd emulab-devel/obj/dhcpd; gmake; sudo gmake install; ".
		"cd ..; dhcpd_makeconf -i -r";
	}
	elsif (0) {
	    $command = "scp -p /usr/testbed/etc/openvpn-server.pem elabman\@control:openvpn; scp -p /usr/testbed/etc/openvpn-dh.pem elabman\@control:openvpn; scp -p /usr/testbed/etc/emulab.pem elabman\@control:openvpn; scp -p openvpn/openvpn.conf elabman\@control:openvpn";
	}
	elsif (0) {
	    $command = "cd /usr/ports/converters/p5-JSON; ".
		"sudo make install; ".
		"cd /usr/ports/net-mgmt/p5-Net-IP; ".
		"sudo make install";
	}
	elsif (0) {
	    $command = "sudo -u geniuser /usr/testbed/sbin/wap ".
		"/usr/testbed/sbin/image_import -g -p ch-geni-net ".
		"https://www.utahddc.geniracks.net/image_metadata.php\\\?uuid=da660c93-2134-11e3-85ef-000000000000";
	}
	elsif (1) {
	    $command =
		"sudo /usr/testbed/sbin/initilo.pl -i ".
		"    pc1 pc2 pc3 pc4 pc5 control-ilo";
	}
	elsif (0) {
	    $command = "sudo scp $devel/xendomains ".
		"  vhost3.shared-nodes.emulab-ops:/usr/local/etc/emulab";
	}
	elsif (0) {
	    $command = "sudo ssh vhost1.shared-nodes.emulab-ops ".
		"sysctl -w net.netfilter.nf_conntrack_generic_timeout=120 ".
		" net.netfilter.nf_conntrack_tcp_timeout_established=54000 ".
		" net.netfilter.nf_conntrack_max=131071";
	}
	elsif (0) {
	    $command = "sudo ssh vhost3.shared-nodes.emulab-ops ".
		"/etc/rc3.d/S23ntp restart";
	}
	elsif (0) {
	    # Utah Rack is vhost2. Sheesh.
	    $command =
		"sudo ssh vhost1.shared-nodes.emulab-ops ".
		"   systemctl restart ntpd.service; " .
		"sudo ssh vhost2.shared-nodes.emulab-ops ".
		"  systemctl restart ntpd.service; ";
	}
	elsif (0) {
	    $command =
		"cd emulab-devel/obj/db; sudo gmake /usr/testbed/lib/Node.pm";
	}
	elsif (0) {
	    $command = "cd emulab-devel/obj/ntpd; ".
		"sudo gmake install; ".
		"sudo /etc/rc.d/ntpd restart";
	}
	elsif (0) {
	    $command =
		"ssh ops \"(cd emulab-devel/obj/ntpd; ".
		"sudo gmake control-install; ".
		"sudo /etc/rc.d/ntpd restart)\"";
	}
	elsif (0) {
	    $command =
	    "chmod 664 emulab-devel/defs-genirack; ".
	    "  echo 'BROWSER_CONSOLE_ENABLE=1' >> emulab-devel/defs-genirack; ".
	    "  sudo scp $devel/capture-nossl ".
	    "    vhost3.shared-nodes.emulab-ops:/usr/local/etc/emulab/capture";
	}
	elsif (0) {
	    #$command = "sudo /usr/testbed/sbin/getimages";
	    #$command = "/usr/testbed/sbin/wap /usr/testbed/sbin/grantimage -a -x emulab-ops,Ubuntu12-64-OVS";
	    $command = "emulab-devel/emulab-devel/runsonxen.pl -r ".
		"emulab-ops,FEDORA15-STD";
	}
	else {
	    $command = "cat emulab-devel/emulab-devel/foo.sql | mysql tbdb";
	}
	#$command = "($command >& /tmp/function.log)";
	
	if (SSH($rack, $command)) {
	    print STDERR "Error running '$command' on $rack\n";
	    return 2;
	}
	if ($dofunc > 1) {
	    print "-> Booting the testbed ...\n";
	    if (SSH($rack,
		    "(sudo $TB/sbin/testbed-control boot >& boot.log)")) {
		print STDERR "** could not boot!\n";
		return 3;
		next;
	    }
	}
    };

    # List of racks that we can proceed with.
    my @doracks = @TODO;
    # Return codes for each rack. 
    my @results = ();
    if (ParRun({"maxwaittime" => 99999, "maxchildren" => ($nopar ? 1 : 6)},
	       \@results, $coderef, @doracks)) {
	fatal("ParRun failed!");
    }

    #
    # Check the exit codes. 
    #
    my $count = 0;
    foreach my $result (@results) {
	my ($rack) = $doracks[$count];

	if ($result) {
	    $result = $result >> 8;
	    if ($result == 1) {
		$SKIP{$rack} = "could not shutdown";
	    }
	    elsif ($result == 2) {
		$SKIP{$rack} = "could not run function";
	    }
	    elsif ($result == 3) {
		$SKIP{$rack} = "could not retstart";
	    }
	}
	$count++;
    }
    
    if (keys(%SKIP)) {
	print "The following racks failed!\n";
	foreach my $rack (keys(%SKIP)) {
	    my $reason = $SKIP{$rack};
	    print "$rack: $reason\n";
	}
    }
    exit(scalar(keys(%SKIP)));
}

#
# First do all of the rsyncs.
#
if ($rsync) {
    my $coderef = sub {
	my ($rack) = @_;

	print "rsyncing $rack ...\n";
	print "-> rsyncing emulab-devel\n";
	system("rsync -a --timeout=30 --delete ".
	       "--exclude-from $HOME/.rsyncignore ".
	       "     $HOME/testbed-noelvin/emulab-devel ".
	       "     $HOME/testbed-noelvin/reconfig.rack ".
	       "  elabman\@${rack}:emulab-devel");
	if ($?) {
	    print STDERR "** $rack: error rsyncing emulab-devel\n";
	    return 1;
	}
	print "-> rsyncing pubsub\n";
	system("rsync -a --timeout=30 --delete ".
	       "--exclude-from $HOME/.rsyncignore ".
	       "     $HOME/testbed-noelvin/pubsub elabman\@${rack}:");
	if ($?) {
	    print STDERR "** $rack: error rsyncing pubsub\n";
	    return 1;
	}
	print "-> rsyncing shellinabox\n";
	system("rsync -a --timeout=30 --delete ".
	       "--exclude-from $HOME/.rsyncignore ".
	       "     $HOME/testbed-noelvin/shellinabox elabman\@${rack}:");
	if ($?) {
	    print STDERR "** $rack: error rsyncing shellinabox\n";
	    return 1;
	}
    };

    # List of racks that we can proceed with.
    my @doracks = @TODO;
    # Return codes for each rack. 
    my @results = ();
    if (ParRun({"maxwaittime" => 99999, "maxchildren" => ($nopar ? 1 : 6)},
	       \@results, $coderef, @doracks)) {
	fatal("ParRun failed!");
    }

    #
    # Check the exit codes. 
    #
    my $count = 0;
    foreach my $result (@results) {
	my ($rack) = $doracks[$count];

	if ($result) {
	    $SKIP{$rack} = "could not rsync";
	}
	$count++;
    }
    
    if (keys(%SKIP)) {
	print "The following racks failed!\n";
	foreach my $rack (keys(%SKIP)) {
	    my $reason = $SKIP{$rack};
	    print "$rack: $reason\n";
	}
    }
}

if ($rebuild) {
    my $coderef = sub {
	my ($rack) = @_;

	print "rebuilding on $rack ...\n";
	print "-> $rack: Starting reconfig ...\n";
	if (SSH($rack,
		"(cd emulab-devel; ./reconfig.rack >& reconfig.log)")) {
	    print STDERR "** $rack: could not reconfig!\n";
	    return 1;
	}
	print "-> $rack: Starting clean ...\n";
	if (SSH($rack,
		"(cd emulab-devel/obj; sudo gmake clean >& clean.log)")) {
	    print STDERR "** $rack: could not clean!\n";
	    return 2;
	}
	print "-> $rack: Starting rebuild ...\n";
	if (SSH($rack,
		"(cd emulab-devel/obj; gmake >& rebuild.log)")) {
	    print STDERR "** $rack: could not rebuild!\n";
	    return 3;
	}
	return 0;
    };

    # List of racks that we can proceed with.
    my @doracks = ();
    foreach my $rack (@TODO) {
	if (exists($SKIP{$rack})) {
	    print "skipping rebuild on $rack\n";
	    next;
	}
	push(@doracks, $rack);
    }
    # Return codes for each rack. 
    my @results = ();
    if (ParRun({"maxwaittime" => 99999, "maxchildren" => ($nopar ? 1 : 8)},
	       \@results, $coderef, @doracks)) {
	fatal("ParRun failed!");
    }

    #
    # Check the exit codes. 
    #
    my $count = 0;
    foreach my $result (@results) {
	my ($rack) = $doracks[$count];

	if ($result) {
	    $result = $result >> 8;
	    if ($result == 1) {
		$SKIP{$rack} = "could not reconfig";
	    }
	    elsif ($result == 2) {
		$SKIP{$rack} = "could not clean";
	    }
	    elsif ($result == 3) {
		$SKIP{$rack} = "could not rebuild";
	    }
	}
	$count++;
    }
}

if ($install) {
    foreach my $rack (@TODO) {
	if (exists($SKIP{$rack})) {
	    print "skipping install on $rack\n";
	    next;
	}
	print "installing on $rack ...\n";

	print "-> Shutting down testbed ...\n";
	if (SSH($rack,
	     "(sudo $TB/sbin/testbed-control shutdown >& /tmp/shutdown.log)")) {
	    print STDERR "** could not shutdown!\n";
	    $SKIP{$rack} = "could not shutdown";
	    next;
	}
	print "-> Starting install on boss ...\n";
	if (SSH($rack, "(cd emulab-devel/obj; ".
		"    sudo gmake update-testbed-nostop >& /tmp/install.log)")) {
	    print STDERR "** could not install on boss!\n";
	    $SKIP{$rack} = "could not install on boss";
	    next;
	}
	print "-> Starting install on ops ...\n";
	my $rackops = $rack;
	$rackops =~ s/^boss/ops/;
	
	if (SSH($rackops, "(cd emulab-devel/obj/clientside; ".
		"        sudo gmake control-install >>& /tmp/install.log)")) {
	    print STDERR "** could not install on ops!\n";
	    $SKIP{$rack} = "could not install ops";
	    next;
	}
	if ($dofunc) {
	    print "-> Running function ...\n";
 	    my $command = "/bin/ls /";
	    if (1) {
		$command = "cd emulab-devel/obj/install; ".
		    " sudo perl emulab-install -b -u -i boss/mfs boss ";
	    }
	    if (defined($command) &&
		SSH($rack, "($command >& /tmp/function.log)")) {
		print STDERR "Error running '$command' on $rack\n";
		$SKIP{$rack} = "could not run function";
		next;
	    }
	}
	print "-> Booting the testbed ...\n";
	if (SSH($rack,
		"(sudo $TB/sbin/testbed-control boot >& /tmp/boot.log)")) {
	    print STDERR "** could not boot!\n";
	    $SKIP{$rack} = "could not boot";
	    next;
	}
    }
}

if (keys(%SKIP)) {
    print "The following racks failed!\n";
    foreach my $rack (keys(%SKIP)) {
	my $reason = $SKIP{$rack};
	print "$rack: $reason\n";
    }
}

sub SSH($$)
{
    my ($host, $cmd, $timeout) = @_;
    $timeout = 2500 if (!defined($timeout));
    
    my $childpid = fork();

    if ($childpid) {
	local $SIG{ALRM} = sub { kill("TERM", $childpid); };
	alarm $timeout;
	waitpid($childpid, 0);
	alarm 0;

	my $stat = $?;

	#
	# Any failure, revert to plain reboot below.
	#
	if ($?) {
	    return -1;
	}
	return 0;
    }
    else {
	exec("ssh elabman\@${host} '$cmd'");
	exit(1);
    }
}

sub TagSchema()
{
    my $tag  = POSIX::strftime("instageni-20%y%m%d", localtime(time()));
    print "Tagging with $tag\n";
    system("git tag -f -m 'Push to InstaGeni Racks' $tag");
    fatal("Could not tag repo!")
	if ($?);
    system("git push --tags");
    fatal("Could not push tag up!")
	if ($?);
    return 0;
}