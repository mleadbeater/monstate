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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <net/if.h>
#include <arpa/inet.h>
#include "plugin.h"
#include "symboltable.h"
#include "buffers.h"

char *ipaddr(char *output, int argc, char *argv[])
{
	int sock;
	int err;
	struct ifconf if_info;
	int buf_len = sizeof(struct ifreq) * 40;
	struct ifreq *ifreqs = malloc(buf_len);
	char *bufp;
	char *out = NULL;

	sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sock < 0)
	{
		asprintf(&out,"socket: %s (%d)\n", strerror(errno), errno);
        output = append_buffer(output, out);
		return output;
	}
    memset(&if_info, 0, sizeof(struct ifconf));
    if_info.ifc_len = buf_len;
    if_info.ifc_buf = (char *) ifreqs;

    err = ioctl(sock, SIOCGIFCONF, &if_info);
    if (err == -1)
    {
        if (errno != EINVAL)
        {
            asprintf(&out,"ioctl: %s (%d)\n", strerror(errno), errno);
            output = append_buffer(output, out);
            return output;
        }
    }
/*
	if ( (char *)ifreqs != if_info.ifc_buf) 
    {
        asprintf(&out, "buffer changed\n");
        output = append_buffer(output, out);
    }
*/	
	bufp = if_info.ifc_buf;
	while (bufp - if_info.ifc_buf < if_info.ifc_len)
	{
		struct sockaddr_in *sa;
		struct ifreq *ifrp, *ifrp2;	  
		int len;
		char *ip4 = malloc(100); /* space to hold the IPv4 string struct sockaddr_in sa; */
		*ip4 = 0;

		ifrp = (struct ifreq *)bufp;
		sa = (struct sockaddr_in *) &(ifrp->ifr_addr);
#ifdef _SIZEOF_ADDR_IFREQ
		len = _SIZEOF_ADDR_IFREQ(*ifrp);
#else
	 	len = sizeof(struct ifreq);
		/*
			if (ifrp->ifr_addr.sa_len > sizeof(struct sockaddr))
				len = len + ifrp->ifr_addr.sa_len - sizeof(struct sockaddr);
		*/
#endif
/*		len = _SIZEOF_ADDR_IFREQ(*ifrp);*/
		ifrp2 = malloc(len);
		memcpy(ifrp2, bufp, len);

		if ((err = ioctl(sock, SIOCGIFADDR, ifrp2)) < 0)
		{
			if (errno != EADDRNOTAVAIL) {
				asprintf(&out, "ioctl error %d while getting interface address for %s\n", errno, ifrp2->ifr_name);
                output = append_buffer(output, out);
            }
		}
		else {
			strcpy(ip4, inet_ntoa(sa->sin_addr));
			inet_ntop(AF_INET, &(sa->sin_addr), ip4, INET_ADDRSTRLEN);
		}
		
		if ((err = ioctl(sock, SIOCGIFFLAGS, ifrp)) < 0) {
			asprintf(&out, "ioctl error while getting interface flags");
            output = append_buffer(output, out);
        }

		if ((ifrp->ifr_flags & IFF_UP) && ((ifrp->ifr_flags & IFF_LOOPBACK) == 0)) {
			asprintf(&out, "%s %s\n", (char *)&(ifrp->ifr_name), ip4);
			output = append_buffer(output, out);
      }
	free(ip4);
	free(ifrp2);

	bufp += len;
  }
  close(sock);
  free(ifreqs);
    
  return output;
}

EXPORT
char *plugin_func(symbol_table variables, char *buf, int buflen, int argc, char *argv[])
{
	char *output = strdup("");
	output = ipaddr(output, argc, argv);
	
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

#ifdef TESTING_PLUGIN
int main(int argc, char *argv[])
{
	char *output = strdup("");
	output = ipaddr(output, argc, argv);
	if (output)
	{
		printf("%s", output);
		free(output);
	}
	return 0;	
}
#endif
