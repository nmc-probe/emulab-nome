/* 
 * File:	capture.c
 * Description: 
 * Author:	Leigh Stoller
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	27-Jul-92
 *
 * (c) Copyright 1992, 2000, University of Utah, all rights reserved.
 */

/*
 * Testbed note:  This code has developed over the last several
 * years in RCS.  This is an import of the current version of
 * capture from the /usr/src/utah RCS repository into the testbed,
 * with some new hacks to port it to Linux.
 *
 * - dga, 10/10/2000
 */

/*
 * A LITTLE hack to record output from a tty device to a file, and still
 * have it available to tip using a pty/tty pair.
 */
	
#include <sys/param.h>

#include <stdio.h>
#include <ctype.h>
#include <strings.h>
#include <syslog.h>
#ifdef HPBSD
#include <sgtty.h>
#else
#include <termios.h>
#endif
#include <errno.h>

#include <sys/param.h>
#include <sys/file.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/termios.h>

#define geterr(e)	strerror(e)

void quit(int);
void reinit(int);
void cleanup(void);
void capture();
void usage();

#ifdef __linux__
#define _POSIX_VDISABLE '\0'
#endif

/*
 *  Configurable things.
 */
#define DEVPATH		"/dev"
#ifdef HPBSD
#define LOGPATH		"/usr/adm/tiplogs"
#else
#define LOGPATH		"/var/log/tiplogs"
#endif
#define PIDNAME		"%s/%s.pid"
#define LOGNAME		"%s/%s.log"
#define PTYNAME		"%s/tip/%s-pty"
#define DEVNAME		"%s/%s"

char 	*Progname;
char 	*Pidname;
char	*Logname;
char	*Ptyname;
char	*Devname;
char	*Machine;
int	logfd, devfd, ptyfd;
int	pid;
int	hwflow = 0, speed = B9600;

int
main(argc, argv)
	int argc;
	char **argv;
{
	char strbuf[MAXPATHLEN], *newstr();
	int flags, op, i;
	extern int optind;
	extern char *optarg;

	Progname = (Progname = rindex(argv[0], '/')) ? ++Progname : *argv;

	while ((op = getopt(argc, argv, "s:H")) != EOF)
		switch (op) {

		case 'H':
			++hwflow;
			break;

		case 's':
			if ((i = atoi(optarg)) == 0 ||
			    (speed = val2speed(i)) == 0)
				usage();
			break;
		}

	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	Machine = argv[0];

	(void) sprintf(strbuf, PIDNAME, LOGPATH, argv[0]);
	Pidname = newstr(strbuf);
	(void) sprintf(strbuf, LOGNAME, LOGPATH, argv[0]);
	Logname = newstr(strbuf);
	(void) sprintf(strbuf, PTYNAME, DEVPATH, argv[0]);
	Ptyname = newstr(strbuf);
	(void) sprintf(strbuf, DEVNAME, DEVPATH, argv[1]);
	Devname = newstr(strbuf);

	openlog(Progname, LOG_PID, LOG_USER);
	dolog(LOG_NOTICE, "starting");

	signal(SIGINT, quit);
	signal(SIGTERM, quit);
	signal(SIGHUP, reinit);

	/*
	 * Open up log file, console tty, and controlling pty.
	 */
	if ((logfd = open(Logname, O_WRONLY|O_CREAT|O_APPEND, 0666)) < 0)
		die("open(%s) : %s", Logname, geterr(errno));
	if ((ptyfd = open(Ptyname, O_RDWR, 0666)) < 0)
		die("open(%s) : %s", Ptyname, geterr(errno));
	if ((devfd = open(Devname, O_RDWR|O_NONBLOCK, 0666)) < 0)
		die("open(%s) : %s", Devname, geterr(errno));

	if (ioctl(devfd, TIOCEXCL, 0) < 0)
		fprintf(stderr, "%s: TIOCEXCL %s: %s\n",
			Progname, Devname, geterr(errno));

	writepid();
	rawmode(speed);

	capture();

	cleanup();
	exit(0);
}

#ifdef HPBSD
void
capture()
{
	flags = FNDELAY;
	(void) fcntl(ptyfd, F_SETFL, &flags);

	if (pid = fork())
		in();
	else
		out();
}

/*
 * Loop reading from the console device, writing data to log file and
 * to the pty for tip to pick up.
 */
in()
{
	char buf[NBPG];
	int cc, omask;
	
	while (1) {
		if ((cc = read(devfd, buf, NBPG)) < 0) {
			if ((errno == EWOULDBLOCK) || (errno == EINTR))
				continue;
			else
				die("read(%s) : %s", Devname, geterr(errno));
		}
		omask = sigblock(sigmask(SIGHUP)|sigmask(SIGTERM));

		if (write(logfd, buf, cc) < 0)
			die("write(%s) : %s", Logname, geterr(errno));

		if (write(ptyfd, buf, cc) < 0) {
			if ((errno != EIO) && (errno != EWOULDBLOCK))
				die("write(%s) : %s", Ptyname, geterr(errno));
		}
		(void) sigsetmask(omask);
	}
}

/*
 * Loop reading input from pty (tip), and send off to the console device.
 */
out()
{
	char buf[NBPG];
	int cc, omask;
	struct timeval timeout;

	timeout.tv_sec  = 0;
	timeout.tv_usec = 100000;
	
	while (1) {
		if ((cc = read(ptyfd, buf, NBPG)) < 0) {
			if ((errno == EIO) || (errno == EWOULDBLOCK) ||
			    (errno == EINTR)) {
				select(0, 0, 0, 0, &timeout);
				continue;
			}
			else
				die("read(%s) : %s", Ptyname, geterr(errno));
		}
		omask = sigblock(sigmask(SIGHUP)|sigmask(SIGTERM));

		if (write(devfd, buf, cc) < 0)
			die("write(%s) : %s", Devname, geterr(errno));
		
		(void) sigsetmask(omask);
	}
}
#else
void
capture()
{
	fd_set sfds, fds;
	int n, i, cc, lcc, omask, dchars = 0;
	char buf[4096];
	struct timeval timeout;

	timeout.tv_sec  = 0;
	timeout.tv_usec = 10000;	/* ~10 chars at 9600 baud */

	n = devfd;
	if (devfd < ptyfd)
		n = ptyfd;
	++n;
	FD_ZERO(&sfds);
	FD_SET(devfd, &sfds);
	FD_SET(ptyfd, &sfds);
	for (;;) {
		if (dchars) {
			warn("%d %s chars dropped",
			     dchars < 0 ? -dchars : dchars,
			     dchars < 0 ? "input" : "output");
			dchars = 0;
		}
		fds = sfds;
		i = select(n, &fds, NULL, NULL, NULL);
		if (i < 0)
			err(1, "select");
		if (i == 0) {
			fprintf(stderr, "%s: no fds ready!\n", Progname);
			sleep(1);
			continue;
		}
		if (FD_ISSET(devfd, &fds)) {
			errno = 0;
			cc = read(devfd, buf, sizeof(buf));
			if (cc < 0)
				die("read(%s) : %s", Devname, geterr(errno));
			if (cc == 0)
				die("read(%s) : EOF", Devname);
			errno = 0;

			omask = sigblock(sigmask(SIGHUP)|sigmask(SIGTERM));
			for (lcc = 0; lcc < cc; lcc += i) {
				i = write(ptyfd, &buf[lcc], cc-lcc);
				if (i < 0) {
					/* XXX commonly observed */
					if (errno == EIO || errno == EAGAIN) {
/* Dropped -- tip isn't running.  Log it anyway, and just ignore the error.  */
#if 0
						dchars = -(cc-lcc);
#endif
						goto dropped;
					}
					die("write(%s) : %s",
					    Ptyname, geterr(errno));
				}
				if (i == 0)
					die("write(%s) : zero-length", Ptyname);
			}
dropped:
			i = write(logfd, buf, cc);
			if (i < 0)
				die("write(%s) : %s", Logname, geterr(errno));
			if (i != cc)
				die("write(%s) : incomplete", Logname);
			(void) sigsetmask(omask);

		}
		if (FD_ISSET(ptyfd, &fds)) {
			errno = 0;
			cc = read(ptyfd, buf, sizeof(buf), 0);
			if (cc < 0) {
				/* XXX commonly observed */
				if (errno == EIO || errno == EAGAIN)
					continue;
				die("read(%s) : %s", Ptyname, geterr(errno));
			}
			if (cc == 0) {
				select(0, 0, 0, 0, &timeout);
				continue;
			}
			errno = 0;

			omask = sigblock(sigmask(SIGHUP)|sigmask(SIGTERM));
			for (lcc = 0; lcc < cc; lcc += i) {
				i = write(devfd, &buf[lcc], cc-lcc);
				if (i < 0) {
					/* XXX commonly observed, bad idea? */
					if (errno == EAGAIN) {
						dchars = cc-lcc;
						goto dropped2;
					}
					die("write(%s) : %s",
					    Devname, geterr(errno));
				}
				if (i == 0)
					die("write(%s) : zero-length", Devname);
			}
dropped2:
			(void) sigsetmask(omask);

		}
	}
}
#endif

/*
 * SIGHUP means we want to close the old log file (because it has probably
 * been moved) and start a new version of it.
 */
void
reinit(int sig)
{
	/*
	 * We know that the any pending write to the log file completed
	 * because we blocked SIGHUP during the write.
	 */
	close(logfd);
	
	if ((logfd = open(Logname, O_WRONLY|O_CREAT|O_APPEND, 0666)) < 0)
		die("open(%s) : %s", Logname, geterr(errno));

	dolog(LOG_NOTICE, "restarted");
}

/*
 *  Display proper usage / error message and exit.
 */
void
usage()
{
	fprintf(stderr, "usage: %s machine tty\n", Progname);
	exit(1);
}

warn(format, arg0, arg1, arg2, arg3)
	char *format, *arg0, *arg1, *arg2, *arg3;
{
	char msgbuf[NBPG];

	sprintf(msgbuf, format, arg0, arg1, arg2, arg3);
	dolog(LOG_ERR, msgbuf);
}

die(format, arg0, arg1, arg2, arg3)
	char *format, *arg0, *arg1, *arg2, *arg3;
{
	char msgbuf[NBPG];

	sprintf(msgbuf, format, arg0, arg1, arg2, arg3);
	dolog(LOG_ERR, msgbuf);
	quit(0);
}

dolog(level, msg)
{
	char msgbuf[NBPG];

	sprintf(msgbuf, "%s - %s", Machine, msg);
	syslog(level, msgbuf);
}

void
quit(int sig)
{
	cleanup();
	exit(1);
}

void
cleanup()
{
	if (pid)
		(void) kill(pid, SIGTERM);
	(void) unlink(Pidname);
}

char *
newstr(str)
	char *str;
{
	char *malloc();
	register char *np;

	if ((np = malloc((unsigned) strlen(str) + 1)) == NULL) {
		fprintf(stderr, "%s: malloc: out of memory\n", Progname);
		exit(1);
	}

	return(strcpy(np, str));
}

#if 0
char *
geterr(errindx)
int errindx;
{
	extern int sys_nerr;
	extern char *sys_errlist[];
	char syserr[25];

	if (errindx < sys_nerr)
		return(sys_errlist[errindx]);

	(void) sprintf(syserr, "unknown error (%d)", errindx);
	return(syserr);
}
#endif

/*
 * Open up PID file and write our pid into it.
 */
writepid()
{
	int fd;
	char buf[8];
	
	if ((fd = open(Pidname, O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0)
		die("open(%s) : %s", Pidname, geterr(errno));
	
	(void) sprintf(buf, "%d\n", getpid());
	
	if (write(fd, buf, strlen(buf)) < 0)
		die("write(%s) : %s", Pidname, geterr(errno));
	
	(void) close(fd);
}

/*
 * Put the console line into raw mode.
 */
rawmode(speed)
int speed;
{
#ifdef HPBSD
	struct sgttyb sgbuf;
	int bits;
	
	if (ioctl(devfd, TIOCGETP, (char *)&sgbuf) < 0)
		die("TIOCGETP(%s) : %s", Device, geterr(errno));
	sgbuf.sg_ispeed = sgbuf.sg_ospeed = speed;
	sgbuf.sg_flags = RAW|TANDEM;
	bits = LDECCTQ;
	if (ioctl(devfd, TIOCSETP, (char *)&sgbuf) < 0)
		die("TIOCSETP(%s) : %s", Device, geterr(errno));
	if (ioctl(devfd, TIOCLBIS, (char *)&bits) < 0)
		die("TIOCLBIS(%s) : %s", Device, geterr(errno));
#else
	struct termios t;

	if (tcgetattr(devfd, &t) < 0)
		die("tcgetattr(%s) : %s", Devname, geterr(errno));
	(void) cfsetispeed(&t, speed);
	(void) cfsetospeed(&t, speed);
	cfmakeraw(&t);
	t.c_cflag |= CLOCAL;
	if (hwflow)
#ifdef __linux__
	        t.c_cflag |= CRTSCTS;
#else
		t.c_cflag |= CCTS_OFLOW | CRTS_IFLOW;
#endif
	t.c_cc[VSTART] = t.c_cc[VSTOP] = _POSIX_VDISABLE;
	if (tcsetattr(devfd, TCSAFLUSH, &t) < 0)
		die("tcsetattr(%s) : %s", Devname, geterr(errno));
#endif
}

/*
 * From kgdbtunnel
 */
static struct speeds {
	int speed;		/* symbolic speed */
	int val;		/* numeric value */
} speeds[] = {
#ifdef B50
	{ B50,	50 },
#endif
#ifdef B75
	{ B75,	75 },
#endif
#ifdef B110
	{ B110,	110 },
#endif
#ifdef B134
	{ B134,	134 },
#endif
#ifdef B150
	{ B150,	150 },
#endif
#ifdef B200
	{ B200,	200 },
#endif
#ifdef B300
	{ B300,	300 },
#endif
#ifdef B600
	{ B600,	600 },
#endif
#ifdef B1200
	{ B1200, 1200 },
#endif
#ifdef B1800
	{ B1800, 1800 },
#endif
#ifdef B2400
	{ B2400, 2400 },
#endif
#ifdef B4800
	{ B4800, 4800 },
#endif
#ifdef B7200
	{ B7200, 7200 },
#endif
#ifdef B9600
	{ B9600, 9600 },
#endif
#ifdef B14400
	{ B14400, 14400 },
#endif
#ifdef B19200
	{ B19200, 19200 },
#endif
#ifdef B38400
	{ B38400, 38400 },
#endif
#ifdef B28800
	{ B28800, 28800 },
#endif
#ifdef B57600
	{ B57600, 57600 },
#endif
#ifdef B76800
	{ B76800, 76800 },
#endif
#ifdef B115200
	{ B115200, 115200 },
#endif
#ifdef B230400
	{ B230400, 230400 },
#endif
};
#define NSPEEDS (sizeof(speeds) / sizeof(speeds[0]))

int
val2speed(val)
	register int val;
{
	register int n;
	register struct speeds *sp;

	for (sp = speeds, n = NSPEEDS; n > 0; ++sp, --n)
		if (val == sp->val)
			return (sp->speed);

	return (0);
}
