#!/usr/bin/perl -w
#
# EMULAB-COPYRIGHT
# Copyright (c) 2000-2003 University of Utah and the Flux Group.
# All rights reserved.
#
use English;
use Getopt::Std;

#
# The point of this is to fire up the init code inside the jail,
# and then wait for a signal from outside the jail. When that happens
# kill off everything inside the jail and exit. So, like a mini version
# of /sbin/init, since killing the jail cleanly from outside the jail
# turns out to be rather difficult, and doing it from inside is very easy!
#
# Note though, when the machine is shutdown we can cleanly send this
# script a signal from mkjail. If reboot is used instead of shutdown,
# the system broadcasts SIGTERM, and this script will catch that and
# die. However, thats not good cause the caller (mkjail) wipes out the
# jail when this script exits of its own accord. So, ignore TERM, and wait
# for mkjail to send us a USR1, which means to exit.
#
my $DEFCONSIX = "/bin/sh /etc/rc";

#
# Catch TERM.
# 
sub handler () {
    $SIG{USR1} = 'IGNORE';
    system("kill -TERM -1");
    sleep(1);
    system("kill -KILL -1");
    exit(1);
}
$SIG{TERM} = 'IGNORE';
$SIG{USR1} = \&handler;

my $childpid = fork();
if (!$childpid) {
    $SIG{TERM} = 'DEFAULT';
    
    if (@ARGV) {
	exec @ARGV;
    }
    else {
	exec $DEFCONSIX;
    }
    die("*** $0:\n".
	"    exec failed: '@ARGV'\n");
}

#
# If a command list was provided, we wait for whatever it was to
# finish. Otherwise sleep forever. 
#
if (@ARGV) {
    waitpid($childpid, 0);
    $SIG{TERM} = 'IGNORE';
    system("kill -TERM -1");
    sleep(1);
    system("kill -KILL -1");
}
else {
    #
    # Otherwise, wait for the command to exit (prevent zombie), but
    # then just wait forever. The only way to die is to be killed
    # from outside the jail via the signal handler above. I suppose
    # we could look at the exit status of the child ...
    # 
    waitpid($childpid, 0);
    while (1) {
	system("/bin/sleep 10000");
    }
}
exit(0);
