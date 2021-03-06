/*
 * Copyright (c) 2005 University of Utah and the Flux Group.
 * 
 * {{{EMULAB-LICENSE
 * 
 * This file is part of the Emulab network testbed software.
 * 
 * This file is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 * 
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this file.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * }}}
 */

/**
 * @file wagthedog.C
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <paths.h>
#include <signal.h>
#include <sys/resource.h>

#include "SGGPIO.h"

#define WDT_STARTUP_DELAY 60

#define WDT_INPUT 58
#define WDT_SET1 59
#define WDT_SET2 60

static int debug = 0;

static volatile int looping = 1;

static char *pidfile = _PATH_VARRUN "wagthedog.pid";

static void usage()
{
    fprintf(stderr,
	    "Usage: wagthedog [-h] [-i pidfile]\n"
	    "\n"
	    "Enable and feed the stargate's onboard watchdog timer."
	    "\n"
	    "Options:\n"
	    "  -h\t\tPrint this message.\n"
	    "  -d\t\tTurn on debugging messages and do not daemonize.\n"
	    "  -i pidfile\tSpecify the path for the PID file. (Default: %s)\n",
	    pidfile);
    exit(1);
}

int main(int argc, char **argv)
{
    int c, wiggle = 1, retval = EXIT_SUCCESS;
    unsigned int elapsed = 0;
    SGGPIO_PORT sggpio;
    
    while ((c = getopt(argc, argv, "dhi")) != -1) {
	switch (c) {
	case 'd':
	    debug += 1;
	    break;
	case 'i':
	    pidfile = optarg;
	    break;
	case 'h':
	case '?':
	default:
	    usage();
	    exit(retval);
	    break;
	}
    }
    
    if (!debug) {
	/* Become a daemon */
	daemon(0, 0);
    }

    signal(SIGTERM, SIG_IGN);

    setpriority(PRIO_PROCESS, 0, 20);
    openlog("wagthedog", LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "wagthedog started");

    if (pidfile) {
	FILE *fp;
	
	if ((fp = fopen(pidfile, "w")) != NULL) {
	    fprintf(fp, "%d\n", getpid());
	    (void) fclose(fp);
	}
    }

    sggpio.setDir(WDT_INPUT, 1);
    sggpio.setDir(WDT_SET1, 1);
    sggpio.setDir(WDT_SET2, 1);

    sggpio.setPin(WDT_INPUT, wiggle);
    sggpio.setPin(WDT_SET1, 0);
    sggpio.setPin(WDT_SET2, 1);
    
    while (looping) {
	wiggle = !wiggle;
	sggpio.setPin(WDT_INPUT, wiggle);
	
	usleep(500);
	
	elapsed += 1;
	if (elapsed == WDT_STARTUP_DELAY)
	    syslog(LOG_INFO, "startup delay reached, watchdog active");
    }
    syslog(LOG_INFO, "wagthedog stopped");
    
    return retval;
}
