/*
 * Copyright (c) 1996 Alex Nash, Paul Traina, Poul-Henning Kamp
 * Copyright (c) 1994 Ugen J.S.Antsilevich
 *
 * Idea and grammar partially left from:
 * Copyright (c) 1993 Daniel Boulet
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 * NEW command line interface for IP firewall facility
 *
 */

/*
 * Copyright (C) 1989, 1995, 1998: R B Davies
 *
 * Permission is granted to use or distribute but not to sell
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sysexits.h>
#include <math.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_fw.h>
#include <net/route.h> /* def. of struct route */
#include <sys/param.h>
#include <sys/mbuf.h>
#include <netinet/ip_dummynet.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

int 		lineno = -1;

int 		s;				/* main RAW socket 	   */
int 		do_resolv=0;			/* Would try to resolve all */
int		do_acct=0;			/* Show packet/byte count  */
int		do_time=0;			/* Show time stamps        */
int		do_quiet=0;			/* Be quiet in add and flush  */
int		do_force=0;			/* Don't ask for confirmation */
int		do_pipe=0;                      /* this cmd refers to a pipe */
int		do_sort=0;			/* field to sort results (0=no) */

struct icmpcode {
	int	code;
	char	*str;
};

static struct icmpcode icmpcodes[] = {
      { ICMP_UNREACH_NET,		"net" },
      { ICMP_UNREACH_HOST,		"host" },
      { ICMP_UNREACH_PROTOCOL,		"protocol" },
      { ICMP_UNREACH_PORT,		"port" },
      { ICMP_UNREACH_NEEDFRAG,		"needfrag" },
      { ICMP_UNREACH_SRCFAIL,		"srcfail" },
      { ICMP_UNREACH_NET_UNKNOWN,	"net-unknown" },
      { ICMP_UNREACH_HOST_UNKNOWN,	"host-unknown" },
      { ICMP_UNREACH_ISOLATED,		"isolated" },
      { ICMP_UNREACH_NET_PROHIB,	"net-prohib" },
      { ICMP_UNREACH_HOST_PROHIB,	"host-prohib" },
      { ICMP_UNREACH_TOSNET,		"tosnet" },
      { ICMP_UNREACH_TOSHOST,		"toshost" },
      { ICMP_UNREACH_FILTER_PROHIB,	"filter-prohib" },
      { ICMP_UNREACH_HOST_PRECEDENCE,	"host-precedence" },
      { ICMP_UNREACH_PRECEDENCE_CUTOFF,	"precedence-cutoff" },
      { 0, NULL }
};

static void show_usage(const char *fmt, ...);

static int
mask_bits(struct in_addr m_ad)
{
	int h_fnd=0,h_num=0,i;
	u_long mask;

	mask=ntohl(m_ad.s_addr);
	for (i=0;i<sizeof(u_long)*CHAR_BIT;i++) {
		if (mask & 1L) {
			h_fnd=1;
			h_num++;
		} else {
			if (h_fnd)
				return -1;
		}
		mask=mask>>1;
	}
	return h_num;
}                         

static void
print_port(prot, port, comma)
	u_char  prot;
	u_short port;
	const char *comma;
{
	struct servent *se;
	struct protoent *pe;
	const char *protocol;
	int printed = 0;

	if (!strcmp(comma,":")) {
		printf("%s0x%04x", comma, port);
		return ;
	}
	if (do_resolv) {
		pe = getprotobynumber(prot);
		if (pe)
			protocol = pe->p_name;
		else
			protocol = NULL;

		se = getservbyport(htons(port), protocol);
		if (se) {
			printf("%s%s", comma, se->s_name);
			printed = 1;
		}
	} 
	if (!printed)
		printf("%s%d",comma,port);
}

static void
print_iface(char *key, union ip_fw_if *un, int byname)
{
	char ifnb[FW_IFNLEN+1];

	if (byname) {
		strncpy(ifnb, un->fu_via_if.name, FW_IFNLEN);
		ifnb[FW_IFNLEN]='\0';
		if (un->fu_via_if.unit == -1)
			printf(" %s %s*", key, ifnb);
		else 
			printf(" %s %s%d", key, ifnb, un->fu_via_if.unit);
	} else if (un->fu_via_ip.s_addr != 0) {
		printf(" %s %s", key, inet_ntoa(un->fu_via_ip));
	} else
		printf(" %s any", key);
}

static void
print_reject_code(int code)
{
	struct icmpcode *ic;

	for (ic = icmpcodes; ic->str; ic++)
		if (ic->code == code) {
			printf("%s", ic->str);
			return;
		}
	printf("%u", code);
}

static void
show_ipfw(struct ip_fw *chain, int pcwidth, int bcwidth)
{
	char *comma;
	u_long adrt;
	struct hostent *he;
	struct protoent *pe;
	int i, mb;
	int nsp = IP_FW_GETNSRCP(chain);
	int ndp = IP_FW_GETNDSTP(chain);

	if (do_resolv)
		setservent(1/*stay open*/);

	printf("%05u ", chain->fw_number);

	if (do_acct) 
		printf("%*qu %*qu ",pcwidth,chain->fw_pcnt,bcwidth,chain->fw_bcnt);

	if (do_time)
	{
		if (chain->timestamp)
		{
			char timestr[30];

			strcpy(timestr, ctime((time_t *)&chain->timestamp));
			*strchr(timestr, '\n') = '\0';
			printf("%s ", timestr);
		}
		else
			printf("                         ");
	}
	if (chain->fw_flg == IP_FW_F_CHECK_S) {
		printf("check-state\n");
		goto done ;
	}

	switch (chain->fw_flg & IP_FW_F_COMMAND)
	{
		case IP_FW_F_ACCEPT:
			printf("allow");
			break;
		case IP_FW_F_DENY:
			printf("deny");
			break;
		case IP_FW_F_COUNT:
			printf("count");
			break;
		case IP_FW_F_DIVERT:
			printf("divert %u", chain->fw_divert_port);
			break;
		case IP_FW_F_TEE:
			printf("tee %u", chain->fw_divert_port);
			break;
		case IP_FW_F_SKIPTO:
			printf("skipto %u", chain->fw_skipto_rule);
			break;
                case IP_FW_F_PIPE:
                        printf("pipe %u", chain->fw_skipto_rule);
                        break ;
		case IP_FW_F_REJECT:
			if (chain->fw_reject_code == IP_FW_REJECT_RST)
				printf("reset");
			else {
				printf("unreach ");
				print_reject_code(chain->fw_reject_code);
			}
			break;
		case IP_FW_F_FWD:
			printf("fwd %s", inet_ntoa(chain->fw_fwd_ip.sin_addr));
			if(chain->fw_fwd_ip.sin_port)
				printf(",%d", chain->fw_fwd_ip.sin_port);
			break;
		default:
			errx(EX_OSERR, "impossible");
	}

	if (chain->fw_flg & IP_FW_F_RND_MATCH) {
		double d = 1.0 * (int)(chain->pipe_ptr) ;
		d = 1 - (d / 0x7fffffff) ;
		printf(" prob %f", d);
	}
	if (chain->fw_flg & IP_FW_F_PRN) {
		printf(" log");
		if (chain->fw_logamount)
			printf(" logamount %d", chain->fw_logamount);
	}

	pe = getprotobynumber(chain->fw_prot);
	if (pe)
		printf(" %s", pe->p_name);
	else
		printf(" %u", chain->fw_prot);

	printf(" from %s", chain->fw_flg & IP_FW_F_INVSRC ? "not " : "");

	adrt=ntohl(chain->fw_smsk.s_addr);
	if (adrt==ULONG_MAX && do_resolv) {
		adrt=(chain->fw_src.s_addr);
		he=gethostbyaddr((char *)&adrt,sizeof(u_long),AF_INET);
		if (he==NULL) {
			printf(inet_ntoa(chain->fw_src));
		} else
			printf("%s",he->h_name);
	} else {
		if (adrt!=ULONG_MAX) {
			mb=mask_bits(chain->fw_smsk);
			if (mb == 0) {
				printf("any");
			} else {
				if (mb > 0) {
					printf(inet_ntoa(chain->fw_src));
					printf("/%d",mb);
				} else {
					printf(inet_ntoa(chain->fw_src));
					printf(":");
					printf(inet_ntoa(chain->fw_smsk));
				}
			}
		} else
			printf(inet_ntoa(chain->fw_src));
	}

	if (chain->fw_prot == IPPROTO_TCP || chain->fw_prot == IPPROTO_UDP) {
		comma = " ";
		for (i = 0; i < nsp; i++) {
			print_port(chain->fw_prot, chain->fw_uar.fw_pts[i], comma);
			if (i==0 && (chain->fw_flg & IP_FW_F_SRNG))
				comma = "-";
			else if (i==0 && (chain->fw_flg & IP_FW_F_SMSK))
				comma = ":";
			else
				comma = ",";
		}
	}

	printf(" to %s", chain->fw_flg & IP_FW_F_INVDST ? "not " : "");

	adrt=ntohl(chain->fw_dmsk.s_addr);
	if (adrt==ULONG_MAX && do_resolv) {
		adrt=(chain->fw_dst.s_addr);
		he=gethostbyaddr((char *)&adrt,sizeof(u_long),AF_INET);
		if (he==NULL) {
			printf(inet_ntoa(chain->fw_dst));
		} else
			printf("%s",he->h_name);
	} else {
		if (adrt!=ULONG_MAX) {
			mb=mask_bits(chain->fw_dmsk);
			if (mb == 0) {
				printf("any");
			} else {
				if (mb > 0) {
					printf(inet_ntoa(chain->fw_dst));
					printf("/%d",mb);
				} else {
					printf(inet_ntoa(chain->fw_dst));
					printf(":");
					printf(inet_ntoa(chain->fw_dmsk));
				}
			}
		} else
			printf(inet_ntoa(chain->fw_dst));
	}

	if (chain->fw_prot == IPPROTO_TCP || chain->fw_prot == IPPROTO_UDP) {
		comma = " ";
		for (i = 0; i < ndp; i++) {
			print_port(chain->fw_prot, chain->fw_uar.fw_pts[nsp+i], comma);
			if (i==0 && (chain->fw_flg & IP_FW_F_DRNG))
				comma = "-";
			else if (i==0 && (chain->fw_flg & IP_FW_F_DMSK))
				comma = ":";
			else
				comma = ",";
		}
	}

	if (chain->fw_flg & IP_FW_F_UID) {
		struct passwd *pwd = getpwuid(chain->fw_uid);

		if (pwd)
			printf(" uid %s", pwd->pw_name);
		else
			printf(" uid %u", chain->fw_uid);
	}

	if (chain->fw_flg & IP_FW_F_GID) {
		struct group *grp = getgrgid(chain->fw_gid);

		if (grp)
			printf(" gid %s", grp->gr_name);
		else
			printf(" gid %u", chain->fw_gid);
	}

	if (chain->fw_flg & IP_FW_F_KEEP_S) {
		if (chain->next_rule_ptr)
		    printf(" keep-state %d", (int)chain->next_rule_ptr);
		else
		    printf(" keep-state");
	}
	/* Direction */
	if (chain->fw_flg & IP_FW_BRIDGED)
		printf(" bridged");
	if ((chain->fw_flg & IP_FW_F_IN) && !(chain->fw_flg & IP_FW_F_OUT))
		printf(" in");
	if (!(chain->fw_flg & IP_FW_F_IN) && (chain->fw_flg & IP_FW_F_OUT))
		printf(" out");

	/* Handle hack for "via" backwards compatibility */
	if ((chain->fw_flg & IF_FW_F_VIAHACK) == IF_FW_F_VIAHACK) {
		print_iface("via",
		    &chain->fw_in_if, chain->fw_flg & IP_FW_F_IIFNAME);
	} else {
		/* Receive interface specified */
		if (chain->fw_flg & IP_FW_F_IIFACE)
			print_iface("recv", &chain->fw_in_if,
			    chain->fw_flg & IP_FW_F_IIFNAME);
		/* Transmit interface specified */
		if (chain->fw_flg & IP_FW_F_OIFACE)
			print_iface("xmit", &chain->fw_out_if,
			    chain->fw_flg & IP_FW_F_OIFNAME);
	}

	if (chain->fw_flg & IP_FW_F_FRAG)
		printf(" frag");

	if (chain->fw_ipopt || chain->fw_ipnopt) {
		int 	_opt_printed = 0;
#define PRINTOPT(x)	{if (_opt_printed) printf(",");\
			printf(x); _opt_printed = 1;}

		printf(" ipopt ");
		if (chain->fw_ipopt  & IP_FW_IPOPT_SSRR) PRINTOPT("ssrr");
		if (chain->fw_ipnopt & IP_FW_IPOPT_SSRR) PRINTOPT("!ssrr");
		if (chain->fw_ipopt  & IP_FW_IPOPT_LSRR) PRINTOPT("lsrr");
		if (chain->fw_ipnopt & IP_FW_IPOPT_LSRR) PRINTOPT("!lsrr");
		if (chain->fw_ipopt  & IP_FW_IPOPT_RR)   PRINTOPT("rr");
		if (chain->fw_ipnopt & IP_FW_IPOPT_RR)   PRINTOPT("!rr");
		if (chain->fw_ipopt  & IP_FW_IPOPT_TS)   PRINTOPT("ts");
		if (chain->fw_ipnopt & IP_FW_IPOPT_TS)   PRINTOPT("!ts");
	} 

	if (chain->fw_tcpf & IP_FW_TCPF_ESTAB) 
		printf(" established");
	else if (chain->fw_tcpf == IP_FW_TCPF_SYN &&
	    chain->fw_tcpnf == IP_FW_TCPF_ACK)
		printf(" setup");
	else if (chain->fw_tcpf || chain->fw_tcpnf) {
		int 	_flg_printed = 0;
#define PRINTFLG(x)	{if (_flg_printed) printf(",");\
			printf(x); _flg_printed = 1;}

		printf(" tcpflg ");
		if (chain->fw_tcpf  & IP_FW_TCPF_FIN)  PRINTFLG("fin");
		if (chain->fw_tcpnf & IP_FW_TCPF_FIN)  PRINTFLG("!fin");
		if (chain->fw_tcpf  & IP_FW_TCPF_SYN)  PRINTFLG("syn");
		if (chain->fw_tcpnf & IP_FW_TCPF_SYN)  PRINTFLG("!syn");
		if (chain->fw_tcpf  & IP_FW_TCPF_RST)  PRINTFLG("rst");
		if (chain->fw_tcpnf & IP_FW_TCPF_RST)  PRINTFLG("!rst");
		if (chain->fw_tcpf  & IP_FW_TCPF_PSH)  PRINTFLG("psh");
		if (chain->fw_tcpnf & IP_FW_TCPF_PSH)  PRINTFLG("!psh");
		if (chain->fw_tcpf  & IP_FW_TCPF_ACK)  PRINTFLG("ack");
		if (chain->fw_tcpnf & IP_FW_TCPF_ACK)  PRINTFLG("!ack");
		if (chain->fw_tcpf  & IP_FW_TCPF_URG)  PRINTFLG("urg");
		if (chain->fw_tcpnf & IP_FW_TCPF_URG)  PRINTFLG("!urg");
	} 
	if (chain->fw_flg & IP_FW_F_ICMPBIT) {
		int type_index;
		int first = 1;

		printf(" icmptype");

		for (type_index = 0; type_index < IP_FW_ICMPTYPES_DIM * sizeof(unsigned) * 8; ++type_index)
			if (chain->fw_uar.fw_icmptypes[type_index / (sizeof(unsigned) * 8)] & 
				(1U << (type_index % (sizeof(unsigned) * 8)))) {
				printf("%c%d", first == 1 ? ' ' : ',', type_index);
				first = 0;
			}
	}
	printf("\n");
done:
	if (do_resolv)
		endservent();
}

int
sort_q(const void *pa, const void *pb)
{
    int rev = (do_sort < 0) ;
    int field = rev ? -do_sort : do_sort ;
    long long res=0 ;
    const struct dn_flow_queue *a = pa ;
    const struct dn_flow_queue *b = pb ;

    switch (field) {
    case 1: /* pkts */
	res = a->len - b->len ;
	break ;
    case 2 : /* bytes */
	res = a->len_bytes - b->len_bytes ;
	break ;

    case 3 : /* tot pkts */
	res = a->tot_pkts - b->tot_pkts ;
	break ;

    case 4 : /* tot bytes */
	res = a->tot_bytes - b->tot_bytes ;
	break ;
    }
    if (res < 0) res = -1 ;
    if (res > 0) res = 1 ;
    return (int)(rev ? res : -res) ;
}

static void
list(ac, av)
	int	ac;
	char 	**av;
{
	struct ip_fw *rules;
	struct dn_pipe *pipes;
	void *data = NULL;
	int pcwidth = 0;
	int bcwidth = 0;
	int n, num = 0;
	int nbytes;

	/* get rules or pipes from kernel, resizing array as necessary */
	{
		const int unit = do_pipe ? sizeof(*pipes) : sizeof(*rules);
		const int ocmd = do_pipe ? IP_DUMMYNET_GET : IP_FW_GET;
		int nalloc = 0;

		while (num >= nalloc) {
			nalloc = nalloc * 2 + 200;
			nbytes = nalloc * unit;
			if ((data = realloc(data, nbytes)) == NULL)
				err(EX_OSERR, "realloc");
			if (getsockopt(s, IPPROTO_IP, ocmd, data, &nbytes) < 0)
				err(EX_OSERR, "getsockopt(IP_%s_GET)",
				    do_pipe ? "DUMMYNET" : "FW");
			num = nbytes / unit;
		}
	}

	/* display requested pipes */
	if (do_pipe) {
	    u_long rulenum;
	    void *next_pipe ;
	    struct dn_pipe *p = (struct dn_pipe *) data;

	    if (ac > 0)
		rulenum = strtoul(*av++, NULL, 10);
	    else
		rulenum = 0 ;
	    for ( ; nbytes > 0 ; p = (struct dn_pipe *)next_pipe ) {
		char qs[30] ;
		int l ;
		struct dn_flow_queue *q ;

		l = sizeof(*p) + p->rq_elements * sizeof(struct dn_flow_queue) ;
		next_pipe = (void *)p  + l ;
		q = (struct dn_flow_queue *)(p+1) ;
		nbytes -= l ;

		if (rulenum != 0 && rulenum != p->pipe_nr)
			continue;

		printf("%05d: ",p->pipe_nr);

		printf("\tbandwidth dist:\t");
		if (!p->bw.dist)
		    printf("none (unlimited bw)");
		else
		if (p->bw.dist & DN_DIST_CONST_RATE)
		    printf("constant, %4d ",p->bw.bandwidth);
		else
		if (p->bw.dist & DN_DIST_UNIFORM)
		    printf("uniform, var %4d mean %4d ",
			p->bw.variance,p->bw.mean);
		else
		if (p->bw.dist & DN_DIST_POISSON)
		    printf("Poisson, mean %4d ",p->bw.mean);
		else
		if (p->bw.dist & DN_DIST_TABLE_RANDOM)
		    printf("random (PRINT ENTRIES)");
		else
		if (p->bw.dist & DN_DIST_TABLE_DETERM)
		     printf("deterministic (PRINT ENTRIES)");

		printf("\n\tdelay dist:\t");
		if (!p->delay.dist)
		    printf("none");
		else
		if (p->delay.dist & DN_DIST_CONST_TIME)
		    printf("constant, %4d ms",p->delay.delay);
		else
		if (p->delay.dist & DN_DIST_UNIFORM)
		    printf("uniform, var %4d mean %4d ",
			p->delay.variance,p->delay.mean);
		else
		if (p->delay.dist & DN_DIST_POISSON)
		    printf("Poisson, mean %4d ",p->delay.mean);
		else
		if (p->delay.dist & DN_DIST_TABLE_RANDOM)
		    printf("random (PRINT ENTRIES)");
		else
		if (p->delay.dist & DN_DIST_TABLE_DETERM)
		     printf("deterministic (PRINT ENTRIES)");

		printf("\n\tloss dist:\t");
		if (!p->loss.dist)
		    printf("none");
		else
		if (p->loss.dist & DN_DIST_CONST_TIME)
		    printf("constant time, %4d ",p->loss.mean);
		else
		if (p->loss.dist & DN_DIST_CONST_RATE)
		    printf("constant rate, %4d ",p->loss.plr);
		else
		if (p->loss.dist & DN_DIST_UNIFORM)
		    printf("uniform, var %4d mean %4d ",
			p->loss.variance,p->loss.mean);
		else
		if (p->loss.dist & DN_DIST_POISSON)
		    printf("Poisson, mean %4d ",p->loss.mean);
		else
		if (p->loss.dist & DN_DIST_TABLE_RANDOM)
		    printf("random (PRINT ENTRIES)");
		else
		if (p->loss.dist & DN_DIST_TABLE_DETERM)
		    printf("deterministic (PRINT ENTRIES)");
		putchar('\n');

		if ( (l = p->queue_size_bytes) != 0 ) {
		    if (l >= 8192)
			sprintf(qs,"%d KB", l / 1024);
		    else
			sprintf(qs,"%d B", l);
		} else
		    sprintf(qs,"%3d sl.", p->queue_size);

		printf(" %s %d queues (%d buckets)\n",
		    qs, p->rq_elements, p->rq_size);
		printf("    mask: 0x%02x 0x%08x/0x%04x -> 0x%08x/0x%04x\n",
		    p->flow_mask.proto,
		    p->flow_mask.src_ip, p->flow_mask.src_port,
		    p->flow_mask.dst_ip, p->flow_mask.dst_port);
		printf("BKT Prot ___Source IP/port____ "
		       "____Dest. IP/port____ Tot_pkt/bytes Pkt/Byte Drop\n");
		if (do_sort != 0)
		    heapsort(q, p->rq_elements, sizeof( *q), sort_q);
		for (l = 0 ; l < p->rq_elements ; l++) {
		    struct in_addr ina ;
		    struct protoent *pe ;

		    ina.s_addr = htonl(q[l].id.src_ip) ;
		    printf("%3d ", q[l].hash_slot);
		    pe = getprotobynumber(q[l].id.proto);
		    if (pe)
			printf("%-4s ", pe->p_name);
		    else
			printf("%4u ", q[l].id.proto);
		    printf("%15s/%-5d ",
			inet_ntoa(ina), q[l].id.src_port);
		    ina.s_addr = htonl(q[l].id.dst_ip) ;
		    printf("%15s/%-5d ",
			inet_ntoa(ina), q[l].id.dst_port);
		    printf("%4qu %8qu %2u %4u %3u\n",
			q[l].tot_pkts, q[l].tot_bytes,
			q[l].len, q[l].len_bytes, q[l].drops);
		}
	    }
	    free(data);
	    return;
	}
	rules = (struct ip_fw *) data;
	/* determine num more accurately */
	num = 0;
	while (rules[num].fw_number < 65535)
	    num++ ;
	num++ ; /* counting starts from 0 ... */
	/* if showing stats, figure out column widths ahead of time */
	if (do_acct) {
		for (n = 0; n < num; n++) {
			struct ip_fw *const r = &rules[n];
			char temp[32];
			int width;

			/* packet counter */
			width = sprintf(temp, "%qu", r->fw_pcnt);
			if (width > pcwidth)
				pcwidth = width;

			/* byte counter */
			width = sprintf(temp, "%qu", r->fw_bcnt);
			if (width > bcwidth)
				bcwidth = width;
		}
	}
	if (ac == 0) {
		/* display all rules */
		for (n = 0; n < num; n++) {
			struct ip_fw *const r = &rules[n];

			show_ipfw(r, pcwidth, bcwidth);
		}
	} else {
		/* display specific rules requested on command line */
		int exitval = EX_OK;

		while (ac--) {
			u_long rnum;
			char *endptr;
			int seen;

			/* convert command line rule # */
			rnum = strtoul(*av++, &endptr, 10);
			if (*endptr) {
				exitval = EX_USAGE;
				warnx("invalid rule number: %s", *(av - 1));
				continue;
			}
			for (seen = n = 0; n < num; n++) {
				struct ip_fw *const r = &rules[n];

				if (r->fw_number > rnum)
					break;
				if (r->fw_number == rnum) {
					show_ipfw(r, pcwidth, bcwidth);
					seen = 1;
				}
			}
			if (!seen) {
				/* give precedence to other error(s) */
				if (exitval == EX_OK)
					exitval = EX_UNAVAILABLE;
				warnx("rule %lu does not exist", rnum);
			}
		}
		if (exitval != EX_OK)
			exit(exitval);
	}
	/*
	 * show dynamic rules
	 */
	if (num * sizeof (rules[0]) != nbytes ) {
	    struct ipfw_dyn_rule *d =
		    (struct ipfw_dyn_rule *)&rules[num] ;
	    struct in_addr a ;
	    struct protoent *pe;

	    printf("## Dynamic rules:\n");
	    for (;; d++) {
		printf("%05d %qu %qu (T %d, # %d) ty %d",
		    (int)(d->chain),
		    d->pcnt, d->bcnt,
		    d->expire,
		    d->bucket,
		    d->type);
		pe = getprotobynumber(d->id.proto);
		if (pe)
		    printf(" %s,", pe->p_name);
		else
		    printf(" %u,", d->id.proto);
		a.s_addr = htonl(d->id.src_ip);
		printf(" %s", inet_ntoa(a));
		printf(" %d", d->id.src_port);
		switch (d->type) {
		default: /* bidir, no mask */
		    printf(" <->");
		    break ;
		}
		a.s_addr = htonl(d->id.dst_ip);
		printf(" %s", inet_ntoa(a));
		printf(" %d", d->id.dst_port);
		printf("\n");
		if (d->next == NULL)
		    break ;
	    }
	}

	free(data);
}

static void
show_usage(const char *fmt, ...)
{
	if (fmt) {
		char buf[100];
		va_list args;

		va_start(args, fmt);
		vsnprintf(buf, sizeof(buf), fmt, args);
		va_end(args);
		warnx("error: %s", buf);
	}
	fprintf(stderr, "usage: ipfw [options]\n"
"    [pipe] flush\n"
"    add [number] rule\n"
"    [pipe] delete number ...\n"
"    [pipe] list [number ...]\n"
"    [pipe] show [number ...]\n"
"    zero [number ...]\n"
"    resetlog [number ...]\n"
"    pipe number config [pipeconfig]\n"
"  rule: [prob <match_probability>] action proto src dst extras...\n"
"    action:\n"
"      {allow|permit|accept|pass|deny|drop|reject|unreach code|\n"
"       reset|count|skipto num|divert port|tee port|fwd ip|\n"
"       pipe num} [log [logamount count]]\n"
"    proto: {ip|tcp|udp|icmp|<number>}\n"
"    src: from [not] {any|ip[{/bits|:mask}]} [{port|port-port},[port],...]\n"
"    dst: to [not] {any|ip[{/bits|:mask}]} [{port|port-port},[port],...]\n"
"  extras:\n"
"    uid {user id}\n"
"    gid {group id}\n"
"    fragment     (may not be used with ports or tcpflags)\n"
"    in\n"
"    out\n"
"    {xmit|recv|via} {iface|ip|any}\n"
"    {established|setup}\n"
"    tcpflags [!]{syn|fin|rst|ack|psh|urg},...\n"
"    ipoptions [!]{ssrr|lsrr|rr|ts},...\n"
"    icmptypes {type[,type]}...\n"
"  pipeconfig:\n"
"    [delaydist constant] delay <milliseconds>\n"
"    delaydist uniform delaymean <ms> delayvar <ms>\n"
"    delaydist random delayentries <entry_count> <entry> <entry> <entry> ...\n"
"    delaydist determ delayentries <entry_count> <entry> <entry> <entry> ...\n"
"    delaydist poisson delaymean <ms>\n"
"    [lossdist constrate] plr <decimal>\n"
"    lossdist consttime lossmean <ms>\n"
"    lossdist uniform lossmean <decimal> lossvar <decimal>\n"
"    lossdist poisson lossmean <decimal>\n"
"    lossdist random lossentries <entry_count> <entry> <entry> <entry> ...\n"
"    lossdist determ lossentries <entry_count> <entry> <entry> <entry> ...\n"
"    [bwdist constant] {bw|bandwidth} <number>{bit/s|Kbit/s|Mbit/s|Bytes/s|KBytes/s|MBytes/s}\n"
"    bwdist uniform bwmean <value> bwvar <value>\n"
"    bwdist poisson bwmean <value>\n"
"    bwdist random bwentries <entry_count> <entry> <entry> <entry> ...\n"
"    bwdist determ bwentries <entry_count> <entry> <entry> <entry> ...\n"
"    queue <size>{packets|Bytes|KBytes}\n"
"    mask {all| [dst-ip|src-ip|dst-port|src-port|proto] <number>}\n"
"    buckets <number>}\n"
);

	exit(EX_USAGE);
}

static int
lookup_host (host, ipaddr)
	char *host;
	struct in_addr *ipaddr;
{
	struct hostent *he;

	if (!inet_aton(host, ipaddr)) {
		if ((he = gethostbyname(host)) == NULL)
			return(-1);
		*ipaddr = *(struct in_addr *)he->h_addr_list[0];
	}
	return(0);
}

static void
fill_ip(ipno, mask, acp, avp)
	struct in_addr *ipno, *mask;
	int *acp;
	char ***avp;
{
	int ac = *acp;
	char **av = *avp;
	char *p = 0, md = 0;

	if (ac && !strncmp(*av,"any",strlen(*av))) {
		ipno->s_addr = mask->s_addr = 0; av++; ac--;
	} else {
		p = strchr(*av, '/');
		if (!p) 
			p = strchr(*av, ':');
		if (p) {
			md = *p;
			*p++ = '\0'; 
		}

		if (lookup_host(*av, ipno) != 0)
			show_usage("hostname ``%s'' unknown", *av);
		switch (md) {
			case ':':
				if (!inet_aton(p,mask))
					show_usage("bad netmask ``%s''", p);
				break;
			case '/':
				if (atoi(p) == 0) {
					mask->s_addr = 0;
				} else if (atoi(p) > 32) {
					show_usage("bad width ``%s''", p);
				} else {
					mask->s_addr =
					    htonl(~0 << (32 - atoi(p)));
				}
				break;
			default:
				mask->s_addr = htonl(~0);
				break;
		}
		ipno->s_addr &= mask->s_addr;
		av++;
		ac--;
	}
	*acp = ac;
	*avp = av;
}

static void
fill_reject_code(u_short *codep, char *str)
{
	struct icmpcode *ic;
	u_long val;
	char *s;

	val = strtoul(str, &s, 0);
	if (s != str && *s == '\0' && val < 0x100) {
		*codep = val;
		return;
	}
	for (ic = icmpcodes; ic->str; ic++)
		if (!strcasecmp(str, ic->str)) {
			*codep = ic->code;
			return;
		}
	show_usage("unknown ICMP unreachable code ``%s''", str);
}

static void
add_port(cnt, ptr, off, port)
	u_short *cnt, *ptr, off, port;
{
	if (off + *cnt >= IP_FW_MAX_PORTS)
		errx(EX_USAGE, "too many ports (max is %d)", IP_FW_MAX_PORTS);
	ptr[off+*cnt] = port;
	(*cnt)++;
}

static int
lookup_port(const char *arg, int test, int nodash)
{
	int		val;
	char		*earg, buf[32];
	struct servent	*s;
	char		*p, *q;

	snprintf(buf, sizeof(buf), "%s", arg);

	for (p = q = buf; *p; *q++ = *p++) {
		if (*p == '\\') {
			if (*(p+1))
				p++;
		} else {
			if (*p == ',' || (nodash && *p == '-'))
				break;
		}
	}
	*q = '\0';

	val = (int) strtoul(buf, &earg, 0);
	if (!*buf || *earg) {
		setservent(1);
		if ((s = getservbyname(buf, NULL))) {
			val = htons(s->s_port);
		} else {
			if (!test) {
				errx(EX_DATAERR, "unknown port ``%s''", buf);
			}
			val = -1;
		}
	} else {
		if (val < 0 || val > 0xffff) {
			if (!test) {
				errx(EX_DATAERR, "port ``%s'' out of range", buf);
			}
			val = -1;
		}
	}
	return(val);
}

/*
 * return: 0 normally, 1 if first pair is a range,
 * 2 if first pair is a port+mask
 */
static int
fill_port(cnt, ptr, off, arg)
	u_short *cnt, *ptr, off;
	char *arg;
{
	char *s;
	int initial_range = 0;

	for (s = arg; *s && *s != ',' && *s != '-' && *s != ':'; s++) {
		if (*s == '\\' && *(s+1))
			s++;
	}
	if (*s == ':') {
		*s++ = '\0';
		if (strchr(arg, ','))
			errx(EX_USAGE, "port/mask must be first in list");
		add_port(cnt, ptr, off, *arg ? lookup_port(arg, 0, 0) : 0x0000);
		arg = s;
		s = strchr(arg,',');
		if (s)
			*s++ = '\0';
		add_port(cnt, ptr, off, *arg ? lookup_port(arg, 0, 0) : 0xffff);
		arg = s;
		initial_range = 2;
	} else
	if (*s == '-') {
		*s++ = '\0';
		if (strchr(arg, ','))
			errx(EX_USAGE, "port range must be first in list");
		add_port(cnt, ptr, off, *arg ? lookup_port(arg, 0, 0) : 0x0000);
		arg = s;
		s = strchr(arg,',');
		if (s)
			*s++ = '\0';
		add_port(cnt, ptr, off, *arg ? lookup_port(arg, 0, 0) : 0xffff);
		arg = s;
		initial_range = 1;
	}
	while (arg != NULL) {
		s = strchr(arg,',');
		if (s)
			*s++ = '\0';
		add_port(cnt, ptr, off, lookup_port(arg, 0, 0));
		arg = s;
	}
	return initial_range;
}

static void
fill_tcpflag(set, reset, vp)
	u_char *set, *reset;
	char **vp;
{
	char *p = *vp,*q;
	u_char *d;

	while (p && *p) {
		struct tpcflags {
			char * name;
			u_char value;
		} flags[] = {
			{ "syn", IP_FW_TCPF_SYN },
			{ "fin", IP_FW_TCPF_FIN },
			{ "ack", IP_FW_TCPF_ACK },
			{ "psh", IP_FW_TCPF_PSH },
			{ "rst", IP_FW_TCPF_RST },
			{ "urg", IP_FW_TCPF_URG }
		};
		int i;

		if (*p == '!') {
			p++;
			d = reset;
		} else {
			d = set;
		}
		q = strchr(p, ',');
		if (q) 
			*q++ = '\0';
		for (i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i)
			if (!strncmp(p, flags[i].name, strlen(p))) {
				*d |= flags[i].value;
				break;
			}
		if (i == sizeof(flags) / sizeof(flags[0]))
			show_usage("invalid tcp flag ``%s''", p);
		p = q;
	}
}

static void
fill_ipopt(u_char *set, u_char *reset, char **vp)
{
	char *p = *vp,*q;
	u_char *d;

	while (p && *p) {
		if (*p == '!') {
			p++;
			d = reset;
		} else {
			d = set;
		}
		q = strchr(p, ',');
		if (q) 
			*q++ = '\0';
		if (!strncmp(p,"ssrr",strlen(p))) *d |= IP_FW_IPOPT_SSRR;
		if (!strncmp(p,"lsrr",strlen(p))) *d |= IP_FW_IPOPT_LSRR;
		if (!strncmp(p,"rr",strlen(p)))   *d |= IP_FW_IPOPT_RR;
		if (!strncmp(p,"ts",strlen(p)))   *d |= IP_FW_IPOPT_TS;
		p = q;
	}
}

static void
fill_icmptypes(types, vp, fw_flg)
	u_long *types;
	char **vp;
	u_int *fw_flg;
{
	char *c = *vp;

	while (*c)
	{
		unsigned long icmptype;

		if ( *c == ',' )
			++c;

		icmptype = strtoul(c, &c, 0);

		if ( *c != ',' && *c != '\0' )
			show_usage("invalid ICMP type");

		if (icmptype >= IP_FW_ICMPTYPES_DIM * sizeof(unsigned) * 8)
			show_usage("ICMP type out of range");

		types[icmptype / (sizeof(unsigned) * 8)] |= 
			1 << (icmptype % (sizeof(unsigned) * 8));
		*fw_flg |= IP_FW_F_ICMPBIT;
	}
}

static void
delete(ac,av)
	int ac;
	char **av;
{
	struct ip_fw rule;
	struct dn_pipe pipe;
	int i;
	int exitval = EX_OK;

	memset(&rule, 0, sizeof rule);
	memset(&pipe, 0, sizeof pipe);

	av++; ac--;

	/* Rule number */
	while (ac && isdigit(**av)) {
            if (do_pipe) {
                pipe.pipe_nr = atoi(*av); av++; ac--;
                i = setsockopt(s, IPPROTO_IP, IP_DUMMYNET_DEL,
                    &pipe, sizeof pipe);
                if (i) {
                    exitval = 1;
                    warn("rule %u: setsockopt(%s)", pipe.pipe_nr, "IP_DUMMYNET_DEL");
                }
            } else {
		rule.fw_number = atoi(*av); av++; ac--;
		i = setsockopt(s, IPPROTO_IP, IP_FW_DEL, &rule, sizeof rule);
		if (i) {
			exitval = EX_UNAVAILABLE;
			warn("rule %u: setsockopt(%s)", rule.fw_number, "IP_FW_DEL");
		}
	}
	}
	if (exitval != EX_OK)
		exit(exitval);
}

static void
verify_interface(union ip_fw_if *ifu)
{
	struct ifreq ifr;

	/*
	 *	If a unit was specified, check for that exact interface.
	 *	If a wildcard was specified, check for unit 0.
	 */
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s%d", 
			 ifu->fu_via_if.name,
			 ifu->fu_via_if.unit == -1 ? 0 : ifu->fu_via_if.unit);

	if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0)
		warnx("warning: interface ``%s'' does not exist", ifr.ifr_name);
}

static void
fill_iface(char *which, union ip_fw_if *ifu, int *byname, int ac, char *arg)
{
	if (!ac)
	    show_usage("missing argument for ``%s''", which);

	/* Parse the interface or address */
	if (!strcmp(arg, "any")) {
		ifu->fu_via_ip.s_addr = 0;
		*byname = 0;
	} else if (!isdigit(*arg)) {
		char *q;

		*byname = 1;
		strncpy(ifu->fu_via_if.name, arg, sizeof(ifu->fu_via_if.name));
		ifu->fu_via_if.name[sizeof(ifu->fu_via_if.name) - 1] = '\0';
		for (q = ifu->fu_via_if.name;
		    *q && !isdigit(*q) && *q != '*'; q++)
			continue;
		ifu->fu_via_if.unit = (*q == '*') ? -1 : atoi(q);
		*q = '\0';
		verify_interface(ifu);
	} else if (!inet_aton(arg, &ifu->fu_via_ip)) {
		show_usage("bad ip address ``%s''", arg);
	} else
		*byname = 0;
}


static double
ln_gamma(double xx)
{
   /* log gamma function adapted from numerical recipes in C */

   if (xx<1.0)                           /* Use reflection formula */
   {
      double piz = 3.14159265359 * (1.0-xx);
      return log(piz/sin(piz))-ln_gamma(2.0-xx);
   }
   else
   {
      static double cof[6]={76.18009173,-86.50532033,24.01409822,
         -1.231739516,0.120858003e-2,-0.536382e-5};
      double x,tmp,ser;
      int j;

      x=xx-1.0; tmp=x+5.5;
      tmp -= (x+0.5)*log(tmp);  ser=1.0;
      for (j=0;j<=5;j++) { x += 1.0; ser += cof[j]/x; }
      return -tmp+log(2.50662827465*ser);
   }
}

/*
 * build_poisson()
 * given a mean, build a table for poisson distribution.
 *
 * remember calculus long, long ago, estimating the area under the function by 
 * calculating f(1) + f(2) + f(3) + ... ? Well, that is sort of what we are
 * doing here. if f(1)=1, we put one 1 in the table. if f(7)=3, we put three 
 * 7's in the table.  When the table is complete, one can randomly choose
 * values from this table, with the results following a poisson distribution.
 */
static int *
build_poisson(int mean, int *entries)
{
    int p, x;
    int *table;
    double probability;

    if (mean <= 0)
	show_usage("invalid mean %d\n",mean);

    x = *entries = 0;

    if ( ! ( table = malloc(sizeof(int)*0x8200)))
        err(1,"poisson table ate all of the memory\n");

    do {
        /* P(x) =  e^-mean * mean^x
         *         ----------------
         *              x!
         *
         * we will have approximately 0x8000 entries in our table.
         * there is no deep meaning to 0x8000. arbitrary.
         *
         */
        probability = log(mean) * x - mean - ln_gamma(1.0 + x);
        probability = (probability < -40.0) ? 0.0 : exp(probability);
        probability *= 0x8000;
        for(p= (int)probability ;p;p--) {
	    table[(*entries)++]=x;
	    /* since we truncate when casting to int, I don't even think
	     * we need any more than 0x8000 entries. However, I do not
	     * feel confident enough to chance it.
	     */
	    if (*entries == 0x8200)
	        return table;
        }
        x++;
    } while(x < mean || probability >= 1);
    return table;
}

/*
 * read a parameter and its value from cmdline
 */
static int
readint(char *label, char **argv)
{
    if (strcmp(argv[0],label))
	show_usage("%s found where %s expected",argv[0],label);
    return atoi(argv[1]);
}

/*
 * read a parameter and its value from cmdline
 * luckily, we get to parse units on bw specifications. fun.
 */
static int
readbw_rate(char *label, char **argv)
{
    int i;
    char *end;
    if (strcmp(argv[0],label))
	show_usage("%s found where %s expected",argv[0],label);
    i = strtoul(argv[1], &end, 0);
    if (*end == 'K' || *end == 'k' )
	end++, i *= 1000 ;
    else if (*end == 'M')
	end++, i *= 1000000 ;
    if ( *end == 'B' || !strncmp(end, "by", 2) )
	i *= 8 ;
    return i;
}

/*
 * read a parameter and its value from cmdline
 * the string is a real number 0..1, which we convert to an
 * int 0..7fffffff
 */
static int
readlossrate(char *label, char **argv)
{
    double d;
    if (strcmp(argv[0],label))
	show_usage("%s found where %s expected",argv[0],label);
    d = strtod(argv[1], NULL);
    if (d>1 || d<0)
	show_usage("loss rate %s makes no sense",argv[1]);
    return ((int)(d*0x7fffffff));
}

/*
 * read table from cmdline
 * argv == {"entries","<num_of_ents>","<ent0>","<ent1>"...}
 */
static int
readtable(char **argv, int *ac, int **tabptr)
{
    int entries, i;
    int *table; /* just for syntactic convenience */
    if (strcmp(argv[0],"entries"))
	show_usage("%s found where 'entries' expected",argv[0]);
    entries = strtoul(argv[1], NULL, 0);
    if (entries <= 0)
	show_usage("%d entries in your table?", argv[1]);
    if ((*ac -= entries) < 0)
	show_usage("fewer entries than claimed ");

    table = *tabptr = malloc(entries*sizeof(int));
    for (i=0; i<entries; i++)
	table[i] = strtol(argv[i+2], NULL, 0);
    return entries;
}

/*
 * read bw rate table
 * ugh, parsing those dumb unit things every time
 */
static int
readbwratetable(char **argv, int *ac, int **tabptr)
{
    int entries, i;
    int *table;
    char *end;
    if (strcmp(argv[0],"entries"))
	show_usage("%s found where 'entries' expected",argv[0]);
    entries = strtoul(argv[1], NULL, 0);
    if (entries <= 0)
	show_usage("%d entries in your table?", argv[1]);
    if ((*ac -= entries) < 0)
	show_usage("fewer entries than claimed ");

    table = *tabptr = malloc(entries*sizeof(int));
    for (i=0; i<entries; i++) {
	table[i] = strtol(argv[i+2], &end, 0);
	if (*end == 'K' || *end == 'k' )
	    end++, table[i] *= 1000 ;
	else if (*end == 'M')
	    end++, table[i] *= 1000000 ;
	if ( *end == 'B' || !strncmp(end, "by", 2) )
	    table[i] *= 8 ;
    }
    return entries;
}

/*
 * read loss rate table.
 * obviously, there are alternative ways to structure these little functions.
 * ...but does it matter for these things?
 */
static int
readlossratetable(char **argv, int *ac, int **tabptr)
{
    int entries, i;
    int *table;
    if (strcmp(argv[0],"entries"))
	show_usage("%s found where 'entries' expected",argv[0]);
    entries = strtoul(argv[1], NULL, 0);
    if (entries <= 0)
	show_usage("%d entries in your table?", argv[1]);
    if ((*ac -= entries) < 0)
	show_usage("fewer entries than claimed ");

    table = *tabptr = malloc(entries*sizeof(int));
    for (i=0; i<entries; i++) {
	double d = strtod(argv[i+2], NULL);
	if (d>1 || d<0)
	    show_usage("loss rate %s makes no sense",argv[i+2]);
	table[i] = (int)(d*0x7fffffff);
    }
    return entries;
}

/*
 * adjust_ac()
 * a _cheesy_ little fn because we have to make this check all of the time.
 */
static void
adjust_ac(int *ac, int i)
{
    if ((*ac+=i) < 0)
	show_usage("more args required\n");
}

static void
config_pipe(int ac, char **av)
{
       struct dn_pipe pipe;
        int i ;
	int arglen;
        char *end ;
 
        memset(&pipe, 0, sizeof pipe);
 
        av++; ac-=2;

        /* Pipe number */
        if (ac && isdigit(**av)) {
                pipe.pipe_nr = atoi(*av); av++; ac--;
        }
        while (ac >= 1) {
	    arglen = strlen(*av);

	    /* bandwidth */
            if (!strncmp(*av,"bw",arglen) ||
                ! strncmp(*av,"bandwidth",arglen)) {
		adjust_ac(&ac,-2);
		pipe.bw.dist = DN_DIST_CONST_RATE;
                pipe.bw.bandwidth = strtoul(av[1], &end, 0);
                if (*end == 'K' || *end == 'k' )
                        end++, pipe.bw.bandwidth *= 1000 ;
                else if (*end == 'M')
                        end++, pipe.bw.bandwidth *= 1000000 ;
                if ( *end == 'B' || !strncmp(end, "by", 2) )
                        pipe.bw.bandwidth *= 8 ;
                av+=2;
	    } else if (!strncmp(*av,"bwdist",arglen) ) {
		av++;
		adjust_ac(&ac,-1);
		arglen = strlen(*av);
		if (!strncmp(*av,"constant",arglen) ) {
		    adjust_ac(&ac,-2);
		    pipe.bw.dist=DN_DIST_CONST_RATE;
		    pipe.bw.bandwidth = readbw_rate("constant",av);
		    av+=2;
		} else if (!strncmp(*av,"uniform",arglen) ) {
		    adjust_ac(&ac,-7);
		    pipe.bw.dist=DN_DIST_UNIFORM;
		    pipe.bw.mean = readbw_rate("mean",av+1);
		    pipe.bw.variance = readbw_rate("var",av+3);
		    pipe.bw.quantum = readint("quantum",av+5);
		    av+=7;
		} else if (!strncmp(*av,"poisson",arglen) ) {
		    adjust_ac(&ac,-5);
		    pipe.bw.dist=DN_DIST_POISSON;
		    pipe.bw.mean = readbw_rate("mean",av+1);
		    pipe.bw.quantum = readint("quantum",av+3);
		    pipe.bw.table =
			build_poisson(pipe.bw.mean,&(pipe.bw.entries));
		    av+=5;
                } else if (!strncmp(*av,"random",arglen) ) {
		    adjust_ac(&ac,-4);
                    pipe.bw.dist=DN_DIST_TABLE_RANDOM;
		    pipe.bw.quantum = readint("quantum",av+1);
		    pipe.bw.entries =
			readbwratetable(av+3,&ac,&pipe.bw.table);
		    av+=4+pipe.bw.entries;
		} else if (!strncmp(*av,"determ",arglen) ) {
		    adjust_ac(&ac,-4);
		    pipe.bw.dist=DN_DIST_TABLE_DETERM;
		    pipe.bw.quantum = readint("quantum",av+1);
		    pipe.bw.entries =
			readbwratetable(av+3,&ac,&pipe.bw.table);
		    av+=4+pipe.bw.entries;
		} else show_usage("invalid bw distribution ``%s''", *av);

	    /* delay */
            } else if (!strcmp(*av,"delay") ) {
		adjust_ac(&ac,-2);
		pipe.delay.dist = DN_DIST_CONST_TIME;
                pipe.delay.delay = strtoul(av[1], NULL, 0);
                av+=2;
            } else if (!strncmp(*av,"delaydist",arglen) ) {
		adjust_ac(&ac,-1);
                av++;
		arglen = strlen(*av);
                if (!strncmp(*av,"constant",arglen) ) {
		    adjust_ac(&ac,-2);
                    pipe.delay.dist=DN_DIST_CONST_TIME;
		    pipe.delay.delay = strtoul(av[1], NULL, 0);
		    av+=2;
                } else if (!strncmp(*av,"uniform",arglen) ) {
		    adjust_ac(&ac,-5);
                    pipe.delay.dist=DN_DIST_UNIFORM;
		    pipe.delay.mean = readint("mean",av+1);
		    pipe.delay.variance = readint("var",av+3);
		    av+=5;
                } else if (!strncmp(*av,"poisson",arglen) ) {
		    adjust_ac(&ac,-3);
                    pipe.delay.dist=DN_DIST_POISSON;
		    pipe.delay.mean = readint("mean",av+1);
		    pipe.delay.table = 
			build_poisson(pipe.delay.mean,&(pipe.delay.entries));
		    av+=3;
                } else if (!strncmp(*av,"random",arglen) ) {
		    adjust_ac(&ac,-2);
                    pipe.delay.dist=DN_DIST_TABLE_RANDOM;
		    pipe.delay.entries = 
			readtable(av+1,&ac,&pipe.delay.table);
		    av+=2+pipe.delay.entries;
		} else if (!strncmp(*av,"determ",arglen) ) {
		    adjust_ac(&ac,-2);
		    pipe.delay.dist=DN_DIST_TABLE_DETERM;
		    pipe.delay.entries =
			readtable(av+1,&ac,&pipe.delay.table);
		    av+=2+pipe.delay.entries;
                } else show_usage("invalid delay distribution ``%s''", *av);

	    /* loss */
            } else if (!strncmp(*av,"plr",arglen) ) {
		adjust_ac(&ac,-2);
		pipe.loss.dist = DN_DIST_CONST_RATE;
		pipe.loss.plr = readlossrate("plr",av);
                av+=2;
            } else if (!strncmp(*av,"lossdist",arglen) ) {
		adjust_ac(&ac,-1);
		av++;
		arglen = strlen(*av);
		if (!strncmp(*av,"consttime",arglen) ) {
		    adjust_ac(&ac,-2);
		    pipe.loss.dist=DN_DIST_CONST_TIME;
		    pipe.loss.mean = atoi(av[1]);
		    av+=2;
		} else if (!strncmp(*av,"constrate",arglen) ) { 
		    adjust_ac(&ac,-2);
                    pipe.loss.dist=DN_DIST_CONST_RATE;
		    pipe.loss.plr = readlossrate("constrate",av);
		    av+=2;
		} else if (!strncmp(*av,"determ",arglen) ) {
		    adjust_ac(&ac,-4);
		    pipe.loss.dist=DN_DIST_TABLE_DETERM;
		    pipe.loss.quantum = readint("quantum",av+1);
		    pipe.loss.entries = 
			readtable(av+3,&ac,&pipe.loss.table);
		    av+=4+pipe.loss.entries;
		} else if (!strncmp(*av,"random",arglen) ) {
		    adjust_ac(&ac,-4);
		    pipe.loss.dist=DN_DIST_TABLE_RANDOM;
		    pipe.loss.quantum = readint("quantum",av+1);
		    pipe.loss.entries = 
			readlossratetable(av+3,&ac,&pipe.loss.table);
		    av+=4+pipe.loss.entries;
		} else if (!strncmp(*av,"uniform",arglen) ) {
		    adjust_ac(&ac,-6);
		    pipe.loss.dist=DN_DIST_UNIFORM;
		    pipe.loss.mean = readlossrate("mean",av+1);
		    pipe.loss.variance = readlossrate("var",av+3);
		    pipe.loss.quantum = readint("quantum",av+5);
		    av+=6;
		} else if (!strncmp(*av,"poisson",arglen) ) {
		    int i;
		    adjust_ac(&ac,-4);
		    pipe.loss.dist=DN_DIST_POISSON;
		    pipe.loss.mean = readlossrate("mean",av+1);
		    pipe.loss.quantum = readint("quantum",av+3);
		    pipe.loss.table =
			build_poisson(pipe.loss.mean>>0x10,&(pipe.loss.entries));
		    for (i = 0; i < pipe.loss.entries; i++)
			pipe.loss.table[i]<<=0x10;
		    av+=4;
		} else show_usage("invalid loss distribution ``%s''", *av);
	    } else if (!strncmp(*av,"queue",arglen) ) {
                end = NULL ;
                pipe.queue_size = strtoul(av[1], &end, 0);
                if (*end == 'K' || *end == 'k') {
                    pipe.queue_size_bytes = pipe.queue_size*1024 ;
                    pipe.queue_size = 0 ;
                } else if (*end == 'B' || !strncmp(end, "by", 2)) {
                    pipe.queue_size_bytes = pipe.queue_size ;
                    pipe.queue_size = 0 ;
                }
                av+=2; ac-=2;
            } else if (!strncmp(*av,"buckets",arglen) ) {
                pipe.rq_size = strtoul(av[1], NULL, 0);
                av+=2; ac-=2;
	    } else if (!strncmp(*av,"mask",arglen) ) {
		/* per-flow queue, mask is dst_ip, dst_port,
		 * src_ip, src_port, proto measured in bits
		 */
		u_int32_t a ;
		u_int32_t *par = NULL ;

		pipe.flow_mask.dst_ip = 0 ;
		pipe.flow_mask.src_ip = 0 ;
		pipe.flow_mask.dst_port = 0 ;
		pipe.flow_mask.src_port = 0 ;
		pipe.flow_mask.proto = 0 ;
		end = NULL ;
		av++ ; ac-- ;
		arglen = strlen(*av);
		if (ac >= 1 && !strncmp(*av,"all", arglen) ) {
		    /* special case -- all bits are significant */
		    pipe.flow_mask.dst_ip = ~0 ;
		    pipe.flow_mask.src_ip = ~0 ;
		    pipe.flow_mask.dst_port = ~0 ;
		    pipe.flow_mask.src_port = ~0 ;
		    pipe.flow_mask.proto = ~0 ;
		    pipe.flags |= DN_HAVE_FLOW_MASK ;
		    av++ ; ac-- ;
		} else {
		  for (;;) {
		    if (ac < 1)
			break ;
		    if (!strncmp(*av,"dst-ip", arglen))
			par = &(pipe.flow_mask.dst_ip) ;
		    else if (!strncmp(*av,"src-ip", arglen))
			par = &(pipe.flow_mask.src_ip) ;
		    else if (!strncmp(*av,"dst-port", arglen))
			(u_int16_t *)par = &(pipe.flow_mask.dst_port) ;
		    else if (!strncmp(*av,"src-port", arglen))
			(u_int16_t *)par = &(pipe.flow_mask.src_port) ;
		    else if (!strncmp(*av,"proto", arglen))
			(u_int8_t *)par = &(pipe.flow_mask.proto) ;
		    else
			break ;
		    if (ac < 2)
			show_usage("mask: %s value missing", *av);
		    if (*av[1] == '/') {
			a = strtoul(av[1]+1, &end, 0);
			if (a == 32) /* special case... */
			    a = ~0 ;
			else
			    a = (1 << a) - 1 ;
			fprintf(stderr, " mask is 0x%08x\n", a);
		    } else
			a = strtoul(av[1], &end, 0);
		    if ( (u_int16_t *)par == &(pipe.flow_mask.src_port) ||
			 (u_int16_t *)par == &(pipe.flow_mask.dst_port) ) {
			if (a >= (1<<16) )
			    show_usage("mask: %s must be 16 bit, not 0x%08x",
				*av, a);
			*((u_int16_t *)par) = (u_int16_t) a;
		    } else if ( (u_int8_t *)par == &(pipe.flow_mask.proto) ) {
			if (a >= (1<<8) )
			    show_usage("mask: %s must be 8 bit, not 0x%08x",
				*av, a);
			*((u_int8_t *)par) = (u_int8_t) a;
		    } else
			*par = a;
		    if (a != 0)
			pipe.flags |= DN_HAVE_FLOW_MASK ;
		    av += 2 ; ac -= 2 ;
		  } /* end for */
		}
            } else
                show_usage("unrecognised option ``%s''", *av);
        }

        if (pipe.pipe_nr == 0 )
            show_usage("pipe_nr %d be > 0", pipe.pipe_nr);
        if (pipe.queue_size > 100 )
            show_usage("queue size %d must be 2 <= x <= 100", pipe.queue_size);
#if 0
        printf("configuring pipe %d bw %d delay %d size %d\n",
                pipe.pipe_nr, pipe.bandwidth, pipe.delay, pipe.queue_size);
#endif
        i = setsockopt(s,IPPROTO_IP, IP_DUMMYNET_CONFIGURE, &pipe,sizeof pipe);
        if (i)
                err(1, "setsockopt(%s)", "IP_DUMMYNET_CONFIGURE");
	if (pipe.delay.table)
	    free(pipe.delay.table);
	if (pipe.loss.table)
	    free(pipe.loss.table);
	if (pipe.bw.table)
	    free(pipe.bw.table);
                
}

static void
add(ac,av)
	int ac;
	char **av;
{
	struct ip_fw rule;
	int i;
	u_char proto;
	struct protoent *pe;
	int saw_xmrc = 0, saw_via = 0;
	
	memset(&rule, 0, sizeof rule);

	av++; ac--;

	/* Rule number */
	if (ac && isdigit(**av)) {
		rule.fw_number = atoi(*av); av++; ac--;
	}

	/* Action */
	if (ac > 1 && !strncmp(*av, "prob", strlen(*av) ) ) {
		double d = strtod(av[1], NULL);
		if (d <= 0 || d > 1)
			show_usage("illegal match prob. %s", av[1]);
		if (d != 1) { /* 1 means always match */
			rule.fw_flg |= IP_FW_F_RND_MATCH ;
			/* we really store dont_match probability */
			(long)rule.pipe_ptr = (long)((1 - d) * 0x7fffffff) ;
		}
		av += 2 ; ac -= 2 ;
	}

	if (ac == 0)
		show_usage("missing action");
	if (!strncmp(*av,"accept",strlen(*av))
		    || !strncmp(*av,"pass",strlen(*av))
		    || !strncmp(*av,"allow",strlen(*av))
		    || !strncmp(*av,"permit",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_ACCEPT; av++; ac--;
	} else if (!strncmp(*av,"count",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_COUNT; av++; ac--;
        } else if (!strncmp(*av,"pipe",strlen(*av))) {
                rule.fw_flg |= IP_FW_F_PIPE; av++; ac--;
                if (!ac)
                        show_usage("missing pipe number");
                rule.fw_divert_port = strtoul(*av, NULL, 0); av++; ac--;
	} else if (!strncmp(*av,"divert",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_DIVERT; av++; ac--;
		if (!ac)
			show_usage("missing %s port", "divert");
		rule.fw_divert_port = strtoul(*av, NULL, 0); av++; ac--;
		if (rule.fw_divert_port == 0) {
			struct servent *s;
			setservent(1);
			s = getservbyname(av[-1], "divert");
			if (s != NULL)
				rule.fw_divert_port = ntohs(s->s_port);
			else
				show_usage("illegal %s port", "divert");
		}
	} else if (!strncmp(*av,"tee",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_TEE; av++; ac--;
		if (!ac)
			show_usage("missing %s port", "tee divert");
		rule.fw_divert_port = strtoul(*av, NULL, 0); av++; ac--;
		if (rule.fw_divert_port == 0) {
			struct servent *s;
			setservent(1);
			s = getservbyname(av[-1], "divert");
			if (s != NULL)
				rule.fw_divert_port = ntohs(s->s_port);
			else
				show_usage("illegal %s port", "tee divert");
		}
#ifndef IPFW_TEE_IS_FINALLY_IMPLEMENTED
		err(EX_USAGE, "the ``tee'' action is not implemented");
#endif
	} else if (!strncmp(*av,"fwd",strlen(*av)) ||
		   !strncmp(*av,"forward",strlen(*av))) {
		struct in_addr dummyip;
		char *pp;
		rule.fw_flg |= IP_FW_F_FWD; av++; ac--;
		if (!ac)
			show_usage("missing forwarding IP address");
		rule.fw_fwd_ip.sin_len = sizeof(struct sockaddr_in);
		rule.fw_fwd_ip.sin_family = AF_INET;
		rule.fw_fwd_ip.sin_port = 0;
		pp = strchr(*av, ':');
		if(pp == NULL)
			pp = strchr(*av, ',');
		if(pp != NULL)
		{
			*(pp++) = '\0';
			i = lookup_port(pp, 1, 0);
			if (i == -1)
				show_usage("illegal forwarding port ``%s''", pp);
			else
				rule.fw_fwd_ip.sin_port = (u_short)i;
		}
		fill_ip(&(rule.fw_fwd_ip.sin_addr), &dummyip, &ac, &av);
		if (rule.fw_fwd_ip.sin_addr.s_addr == 0)
			show_usage("illegal forwarding IP address");

	} else if (!strncmp(*av,"skipto",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_SKIPTO; av++; ac--;
		if (!ac)
			show_usage("missing skipto rule number");
		rule.fw_skipto_rule = strtoul(*av, NULL, 0); av++; ac--;
	} else if ((!strncmp(*av,"deny",strlen(*av))
		    || !strncmp(*av,"drop",strlen(*av)))) {
		rule.fw_flg |= IP_FW_F_DENY; av++; ac--;
	} else if (!strncmp(*av,"reject",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_REJECT; av++; ac--;
		rule.fw_reject_code = ICMP_UNREACH_HOST;
	} else if (!strncmp(*av,"reset",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_REJECT; av++; ac--;
		rule.fw_reject_code = IP_FW_REJECT_RST;	/* check TCP later */
	} else if (!strncmp(*av,"unreach",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_REJECT; av++; ac--;
		fill_reject_code(&rule.fw_reject_code, *av); av++; ac--;
	} else if (!strncmp(*av,"check-state",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_CHECK_S ; av++; ac--;
		goto done;
	} else {
		show_usage("invalid action ``%s''", *av);
	}

	/* [log] */
	if (ac && !strncmp(*av,"log",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_PRN; av++; ac--;
	}
	if (ac && !strncmp(*av,"logamount",strlen(*av))) {
		if (!(rule.fw_flg & IP_FW_F_PRN))
			show_usage("``logamount'' not valid without ``log''");
		ac--; av++;
		if (!ac)
			show_usage("``logamount'' requires argument");
		rule.fw_logamount = atoi(*av);
		if (rule.fw_logamount <= 0)
			show_usage("``logamount'' argument must be greater "
			    "than 0");
		ac--; av++;
	}

	/* protocol */
	if (ac == 0)
		show_usage("missing protocol");
	if ((proto = atoi(*av)) > 0) {
		rule.fw_prot = proto; av++; ac--;
	} else if (!strncmp(*av,"all",strlen(*av))) {
		rule.fw_prot = IPPROTO_IP; av++; ac--;
	} else if ((pe = getprotobyname(*av)) != NULL) {
		rule.fw_prot = pe->p_proto; av++; ac--;
	} else {
		show_usage("invalid protocol ``%s''", *av);
	}

	if (rule.fw_prot != IPPROTO_TCP
	    && (rule.fw_flg & IP_FW_F_COMMAND) == IP_FW_F_REJECT
	    && rule.fw_reject_code == IP_FW_REJECT_RST)
		show_usage("``reset'' is only valid for tcp packets");

	/* from */
	if (ac && !strncmp(*av,"from",strlen(*av))) { av++; ac--; }
	else
		show_usage("missing ``from''");

	if (ac && !strncmp(*av,"not",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_INVSRC;
		av++; ac--;
	}
	if (!ac)
		show_usage("missing arguments");

	fill_ip(&rule.fw_src, &rule.fw_smsk, &ac, &av);

	if (ac && (isdigit(**av) || lookup_port(*av, 1, 1) >= 0)) {
		u_short nports = 0;
		int retval ;

		retval = fill_port(&nports, rule.fw_uar.fw_pts, 0, *av) ;
		if (retval == 1)
			rule.fw_flg |= IP_FW_F_SRNG;
		else if (retval == 2)
			rule.fw_flg |= IP_FW_F_SMSK;
		IP_FW_SETNSRCP(&rule, nports);
		av++; ac--;
	}

	/* to */
	if (ac && !strncmp(*av,"to",strlen(*av))) { av++; ac--; }
	else
		show_usage("missing ``to''");

	if (ac && !strncmp(*av,"not",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_INVDST;
		av++; ac--;
	}
	if (!ac)
		show_usage("missing arguments");

	fill_ip(&rule.fw_dst, &rule.fw_dmsk, &ac, &av);

	if (ac && (isdigit(**av) || lookup_port(*av, 1, 1) >= 0)) {
		u_short	nports = 0;
		int retval ;

		retval = fill_port(&nports,
		    rule.fw_uar.fw_pts, IP_FW_GETNSRCP(&rule), *av) ;
		if (retval == 1)
			rule.fw_flg |= IP_FW_F_DRNG;
		else if (retval == 2)
			rule.fw_flg |= IP_FW_F_DMSK;
		IP_FW_SETNDSTP(&rule, nports);
		av++; ac--;
	}

	if ((rule.fw_prot != IPPROTO_TCP) && (rule.fw_prot != IPPROTO_UDP)
	    && (IP_FW_GETNSRCP(&rule) || IP_FW_GETNDSTP(&rule))) {
		show_usage("only TCP and UDP protocols are valid"
		    " with port specifications");
	}

	while (ac) {
		if (!strncmp(*av,"uid",strlen(*av))) {
			struct passwd *pwd;
			char *end;
			uid_t uid;

			rule.fw_flg |= IP_FW_F_UID;
			ac--; av++;
			if (!ac)
				show_usage("``uid'' requires argument");
			
			uid = strtoul(*av, &end, 0);
			if (*end == '\0')
				pwd = getpwuid(uid);
			else
				pwd = getpwnam(*av);
			if (pwd == NULL)
				show_usage("uid \"%s\" is nonexistant", *av);
			rule.fw_uid = pwd->pw_uid;
			ac--; av++;
			continue;
		}
		if (!strncmp(*av,"gid",strlen(*av))) {
			struct group *grp;
			char *end;
			gid_t gid;

			rule.fw_flg |= IP_FW_F_GID;
			ac--; av++;
			if (!ac)
				show_usage("``gid'' requires argument");
			
			gid = strtoul(*av, &end, 0);
			if (*end == '\0')
				grp = getgrgid(gid);
			else
				grp = getgrnam(*av);
			if (grp == NULL)
				show_usage("gid \"%s\" is nonexistant", *av);
			rule.fw_gid = grp->gr_gid;
			ac--; av++;
			continue;
		}
		if (!strncmp(*av,"in",strlen(*av))) { 
			rule.fw_flg |= IP_FW_F_IN;
			av++; ac--; continue;
		}
		if (!strncmp(*av,"keep-state",strlen(*av))) { 
			u_long type ;
			rule.fw_flg |= IP_FW_F_KEEP_S;

			av++; ac--;
			if (ac > 0 && (type = atoi(*av)) != 0) {
			    (int)rule.next_rule_ptr = type ;
			    av++; ac--;
			}
			continue;
		}
		if (!strncmp(*av,"bridged",strlen(*av))) { 
			rule.fw_flg |= IP_FW_BRIDGED;
			av++; ac--; continue;
		}
		if (!strncmp(*av,"out",strlen(*av))) { 
			rule.fw_flg |= IP_FW_F_OUT;
			av++; ac--; continue;
		}
		if (ac && !strncmp(*av,"xmit",strlen(*av))) {
			union ip_fw_if ifu;
			int byname;

			if (saw_via) {
badviacombo:
				show_usage("``via'' is incompatible"
				    " with ``xmit'' and ``recv''");
			}
			saw_xmrc = 1;
			av++; ac--; 
			fill_iface("xmit", &ifu, &byname, ac, *av);
			rule.fw_out_if = ifu;
			rule.fw_flg |= IP_FW_F_OIFACE;
			if (byname)
				rule.fw_flg |= IP_FW_F_OIFNAME;
			av++; ac--; continue;
		}
		if (ac && !strncmp(*av,"recv",strlen(*av))) {
			union ip_fw_if ifu;
			int byname;

			if (saw_via)
				goto badviacombo;
			saw_xmrc = 1;
			av++; ac--; 
			fill_iface("recv", &ifu, &byname, ac, *av);
			rule.fw_in_if = ifu;
			rule.fw_flg |= IP_FW_F_IIFACE;
			if (byname)
				rule.fw_flg |= IP_FW_F_IIFNAME;
			av++; ac--; continue;
		}
		if (ac && !strncmp(*av,"via",strlen(*av))) {
			union ip_fw_if ifu;
			int byname = 0;

			if (saw_xmrc)
				goto badviacombo;
			saw_via = 1;
			av++; ac--; 
			fill_iface("via", &ifu, &byname, ac, *av);
			rule.fw_out_if = rule.fw_in_if = ifu;
			if (byname)
				rule.fw_flg |=
				    (IP_FW_F_IIFNAME | IP_FW_F_OIFNAME);
			av++; ac--; continue;
		}
		if (!strncmp(*av,"fragment",strlen(*av))) {
			rule.fw_flg |= IP_FW_F_FRAG;
			av++; ac--; continue;
		}
		if (!strncmp(*av,"ipoptions",strlen(*av))) { 
			av++; ac--; 
			if (!ac)
				show_usage("missing argument"
				    " for ``ipoptions''");
			fill_ipopt(&rule.fw_ipopt, &rule.fw_ipnopt, av);
			av++; ac--; continue;
		}
		if (rule.fw_prot == IPPROTO_TCP) {
			if (!strncmp(*av,"established",strlen(*av))) { 
				rule.fw_tcpf  |= IP_FW_TCPF_ESTAB;
				av++; ac--; continue;
			}
			if (!strncmp(*av,"setup",strlen(*av))) { 
				rule.fw_tcpf  |= IP_FW_TCPF_SYN;
				rule.fw_tcpnf  |= IP_FW_TCPF_ACK;
				av++; ac--; continue;
			}
			if (!strncmp(*av,"tcpflags",strlen(*av))) { 
				av++; ac--; 
				if (!ac)
					show_usage("missing argument"
					    " for ``tcpflags''");
				fill_tcpflag(&rule.fw_tcpf, &rule.fw_tcpnf, av);
				av++; ac--; continue;
			}
		}
		if (rule.fw_prot == IPPROTO_ICMP) {
			if (!strncmp(*av,"icmptypes",strlen(*av))) {
				av++; ac--;
				if (!ac)
					show_usage("missing argument"
					    " for ``icmptypes''");
				fill_icmptypes(rule.fw_uar.fw_icmptypes,
				    av, &rule.fw_flg);
				av++; ac--; continue;
			}
		}
		show_usage("unknown argument ``%s''", *av);
	}

	/* No direction specified -> do both directions */
	if (!(rule.fw_flg & (IP_FW_F_OUT|IP_FW_F_IN)))
		rule.fw_flg |= (IP_FW_F_OUT|IP_FW_F_IN);

	/* Sanity check interface check, but handle "via" case separately */
	if (saw_via) {
		if (rule.fw_flg & IP_FW_F_IN)
			rule.fw_flg |= IP_FW_F_IIFACE;
		if (rule.fw_flg & IP_FW_F_OUT)
			rule.fw_flg |= IP_FW_F_OIFACE;
	} else if ((rule.fw_flg & IP_FW_F_OIFACE) && (rule.fw_flg & IP_FW_F_IN))
		show_usage("can't check xmit interface of incoming packets");

	/* frag may not be used in conjunction with ports or TCP flags */
	if (rule.fw_flg & IP_FW_F_FRAG) {
		if (rule.fw_tcpf || rule.fw_tcpnf)
			show_usage("can't mix 'frag' and tcpflags");

		if (rule.fw_nports)
			show_usage("can't mix 'frag' and port specifications");
	}
	if (rule.fw_flg & IP_FW_F_PRN) {
		if (!rule.fw_logamount) {
			size_t len = sizeof(int);

			if (sysctlbyname("net.inet.ip.fw.verbose_limit",
			    &rule.fw_logamount, &len, NULL, 0) == -1)
				errx(1, "sysctlbyname(\"%s\")",
				    "net.inet.ip.fw.verbose_limit");
		}
		rule.fw_loghighest = rule.fw_logamount;
	}
done:
	if (!do_quiet)
		show_ipfw(&rule, 10, 10);
	i = setsockopt(s, IPPROTO_IP, IP_FW_ADD, &rule, sizeof rule);
	if (i)
		err(EX_UNAVAILABLE, "setsockopt(%s)", "IP_FW_ADD");
}

static void
zero (ac, av)
	int ac;
	char **av;
{
	av++; ac--;

	if (!ac) {
		/* clear all entries */
		if (setsockopt(s,IPPROTO_IP,IP_FW_ZERO,NULL,0)<0)
			err(EX_UNAVAILABLE, "setsockopt(%s)", "IP_FW_ZERO");
		if (!do_quiet)
			printf("Accounting cleared.\n");
	} else {
		struct ip_fw rule;
		int failed = EX_OK;

		memset(&rule, 0, sizeof rule);
		while (ac) {
			/* Rule number */
			if (isdigit(**av)) {
				rule.fw_number = atoi(*av); av++; ac--;
				if (setsockopt(s, IPPROTO_IP,
				    IP_FW_ZERO, &rule, sizeof rule)) {
					warn("rule %u: setsockopt(%s)", rule.fw_number,
						 "IP_FW_ZERO");
					failed = EX_UNAVAILABLE;
				}
				else if (!do_quiet)
					printf("Entry %d cleared\n",
					    rule.fw_number);
			} else
				show_usage("invalid rule number ``%s''", *av);
		}
		if (failed != EX_OK)
			exit(failed);
	}
}

static void
resetlog (ac, av)
	int ac;
	char **av;
{
	av++; ac--;

	if (!ac) {
		/* clear all entries */
		if (setsockopt(s,IPPROTO_IP,IP_FW_RESETLOG,NULL,0)<0)
			err(EX_UNAVAILABLE, "setsockopt(%s)", "IP_FW_RESETLOG");
		if (!do_quiet)
			printf("Logging counts reset.\n");
	} else {
		struct ip_fw rule;
		int failed = EX_OK;

		memset(&rule, 0, sizeof rule);
		while (ac) {
			/* Rule number */
			if (isdigit(**av)) {
				rule.fw_number = atoi(*av); av++; ac--;
				if (setsockopt(s, IPPROTO_IP,
				    IP_FW_RESETLOG, &rule, sizeof rule)) {
					warn("rule %u: setsockopt(%s)", rule.fw_number,
						 "IP_FW_RESETLOG");
					failed = EX_UNAVAILABLE;
				}
				else if (!do_quiet)
					printf("Entry %d logging count reset\n",
					    rule.fw_number);
			} else
				show_usage("invalid rule number ``%s''", *av);
		}
		if (failed != EX_OK)
			exit(failed);
	}
}

static int
ipfw_main(ac,av)
	int 	ac;
	char 	**av;
{

	int 		ch;
	extern int 	optreset; /* XXX should be declared in <unistd.h> */

	if ( ac == 1 ) {
		show_usage(NULL);
	}

	/* Set the force flag for non-interactive processes */
	do_force = !isatty(STDIN_FILENO);

	optind = optreset = 1;
	while ((ch = getopt(ac, av, "s:afqtN")) != -1)
	switch(ch) {
		case 's': /* sort */
			do_sort= atoi(optarg);
			break;
		case 'a':
			do_acct=1;
			break;
		case 'f':
			do_force=1;
			break;
		case 'q':
			do_quiet=1;
			break;
		case 't':
			do_time=1;
			break;
		case 'N':
	 		do_resolv=1;
			break;
		default:
			show_usage(NULL);
	}

	ac -= optind;
	if (*(av+=optind)==NULL) {
		 show_usage("bad arguments");
	}

        if (!strncmp(*av, "pipe", strlen(*av))) {
                do_pipe = 1 ;
                ac-- ;
                av++ ;
        }
	if (!ac) {
		show_usage("pipe requires arguments");
	}
        /* allow argument swapping */
        if (ac > 1 && *av[0]>='0' && *av[0]<='9') {
                char *p = av[0] ;
                av[0] = av[1] ;
                av[1] = p ;
        }
	if (!strncmp(*av, "add", strlen(*av))) {
		add(ac,av);
        } else if (do_pipe && !strncmp(*av, "config", strlen(*av))) {
                config_pipe(ac,av);
	} else if (!strncmp(*av, "delete", strlen(*av))) {
		delete(ac,av);
	} else if (!strncmp(*av, "flush", strlen(*av))) {
		int do_flush = 0;

		if ( do_force || do_quiet )
			do_flush = 1;
		else {
			int c;

			/* Ask the user */
			printf("Are you sure? [yn] ");
			do {
				fflush(stdout);
				c = toupper(getc(stdin));
				while (c != '\n' && getc(stdin) != '\n')
					if (feof(stdin))
						return (0);
			} while (c != 'Y' && c != 'N');
			printf("\n");
			if (c == 'Y') 
				do_flush = 1;
		}
		if ( do_flush ) {
			if (setsockopt(s, IPPROTO_IP,
				do_pipe ? IP_DUMMYNET_FLUSH : IP_FW_FLUSH, NULL, 0) < 0)
			    err(EX_UNAVAILABLE, "setsockopt(IP_%s_FLUSH)",
				do_pipe ? "DUMMYNET" : "FW");
			if (!do_quiet)
				printf("Flushed all %s.\n",
				    do_pipe ? "pipes" : "rules");
		}
	} else if (!strncmp(*av, "zero", strlen(*av))) {
		zero(ac,av);
	} else if (!strncmp(*av, "resetlog", strlen(*av))) {
		resetlog(ac,av);
	} else if (!strncmp(*av, "print", strlen(*av))) {
		list(--ac,++av);
	} else if (!strncmp(*av, "list", strlen(*av))) {
		list(--ac,++av);
	} else if (!strncmp(*av, "show", strlen(*av))) {
		do_acct++;
		list(--ac,++av);
	} else {
		show_usage("bad arguments");
	}
	return 0;
}

int 
main(ac, av)
	int	ac;
	char	**av;
{
#define MAX_ARGS	32
#define WHITESP		" \t\f\v\n\r"
	char	buf[BUFSIZ];
	char	*a, *p, *args[MAX_ARGS], *cmd = NULL;
	char	linename[10];
	int 	i, c, qflag, pflag, status;
	FILE	*f = NULL;
	pid_t	preproc = 0;

	s = socket( AF_INET, SOCK_RAW, IPPROTO_RAW );
	if ( s < 0 )
		err(EX_UNAVAILABLE, "socket");

	setbuf(stdout,0);

/*
 * this is a nasty check on the last argument!!!
 * If there happens to be a filename matching a keyword in the current
 * directory, things will fail miserably.
 */

	if (ac > 1 && access(av[ac - 1], R_OK) == 0) {
		qflag = pflag = i = 0;
		lineno = 0;

		while ((c = getopt(ac, av, "D:U:p:q")) != -1)
			switch(c) {
			case 'D':
				if (!pflag)
					errx(EX_USAGE, "-D requires -p");
				if (i > MAX_ARGS - 2)
					errx(EX_USAGE,
					     "too many -D or -U options");
				args[i++] = "-D";
				args[i++] = optarg;
				break;

			case 'U':
				if (!pflag)
					errx(EX_USAGE, "-U requires -p");
				if (i > MAX_ARGS - 2)
					errx(EX_USAGE,
					     "too many -D or -U options");
				args[i++] = "-U";
				args[i++] = optarg;
				break;

			case 'p':
				pflag = 1;
				cmd = optarg;
				args[0] = cmd;
				i = 1;
				break;

			case 'q':
				qflag = 1;
				break;

			default:
				show_usage(NULL);
			}

		av += optind;
		ac -= optind;
		if (ac != 1)
			show_usage("extraneous filename arguments");

		if ((f = fopen(av[0], "r")) == NULL)
			err(EX_UNAVAILABLE, "fopen: %s", av[0]);

		if (pflag) {
			/* pipe through preprocessor (cpp or m4) */
			int pipedes[2];

			args[i] = 0;

			if (pipe(pipedes) == -1)
				err(EX_OSERR, "cannot create pipe");

			switch((preproc = fork())) {
			case -1:
				err(EX_OSERR, "cannot fork");

			case 0:
				/* child */
				if (dup2(fileno(f), 0) == -1 ||
				    dup2(pipedes[1], 1) == -1)
					err(EX_OSERR, "dup2()");
				fclose(f);
				close(pipedes[1]);
				close(pipedes[0]);
				execvp(cmd, args);
				err(EX_OSERR, "execvp(%s) failed", cmd);

			default:
				/* parent */
				fclose(f);
				close(pipedes[1]);
				if ((f = fdopen(pipedes[0], "r")) == NULL) {
					int savederrno = errno;

					(void)kill(preproc, SIGTERM);
					errno = savederrno;
					err(EX_OSERR, "fdopen()");
				}
			}
		}

		while (fgets(buf, BUFSIZ, f)) {
			lineno++;
			sprintf(linename, "Line %d", lineno);
			args[0] = linename;

			if (*buf == '#')
				continue;
			if ((p = strchr(buf, '#')) != NULL)
				*p = '\0';
			i=1;
			if (qflag) args[i++]="-q";
			for (a = strtok(buf, WHITESP);
			    a && i < MAX_ARGS; a = strtok(NULL, WHITESP), i++)
				args[i] = a;
			if (i == (qflag? 2: 1))
				continue;
			if (i == MAX_ARGS)
				errx(EX_USAGE, "%s: too many arguments", linename);
			args[i] = NULL;

			ipfw_main(i, args); 
		}
		fclose(f);
		if (pflag) {
			if (waitpid(preproc, &status, 0) != -1) {
				if (WIFEXITED(status)) {
					if (WEXITSTATUS(status) != EX_OK)
						errx(EX_UNAVAILABLE,
						     "preprocessor exited with status %d",
						     WEXITSTATUS(status));
				} else if (WIFSIGNALED(status)) {
					errx(EX_UNAVAILABLE,
					     "preprocessor exited with signal %d",
					     WTERMSIG(status));
				}
			}
		}

	} else
		ipfw_main(ac,av);
	return EX_OK;
}
