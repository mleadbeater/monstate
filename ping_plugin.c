/*
Copyright (c) 2009-2019, Martin Leadbeater
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/* Note: based on an original program by Mike Muuss.

   This program has been modified to ensure the program does not hang
   if the ping fails. The changes add a flag called 'use_nonblocking_socket' 
   which can be set if you wish. That will cause the program to use a 
   simple poll with backoff strategy and to wait for data.
   
 */

/*
 *			P I N G . C
 *
 * Using the InterNet Control Message Protocol (ICMP) "ECHO" facility,
 * measure round-trip-delays and packet loss across network paths.
 *
 * Author -
 *	Mike Muuss
 *	U. S. Army Ballistic Research Laboratory
 *	December, 1983
 * Modified at Uc Berkeley
 *
 * Changed argument to inet_ntoa() to be struct in_addr instead of u_long
 * DFM BRL 1992
 *
 * Status -
 *	Public Domain.  Distribution Unlimited.
 *
 * Bugs -
 *	More statistics could always be gathered.
 *	This program has to run SUID to ROOT to access the ICMP socket.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/time.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "plugin.h"
#include "symboltable.h"
#include "buffers.h"
#include "regular_expressions.h"

#define	MAXWAIT		2	/* max time to wait for response, sec. */
#define	MAXPACKET	4096	/* max packet size */
#define VERBOSE		1	/* verbose flag */
#define QUIET		2	/* quiet flag */
#define FLOOD		4	/* floodping flag */
#define NONBLOCKING 8	/* floodping flag */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN	64
#endif

u_char	packet[MAXPACKET];
int	i, pingflags, options;
extern	int errno;

int s;			/* Socket file descriptor */
struct hostent *hp;	/* Pointer to host info */
struct timezone tz;	/* leftover */

struct sockaddr whereto;/* Who to ping */
int datalen;		/* How much data */

char usage[] =
"Usage:  ping [-dfqrvn] host [packetsize [count [preload]]]\n";

char *hostname;
char hnamebuf[MAXHOSTNAMELEN];

int npackets = 1;		/* by default, only send a single packet */
int preload = 0;		/* number of packets to "preload" */
int ntransmitted = 0;		/* sequence # for outbound packets = #sent */
int ident;

int nreceived = 0;		/* # of packets we got back */
int nlost = 0;          /* # of packets we lost */
int timing = 0;
int tmin = 999999999;
int tmax = 0;
int tsum = 0;			/* sum of all times, for doing average */
static void finish(int), catcher(int), pinger(int);
/*char *inet_ntoa();*/

static void pr_pack(unsigned char *buf, int cc, struct sockaddr_in *from);
static void tvsub( struct timeval *out, struct timeval *in);
static int in_cksum(u_short *addr, int len);
static void report();

sig_t saved_alarm_sig = NULL;

char *output = NULL;
int use_nonblocking_socket = 0;
int done;

static void init()
{
	done = 0;
	npackets = 1;		/* by default, only send a single packet */
	preload = 0;		/* number of packets to "preload" */
	ntransmitted = 0;		/* sequence # for outbound packets = #sent */

	nreceived = 0;		/* # of packets we got back */
	nlost = 0;          /* # of packets we lost */
	timing = 0;
	tmin = 999999999;
	tmax = 0;
	tsum = 0;			/* sum of all times, for doing average */
	saved_alarm_sig = SIG_DFL;
}

static void setup_signals()
{
	/*signal( SIGINT, finish );*/
    siginterrupt(SIGALRM, 1); /* this code handles EINTR results */
	saved_alarm_sig = signal(SIGALRM, catcher);
}

static void cleanup_signals()
{
    alarm(0);
	if (saved_alarm_sig)
		signal(SIGALRM, saved_alarm_sig);
    else
        signal(SIGALRM, SIG_DFL);

    saved_alarm_sig = 0;
    siginterrupt(SIGALRM, 0); /* restore default behaviour of restarting system calls */
}

/*
 * 			M A I N
 */
static int ping(int argc, const char **argv)
{
	struct sockaddr_in from;
	const char **av = argv;
	struct sockaddr_in *to = (struct sockaddr_in *) &whereto;
	int on = 1;
	struct protoent *proto;
	char *out;
    
    /* support for use of a non-blocking socket */
#define PING_RETRIES 10
#define INIT_SLEEPTIME 1000
        int retries = PING_RETRIES; /* we retry our recvfrom a few times in the case of an EAGAIN error */
        useconds_t sleeptime = INIT_SLEEPTIME; /* initial sleep time in microsecs. This doubles each retry to
                                    a max of 100ms */
        useconds_t max_sleep = 100000L;

        init();

	argc--, av++;
	while (argc > 0 && *av[0] == '-') {
        int opt=1;
		while (av[0][opt]) switch (av[0][opt++]) {
			case 'd':
				options |= SO_DEBUG;
				break;
			case 'r':
				options |= SO_DONTROUTE;
				break;
			case 'v':
				pingflags |= VERBOSE;
				break;
			case 'q':
				pingflags |= QUIET;
				break;
			case 'n':
				pingflags |= NONBLOCKING;
                use_nonblocking_socket = 1;
				break;
			case 'f':
                asprintf(&out, "Flood mode is not permitted\n");
                output = append_buffer(output, out);
				/* pingflags |= FLOOD; */
				break;
		}
		argc--, av++;
	}
	if(argc < 1 || argc > 4)  {
		asprintf(&out, "%s", usage);
		output = append_buffer(output, out);
		return 1;
	}

	memset((char *)&whereto, 0, sizeof(struct sockaddr) );
	to->sin_family = AF_INET;
	to->sin_addr.s_addr = inet_addr(av[0]);
	if(to->sin_addr.s_addr != (unsigned)-1) {
		strcpy(hnamebuf, av[0]);
		hostname = hnamebuf;
	} else {
		hp = gethostbyname(av[0]);
		if (hp) {
			to->sin_family = hp->h_addrtype;
			memmove((caddr_t)&to->sin_addr, hp->h_addr, hp->h_length);
			hostname = hp->h_name;
		} else {
			asprintf(&out, "%s: unknown host %s\n", argv[0], av[0]);
			output = append_buffer(output, out);
			return 1;
		}
	}

	if( argc >= 2 )
		datalen = atoi( av[1] );
	else
		datalen = 64-8;
	if (datalen > MAXPACKET) {
		asprintf(&out, "%s", "ping: packet size too large\n");
        output = append_buffer(output, out);
		return 1;
	}
	if (datalen >= sizeof(struct timeval))	/* can we time 'em? */
		timing = 1;

	if (argc >= 3)
		npackets = atoi(av[2]);

	if (argc == 4)
		preload = atoi(av[3]);

	ident = getpid() & 0xFFFF;

	if ((proto = getprotobyname("icmp")) == NULL) {
		asprintf(&out, "%s", "icmp: unknown protocol\n");
        output = append_buffer(output, out);
		return 10;
	}

	if ((s = socket(AF_INET, SOCK_RAW, proto->p_proto)) < 0) {
		asprintf(&out, "ping: socket: %s (%d)\n", strerror(errno), errno);
        output = append_buffer(output, out);
		return 5;
	}
    
	if (options & SO_DEBUG) {
		if(pingflags & VERBOSE) {
			asprintf(&out, "%s", "...debug on.\n");
			output = append_buffer(output, out);
		}
		setsockopt(s, SOL_SOCKET, SO_DEBUG, &on, sizeof(on));
	}
	if (options & SO_DONTROUTE) {
		if(pingflags & VERBOSE) {
			asprintf(&out, "%s", "...no routing.\n");
			output = append_buffer(output, out);
		}
		setsockopt(s, SOL_SOCKET, SO_DONTROUTE, &on, sizeof(on));
	}

	if(to->sin_family == AF_INET) {
		asprintf(&out, "PING %s (%s): %d data bytes\n", hostname,
		  inet_ntoa(to->sin_addr), datalen);	/* DFM */
		output = append_buffer(output, out);
	} else {
		asprintf(&out, "PING %s: %d data bytes\n", hostname, datalen );
		output = append_buffer(output, out);
	}
	/*setlinebuf( stdout );*/

    if (use_nonblocking_socket) {
        int flags = fcntl(s, F_GETFL, NULL);
        if (flags < 0 || fcntl(s, F_SETFL, flags | O_NONBLOCK) < 0) {
            perror("Nonblocking fcntl:");
            close(2);
            return 1;
        }
    }
    setup_signals();

	/* fire off them quickies */
	for(i=0; i < preload; i++)
		pinger(0);

	if(!(pingflags & FLOOD)) {
        catcher(0);	/* start things going */
    }
        
	while (!done) {
		int len = sizeof (packet);
		unsigned int fromlen = sizeof (from);
		int cc;
		struct timeval timeout;
		fd_set fdmask;
		FD_ZERO(&fdmask);
		FD_SET(s, &fdmask);
		/* int fdmask = 1 << s; ML*/

		timeout.tv_sec = 0;
		timeout.tv_usec = 10000;

        /* note: ping flood is not permitted */
        /*
		if(pingflags & FLOOD) { 
			pinger(0);
			if( select(32, &fdmask, 0, 0, &timeout) == 0)
				continue;
		}
        */
		if ( (cc=recvfrom(s, packet, len, 0, (struct sockaddr *)&from, &fromlen)) < 0) {
			if( errno == EINTR )
				continue;

            if (use_nonblocking_socket) {
                if ( errno != EAGAIN ) {
                    asprintf(&out, "ping: recvfrom: %s (%d)\n", strerror(errno), errno);
                    output = append_buffer(output, out);
                    continue;
                }
                else /*if (ntransmitted > nreceived)*/ {
                    /* we are waiting for a packet, here is our poll and backoff algorithm */
                    if (retries) {
                        retries--;
                        usleep(sleeptime);
                        sleeptime *= 2;
                        if (sleeptime > max_sleep) {
                            sleeptime = max_sleep;
                        }
                    }
                    else {
                        nlost++; /* give up on this packet */
                        if (npackets && ntransmitted >= npackets)
                            done = 1;
                        retries = PING_RETRIES;
                        sleeptime = INIT_SLEEPTIME;
                    }
                }
                /*else
                    sleep(1); *//* wait for the alarm */
            }
            else {
                perror("ping: recvfrom");
            }
            continue;
		}
		pr_pack( packet, cc, &from );
        if (npackets && ntransmitted >= npackets)
            done = 1;
        retries = PING_RETRIES;
        sleeptime = INIT_SLEEPTIME;
	}
    cleanup_signals();
    close(s);
    report();
	return 0;
}

/*
 * 			C A T C H E R
 * 
 * This routine causes another PING to be transmitted, and then
 * schedules another SIGALRM for 1 second from now.
 * 
 * Bug -
 * 	Our sense of time will slowly skew (ie, packets will not be launched
 * 	exactly at 1-second intervals).  This does not affect the quality
 *	of the delay and loss statistics.
 */
static void catcher(int sig)
{
	int waittime;

    pinger(0);
	if (npackets == 0 || ntransmitted < npackets)
		alarm(1);
	else {
		if (nreceived) {
			waittime = 2 * tmax / 1000;
			if (waittime == 0)
				waittime = 1;
		} else
			waittime = MAXWAIT;
		signal(SIGALRM, finish);
		alarm(waittime);
	}
}

/*
 * 			P I N G E R
 * 
 * Compose and transmit an ICMP ECHO REQUEST packet.  The IP packet
 * will be added on by the kernel.  The ID field is our UNIX process ID,
 * and the sequence number is an ascending integer.  The first 8 bytes
 * of the data portion are used to hold a UNIX "timeval" struct in VAX
 * byte-order, to compute the round-trip time.
 */
static void pinger(int sig)
{
	static u_char outpack[MAXPACKET];
	register struct icmp *icp = (struct icmp *) outpack;
	int i, cc;
	register struct timeval *tp = (struct timeval *) &outpack[8];
	register u_char *datap = &outpack[8+sizeof(struct timeval)];
	char *out;

	icp->icmp_type = ICMP_ECHO;
	icp->icmp_code = 0;
	icp->icmp_cksum = 0;
	icp->icmp_seq = ntransmitted++;
	icp->icmp_id = ident;		/* ID */

	cc = datalen+8;			/* skips ICMP portion */

	if (timing)
		gettimeofday( tp, &tz );

	for( i=8; i<datalen; i++)	/* skip 8 for time */
		*datap++ = i;

	/* Compute ICMP checksum here */
	icp->icmp_cksum = in_cksum( (u_short *)icp, cc );

    /* cc = sendto(s, msg, len, flags, to, tolen) */
    i = sendto( s, outpack, cc, 0, &whereto, sizeof(struct sockaddr) );

    if( i < 0 || i != cc )  {
        if( i<0 ) {
			struct sockaddr_in *to = (struct sockaddr_in *) &whereto;
			asprintf(&out, "ping: sendto: %s pinging %s\n", strerror(errno), inet_ntoa(to->sin_addr));
            output = append_buffer(output, out);
        }

        asprintf(&out, "ping: wrote %s %d chars, ret=%d\n",
            hostname, cc, i );
        output = append_buffer(output, out);
    }
}

/*
 * 			P R _ T Y P E
 *
 * Convert an ICMP "type" field to a printable string.
 */
static char *
pr_type( t )
register int t;
{
	static char *ttab[] = {
		"Echo Reply",
		"ICMP 1",
		"ICMP 2",
		"Dest Unreachable",
		"Source Quench",
		"Redirect",
		"ICMP 6",
		"ICMP 7",
		"Echo",
		"ICMP 9",
		"ICMP 10",
		"Time Exceeded",
		"Parameter Problem",
		"Timestamp",
		"Timestamp Reply",
		"Info Request",
		"Info Reply"
	};

	if( t < 0 || t > 16 )
		return("OUT-OF-RANGE");

	return(ttab[t]);
}

/*
 *			P R _ P A C K
 *
 * Print out the packet, if it came from us.  This logic is necessary
 * because ALL readers of the ICMP socket get a copy of ALL ICMP packets
 * which arrive ('tis only fair).  This permits multiple copies of this
 * program to be run without having intermingled output (or statistics!).
 */
static void pr_pack( buf, cc, from )
unsigned char *buf;
int cc;
struct sockaddr_in *from;
{
	struct ip *ip;
	register struct icmp *icp;
	register long *lp = (long *) packet;
	register int i;
	struct timeval tv;
	struct timeval *tp;
	int hlen, triptime;
	char *out;

	/* ML from->sin_addr.s_addr = from->sin_addr.s_addr;   */
	gettimeofday( &tv, &tz );

	ip = (struct ip *) buf;
	hlen = ip->ip_hl << 2;
	if (cc < hlen + ICMP_MINLEN) {
		if (pingflags & VERBOSE) {
			asprintf(&out, "packet too short (%d bytes) from %s\n", cc,
				inet_ntoa(from->sin_addr)); /* DFM */ /*ML was ntohl(from->sin_addr) */
			output = append_buffer(output, out);
		}
		return;
	}
	cc -= hlen;
	icp = (struct icmp *)(buf + hlen);
	if( (!(pingflags & QUIET)) && icp->icmp_type != ICMP_ECHOREPLY )  {
		asprintf(&out, "%d bytes from %s: icmp_type=%d (%s) icmp_code=%d\n",
		  cc, inet_ntoa(from->sin_addr), /*ML was: inet_ntoa(ntohl(from->sin_addr)) */
		  icp->icmp_type, pr_type(icp->icmp_type), icp->icmp_code);/*DFM*/
		output = append_buffer(output, out);

		if (pingflags & VERBOSE) {
			for( i=0; i<12; i++) {
				asprintf(&out, "x%2.2lx: x%8.8lx\n", (long)sizeof(long)*i,*lp++);
				output = append_buffer(output, out);
			}
		}
		return;
	}
	if( icp->icmp_id != ident )
		return;			/* 'Twas not our ECHO */

	if (timing) {
		tp = (struct timeval *)&icp->icmp_data[0];
		tvsub( &tv, tp );
		triptime = tv.tv_sec*1000+(tv.tv_usec/1000);
		tsum += triptime;
		if( triptime < tmin )
			tmin = triptime;
		if( triptime > tmax )
			tmax = triptime;
	}

	if(!(pingflags & QUIET)) {
		if(pingflags != FLOOD) {
			asprintf(&out, "%d bytes from %s: icmp_seq=%d", cc,
			  inet_ntoa(from->sin_addr),
			  icp->icmp_seq );	/* DFM */
			output = append_buffer(output, out);

			if (timing) {
				asprintf(&out, " time=%d ms\n", triptime );
				output = append_buffer(output, out);
			}
			else {
				/*putchar('\n');*/				
				output = append_buffer(output, strdup("\n"));
			}
		} else {
			/*putchar('\b');*/
			output = append_buffer(output, strdup("\b"));			
			/*fflush(stdout);*/
		}
	}
	nreceived++;
}


/*
 *			I N _ C K S U M
 *
 * Checksum routine for Internet Protocol family headers (C Version)
 *
 */
static int in_cksum(addr, len)
u_short *addr;
int len;
{
	register int nleft = len;
	register u_short *w = addr;
	register u_short answer;
	register int sum = 0;

	/*
	 *  Our algorithm is simple, using a 32 bit accumulator (sum),
	 *  we add sequential 16 bit words to it, and at the end, fold
	 *  back all the carry bits from the top 16 bits into the lower
	 *  16 bits.
	 */
	while( nleft > 1 )  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if( nleft == 1 ) {
		u_short	u = 0;

		*(u_char *)(&u) = *(u_char *)w ;
		sum += u;
	}

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return answer;
}

/*
 * 			T V S U B
 * 
 * Subtract 2 timeval structs:  out = out - in.
 * 
 * Out is assumed to be >= in.
 */
static void tvsub( out, in )
register struct timeval *out, *in;
{
	if( (out->tv_usec -= in->tv_usec) < 0 )   {
		out->tv_sec--;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}

/*
 *			F I N I S H
 *
 * Print out statistics, and give up.
 * Heavily buffered STDIO is used here, so that all the statistics
 * will be written with 1 sys-write call.  This is nice when more
 * than one copy of the program is running on a terminal;  it prevents
 * the statistics output from becomming intermingled.
 */
static void finish(int  sig)
{
    done = 1;
}

static void report()
{
	char *out;
    
	asprintf(&out, "\n\n----%s PING Statistics----\n" 
        "%d packets transmitted, " 
        "%d packets received, ", 
        hostname, ntransmitted, nreceived );
	output = append_buffer(output, out);
	if (ntransmitted) {
		if( nreceived > ntransmitted) {
			asprintf(&out, "%s", "-- somebody's printing up packets!\n");
			output = append_buffer(output, out);
		}
		else {
			int loss = (((ntransmitted-nreceived)*100) / ntransmitted);
			asprintf(&out, "%d packet loss\n", loss);
			  
			output = append_buffer(output, out);
		}
	}

	if (nreceived && timing) {
	    asprintf(&out, "round-trip (ms)  min/avg/max = %d/%d/%d\n",
		tmin,
		tsum / nreceived,
		tmax );
		output = append_buffer(output, out);
	}
	return;
}

static void decode_byte(const char *buf, const char *key, unsigned char *val)
{
    unsigned char x = 0;
    char *pos;
    int i;
    for (i=0; i<2; i++)
    {
        pos = strchr(key, *buf++);
        if (!pos) return; /* unknown char within the string */
        x = x * 16 + (pos - key);
    }
    *val= x;
}

static char *hex_to_ip(char *str)
{
	if (strlen(str) == 8)
	{
	    const char *hex = "0123456789ABCDEF";
		char addr[16];
		unsigned char octet[4];
		char *p, *q;
		int j = 0;
		p = str;
		q = addr;
		for (j=0; j<4; j++)
			decode_byte(str + j*2, hex, octet + j);
		for (j=3; j>=0; j--) 
		{
			sprintf(q, "%d", octet[j]);
			if (j>0) 
			{ 
				q+= strlen(q);
				*q++ = '.';
			}
		}
		return strdup(addr);
	}
	return NULL;
}

EXPORT
char *plugin_func(symbol_table variables, char *buf, int buflen, int argc, char *argv[])
{
    char **new_params = malloc( sizeof(char *) * (argc+1));
    int i;
	output = NULL;
    for (i=0; i<argc; i++) {
        if (i == 0 || *argv[i] == '-')
            new_params[i] = strdup(argv[i]);
        else
        {
            new_params[i] = strdup(name_lookup(variables, argv[i]));
            if (matches(new_params[i], "[0-9A-F]{8}" ))
            {
                char *ipaddr = hex_to_ip(new_params[i]);
                if (ipaddr) 
                {
                    free(new_params[i]);
                    new_params[i] = ipaddr;
                }
            }
        }
    }
    new_params[argc] = 0;
    
	ping(argc, (const char **)new_params);
    
    for (i=0; i<argc; i++)
        free(new_params[i]);
    free(new_params);
	if (output)
	{
		if (buf)
		{
			int n = strlen(output);
			if (n >= buflen) n = buflen-1;
			strncpy(buf, output, n);
			buf[n] = 0;
			free(output);
		}
		else
			buf = output;
	}
	else if (buf)
		buf[0] = 0;
	return buf;	
}


#ifdef TEST_PLUGIN
int main(int argc, const char *argv[])
{
    int res = ping(argc, argv);
    if (output) 
    {
		printf("%s", output);
		free(output);
	}
    return res;
}
#endif


