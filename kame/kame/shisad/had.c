/*	$KAME: had.c,v 1.20 2005/07/13 01:45:31 keiichi Exp $	*/

/*
 * Copyright (C) 2004 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <poll.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <ifaddrs.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#ifdef __FreeBSD__
#include <net/if_var.h>
#endif
#include <netinet/in.h>
#include <netinet/icmp6.h>
#include <netinet/ip6mh.h>
#include <netinet6/in6_var.h>
#include <net/mipsock.h>
#include <netinet6/mip6.h>

#include "callout.h"
#include "stat.h"
#include "shisad.h"
#include "fdlist.h"
#include "command.h"
#include "config.h"

/* Global Variables */
int mipsock, icmp6sock, mhsock;

/* configuration parameters */
struct config_entry *if_params = NULL;
int debug = 0;
int foreground = 0;
int namelookup = 1;
int command_port = HAD_COMMAND_PORT;
int preference = 0;

struct mip6stat mip6stat;
struct mip6_hpfx_list hpfx_head; 
#ifdef MIP_NEMO
struct nemo_hpt_list hpt_head;
#endif /* MIP_NEMO */

struct ha_ifinfo {
	char hainfo_ifname[IFNAMSIZ];
	u_int16_t hainfo_ifindex;
	struct sockaddr_dl hainfo_sdl;
} haifinfo;
#define ha_ifname haifinfo.hainfo_ifname
#define ha_ifindex haifinfo.hainfo_ifindex 
#define ha_sdl haifinfo.hainfo_sdl

/* it indicates that entry having MIP6_HA_INIFITY_LIFE is mine */
#define MIP6_HA_INIFITY_LIFE 0xffff 

static void ha_lists_init(void);
static void had_init_homeprefix(char *, int);
static void terminate(int);
static void command_show_hal(int, char *);

struct command_table show_command_table[] = {
	{"bc", command_show_bc, "binding chache"},
	{"kbc", command_show_kbc, "binding chache in kernel"},
	{"stat", command_show_stat, "statisticts"},
	{"hal", command_show_hal, "Home Agent List"},
	{"callout", show_callout_table, "show callout table "},
	{NULL}
};

static void had_flush_bc(int, char *);
static void had_flush_stat(int, char *);
static void had_flush_hal(int, char *);

struct command_table flush_command[] = {
	{"bc", had_flush_bc, "Clear bc"},
	{"stat", had_flush_stat, "clear stat"},
	{"hal", had_flush_hal, "clear home agent list"},
	{NULL}
};

struct command_table command_table[] = {
	{"show", NULL, "Show status", show_command_table},
	{"flush", NULL, "Flush stat, bc, hal", flush_command},
};

void
ha_usage(path)
	char *path;
{
	char *cmd;

	cmd = strrchr(path, '/');
	if (!cmd)
		cmd = path;
	else
		cmd++;
	fprintf(stderr, "%s [-fn] [-c configfile] ifname\n", cmd);
} 

int
main(argc, argv)
	int argc;
	char **argv;
{
	int pfds;
	int ch = 0;
	char *ifname;
	FILE *pidfp;
	char *conffile = HAD_CONFFILE;
	char *options = "fnc:";

        if (argc < 1) {
		ha_usage(argv[0]);
		exit (0);
	}

	/* get options */
	ifname = NULL;
	while ((ch = getopt(argc, argv, options)) != -1) {
		switch (ch) {
		case 'f':
			foreground = 1;
			break;
		case 'n':
			namelookup = 0;
			break;
		case 'c':
			conffile = optarg;
			break;
		default:
			fprintf(stderr, "unknown option\n");
			ha_usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argv == NULL || ((ifname = *argv) == NULL)) {
		fprintf(stderr, "you have to specify interfacename\n"); 
		exit(1);
	}

	/* open syslog infomation. */
	openlog("shisad(had)", 0, LOG_DAEMON);
	syslog(LOG_INFO, "Start HA daemon at %s\n", ifname);

	/* parse configuration file and set default values. */
	if (parse_config(CFM_HAD, conffile) == 0)
		config_get_interface(ifname, &if_params, config_params);
	if (if_params != NULL) {
		config_get_number(CFT_DEBUG, &debug, if_params);
		config_get_number(CFT_COMMANDPORT, &command_port, if_params);
		config_get_number(CFT_PREFERENCE, &preference, if_params);
	}
	if (config_params != NULL) {
		config_get_number(CFT_DEBUG, &debug, config_params);
		config_get_number(CFT_COMMANDPORT, &command_port,
		    config_params);
		config_get_number(CFT_PREFERENCE, &preference, config_params);
	}

	/* start timer */
	callout_init();

	/* Various Initialization */
	fdlist_init();
	command_init("ha> ", command_table,
		sizeof(command_table) / sizeof(struct command_table), 7778);

	/* register signal handlers. */
	signal(SIGTERM, terminate);
	signal(SIGINT, terminate);

	/* open sockets */
	mhsock_open();
	icmp6sock_open();
	mipsock_open();

	new_fd_list(mipsock, POLLIN, mipsock_input_common);
	new_fd_list(mhsock, POLLIN, mh_input_common);
	new_fd_list(icmp6sock, POLLIN, icmp6_input_common);

	/* initialization */
	ha_lists_init();
	had_init_homeprefix(ifname, preference);
#ifdef MIP_NEMO
	nemo_parse_conf();
#endif /*MIP_NEMO*/

	mip6_bc_init();

	/* notify a kernel to behave as a home agent. */
	mipsock_nodetype_request(MIP6_NODETYPE_HOME_AGENT, 1);

	if (foreground == 0)
		daemon(0, 0);

	/* dump current PID */
	if ((pidfp = fopen(HAD_PIDFILE, "w")) != NULL) {
		fprintf(pidfp, "%d\n", getpid());
		fclose(pidfp);
	}

	while (1) {
		clear_revents();
	    
		if ((pfds = poll(fdl_fds, fdl_nfds, get_next_timeout())) < 0) {
			perror("poll");
			continue;
		}
		
		if (pfds != 0) {
			dispatch_fdfunctions(fdl_fds, fdl_nfds);
		}
		/* Timeout */
		callout_expire_check();
	}

	return (0);
}

static void
ha_lists_init()
{
	LIST_INIT(&hpfx_head);
	return;
}

int
mipsock_input(miphdr)
	struct mip_msghdr *miphdr;
{
	int err = 0;

	switch (miphdr->miph_type) {
	case MIPM_BE_HINT:
		mipsock_behint_input(miphdr);
		break;
	default:
		break;
	}

	return (err);
}

int
had_is_ha_if(ifindex)
	u_int16_t ifindex;
{
	if (ifindex == ha_ifindex)
		return (1);
	
	return (0);
};

struct mip6_hpfxl *
had_is_myhomenet(hoa)
	struct in6_addr *hoa;
{
	struct mip6_hpfxl *hpfx = NULL;

	LIST_FOREACH(hpfx, &hpfx_head, hpfx_entry) {
		if (mip6_are_prefix_equal(&hpfx->hpfx_prefix, hoa,  hpfx->hpfx_prefixlen))
			return (hpfx);
	}

	return (NULL);
}


static void
had_init_homeprefix (ifname, preference)
	char *ifname;
	int preference;
{
        size_t needed;
        char *buf, *next, name[32];
        struct if_msghdr *ifm;
        struct sockaddr_dl *sdl;
	int mib[6];
	struct ifaddrs *ifa, *ifap;
	struct sockaddr *sa;
	struct sockaddr_in6 *addr_sin6, *mask_sin6;
	int prefixlen = 0;
	struct mip6_hpfxl *hpfxent = NULL;
	struct home_agent_list *hal = NULL;

	memset(&haifinfo, 0, sizeof(haifinfo));
	
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET6;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;
	
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
                perror("sysctl");
		return;
	}

        if ((buf = malloc(needed)) == NULL) {
                perror("malloc");
		return;
	}

        if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
                perror("sysctl");
		free(buf);
		return;
	}

        for (next = buf; next < buf + needed; 
	     next += ifm->ifm_msglen) {
                ifm = (struct if_msghdr *)next;

		if (ifm->ifm_type != RTM_IFINFO) 
			continue;

		sdl = (struct sockaddr_dl *)(ifm + 1);
		bzero(name, sizeof(name));
		strncpy(name, &sdl->sdl_data[0], sdl->sdl_nlen);
		
		if (strncmp(name, ifname, strlen(ifname)) == 0) {
			ha_ifindex = sdl->sdl_index;
			strncpy(ha_ifname, name, strlen(name));
			memcpy(&ha_sdl, sdl, sizeof(struct sockaddr_dl));
			break;
		}
        }
	free(buf); 

	if (ha_ifindex == 0) 
		return; 
		
	if (getifaddrs(&ifap) != 0) {
		perror("getifaddrs");
		return;
	}
	
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        	struct in6_ifreq ifreq6;
		int ioctl_s;

		sa = ifa->ifa_addr;
		
		if (sa->sa_family != AF_INET6)
			continue;

		if (!(ifa->ifa_flags & IFF_UP)) 
			continue;

		/* retrieve flags for the ifa addr */
                if((ioctl_s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
			perror("socket\n");
			freeifaddrs(ifap);
			return;
		}
		memset(&ifreq6, 0, sizeof(ifreq6));
		(void)strncpy(ifreq6.ifr_name, ifname, strlen(ifname));
		memcpy(&ifreq6.ifr_addr, ifa->ifa_addr, ifa->ifa_addr->sa_len);

		if(ioctl(ioctl_s, SIOCGIFAFLAG_IN6, (caddr_t)&ifreq6) < 0) {
			close(ioctl_s);
			continue;
		}
		close(ioctl_s);

		/* dont use anycast addresses as home agent address */
		if (ifreq6.ifr_ifru.ifru_flags6 & IN6_IFF_ANYCAST) 
			continue;
		
		if (strncmp(ha_ifname, ifa->ifa_name, 
			    strlen(ifa->ifa_name)) != 0) 
			continue;
		
		addr_sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
		mask_sin6 = (struct sockaddr_in6 *)ifa->ifa_netmask;
		
		if (IN6_IS_ADDR_LINKLOCAL(&addr_sin6->sin6_addr))
			continue;
		
		prefixlen = in6_mask2len(&mask_sin6->sin6_addr, NULL);

		hpfxent = mip6_get_hpfxlist(&addr_sin6->sin6_addr, prefixlen, &hpfx_head);
		if (hpfxent == NULL) {
			hpfxent = had_add_hpfxlist(&addr_sin6->sin6_addr, prefixlen);
			if (hpfxent == NULL) {
				syslog(LOG_INFO, 
				       "unknown errors, check interface configuration");
				break;
			}
		}

		hal = had_add_hal(hpfxent, &addr_sin6->sin6_addr,NULL, MIP6_HA_INIFITY_LIFE, preference, MIP6_HAL_OWN);
		if (hal == NULL) {
			syslog(LOG_INFO, 
			       "unknown errors homeagentlist, check interface configuration");
			break;
		}
	}
	
	freeifaddrs(ifap);
	
	if (LIST_EMPTY(&hpfx_head)) {
		syslog(LOG_ERR, "please configure at least one global home prefix at %s\n", 
		       ifname); 
		exit(0);
	}
	return;
}

struct home_agent_list *
had_add_hal(hpfx_entry, gladdr, lladdr, lifetime, preference, flag) 
	struct  mip6_hpfxl *hpfx_entry;
	struct in6_addr *gladdr;
	struct in6_addr *lladdr;
	uint16_t lifetime;
	uint16_t preference;
	int flag;
{
	struct home_agent_list *hal = NULL, *h;

	hal = mip6_get_hal(hpfx_entry, gladdr);
	if (hal && ((hal->hal_preference != preference))) {
		/* if preference is changed, need to re-arrange order of hal */
		mip6_delete_hal(hpfx_entry, gladdr);
		hal = NULL;
	}

	if (hal == NULL) {
		hal = malloc(sizeof(*hal));
		if (hal == NULL)
			return (NULL);
		memset(hal, 0, sizeof(*hal));

		if (LIST_EMPTY(&hpfx_entry->hpfx_hal_head))  {
			LIST_INSERT_HEAD(&hpfx_entry->hpfx_hal_head, hal, hal_entry);
		} else {
			LIST_FOREACH(h, &hpfx_entry->hpfx_hal_head, hal_entry) {
				if (preference >= h->hal_preference) {
					LIST_INSERT_BEFORE(h, hal, hal_entry);
					break;
				} else if (LIST_NEXT(h, hal_entry) == NULL) {
					LIST_INSERT_AFTER(h, hal, hal_entry);
					break;
				}
			}
		}
	}

	hal->hal_ip6addr = *gladdr;
	if (lladdr)
		hal->hal_lladdr = *lladdr;
	hal->hal_lifetime = lifetime;
	hal->hal_preference = preference;
	hal->hal_flag = flag;

	if (hal->hal_expire)
		update_callout_entry(hal->hal_expire, hal->hal_lifetime);
	else if (hal->hal_flag != MIP6_HAL_OWN)
		hal_set_expire_timer(hal, hal->hal_lifetime);

	if (debug)
		syslog(LOG_INFO, "Home Agent (%s, %d %d) added into home agent list\n", 
		       ip6_sprintf(gladdr), lifetime, preference);
		
	return (hal);
}

struct mip6_hpfxl *
had_add_hpfxlist(home_prefix, home_prefixlen) 
	struct in6_addr *home_prefix;
	u_int16_t home_prefixlen;
{
	struct mip6_hpfxl *hpfx = NULL;

	hpfx = mip6_get_hpfxlist(home_prefix, home_prefixlen, &hpfx_head);
	if (hpfx)
		return (hpfx);

	hpfx = malloc(sizeof(*hpfx));
	if (hpfx == NULL)
		return (NULL);
	memset(hpfx, 0, sizeof(*hpfx));

	hpfx->hpfx_prefix = *home_prefix;
	hpfx->hpfx_prefixlen = home_prefixlen;
	LIST_INIT(&hpfx->hpfx_hal_head);

	if (debug)
		syslog(LOG_INFO, "Home Prefix (%s/%d) added into home prefix list\n", 
		       ip6_sprintf(home_prefix), home_prefixlen);
	
	LIST_INSERT_HEAD(&hpfx_head, hpfx, hpfx_entry);
	return (hpfx);
}

int
send_haadrep(dst, anycastaddr, dhreq, ifindex) 
	struct in6_addr *dst;
	struct in6_addr *anycastaddr;
	struct mip6_dhaad_req *dhreq;
	u_short ifindex;
{
	int src_decided = 0;
        struct msghdr msg;
        struct iovec iov;
        struct cmsghdr  *cmsgptr = NULL;
        struct in6_pktinfo *pi = NULL;
        struct sockaddr_in6 to;
        char adata[512], buf[1024];
	struct mip6_dhaad_rep *dhrep;
	struct mip6_hpfxl *hpfx = NULL;
	int reqlen = 0;
        struct home_agent_list *hal = NULL;

        memset(&to, 0, sizeof(to));
	memcpy(&to.sin6_addr, dst, sizeof(struct in6_addr)); /* fill the prefix part */
	to.sin6_family = AF_INET6;
	to.sin6_port = 0;
	to.sin6_scope_id = 0;
	to.sin6_len = sizeof (struct sockaddr_in6);

        msg.msg_name = (void *)&to;
        msg.msg_namelen = sizeof(struct sockaddr_in6);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = (void *) adata;
        msg.msg_controllen = CMSG_SPACE(sizeof(struct in6_pktinfo));

	cmsgptr = CMSG_FIRSTHDR(&msg);
	pi = (struct in6_pktinfo *)(CMSG_DATA(cmsgptr));
	memset(pi, 0, sizeof(*pi));
	if (ifindex)
		pi->ipi6_ifindex = ifindex;
	else
		return (-1);
	cmsgptr->cmsg_level = IPPROTO_IPV6;
	cmsgptr->cmsg_type = IPV6_PKTINFO;
	cmsgptr->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
	cmsgptr = CMSG_NXTHDR(&msg, cmsgptr);
		
	bzero(buf, sizeof(buf));
	dhrep = (struct mip6_dhaad_rep *)buf; 
		
	dhrep->mip6_dhrep_type = MIP6_HA_DISCOVERY_REPLY;
	dhrep->mip6_dhrep_code = 0;
	dhrep->mip6_dhrep_cksum = 0;
	dhrep->mip6_dhrep_id = dhreq->mip6_dhreq_id;
#ifdef MIP_NEMO
	dhrep->mip6_dhrep_reserved |= MIP6_DHREP_FLAG_MR;
#endif

	hpfx = mip6_get_hpfxlist(anycastaddr, 64 /* XXX */, &hpfx_head);
	if (hpfx == NULL) {
		if (debug)
			syslog(LOG_INFO, 
			       "no matched home prefix list is found, drop dhaad request\n");
		return (0);
	}

	reqlen = sizeof(struct mip6_dhaad_rep);

	LIST_FOREACH(hal, &hpfx->hpfx_hal_head, hal_entry) {
		if (reqlen + sizeof(struct in6_addr) >= sizeof(buf)) {
			syslog(LOG_INFO,
			       "adding %s into DHAAD reply was missed\n",
			       ip6_sprintf(&hal->hal_ip6addr));
			break;	/* no more space */
		}

		syslog(LOG_INFO, "add %s into DHAAD reply \n", ip6_sprintf(&hal->hal_ip6addr));
		memcpy((buf + reqlen), &hal->hal_ip6addr, sizeof(struct in6_addr));

		if ((hal->hal_flag == MIP6_HAL_OWN) && !src_decided) {
			pi->ipi6_addr = hal->hal_ip6addr;
			syslog(LOG_INFO, "Src addr was deceided as [%s]\n",
			       ip6_sprintf(&pi->ipi6_addr));
			src_decided = 1;
		}
		reqlen += sizeof(struct in6_addr);
	}
	
	iov.iov_base = buf;
	iov.iov_len = reqlen;
	
	if (debug) 
		syslog(LOG_INFO, "send DHAAD reply to %s\n", ip6_sprintf(dst));

	if (sendmsg(icmp6sock, &msg, 0) < 0)
		perror ("sendmsg icmp6 @haadreply");
	else
		mip6stat.mip6s_odhreply++;

	return (errno);
}

static void
had_flush_bc(s, line)
	int s;
	char *line;
{
	command_printf(s, "-- Clear Binding Cache --\n");
	/*flush_bc();*/
	mip6_flush_kernel_bc();
}

static void
had_flush_stat(s, line)
	int s;
	char *line;
{
	command_printf(s, "-- Clear Shisa Statistics --\n");
	command_printf(s, "Not implemented yet\n");
}

static void
had_flush_hal(s, line)
	int s;
	char *line;
{
	command_printf(s, "-- Clear Home Agent List --\n");
	command_printf(s, "Not implemented yet\n");
}

static void
command_show_hal(s, dummy)
	int s;
	char *dummy;
{
	show_hal(s, &hpfx_head);
}

static void
terminate(dummy)
	int dummy;
{
	mip6_flush_kernel_bc();
	mipsock_nodetype_request(MIP6_NODETYPE_HOME_AGENT, 0);
	unlink(HAD_PIDFILE);
	exit(1);
}

int
send_mpa(dst, mps_id, ifindex)
	struct in6_addr *dst;
	u_int16_t mps_id;
	u_short ifindex;
{
	struct nd_opt_prefix_info *ndopt_pi;

        struct msghdr msg;
        struct iovec iov;
        struct cmsghdr  *cmsgptr = NULL;
        struct in6_pktinfo *pi = NULL;
        struct sockaddr_in6 to;
        char adata[512], buf[1024];
        struct mip6_prefix_advert *mpa;
	struct mip6_hpfxl *hpfx = NULL;
	int reqlen = 0;

        memset(&to, 0, sizeof(to));
	memcpy(&to.sin6_addr, dst, sizeof(struct in6_addr));
	to.sin6_family = AF_INET6;
	to.sin6_port = 0;
	to.sin6_scope_id = 0;
	to.sin6_len = sizeof (struct sockaddr_in6);

        msg.msg_name = (void *)&to;
        msg.msg_namelen = sizeof(struct sockaddr_in6);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = (void *) adata;
        msg.msg_controllen = CMSG_SPACE(sizeof(struct in6_pktinfo));

	cmsgptr = CMSG_FIRSTHDR(&msg);
	pi = (struct in6_pktinfo *)(CMSG_DATA(cmsgptr));
	memset(pi, 0, sizeof(*pi));
	if (ifindex)
		pi->ipi6_ifindex = ifindex;
	else
		return (-1);
	cmsgptr->cmsg_level = IPPROTO_IPV6;
	cmsgptr->cmsg_type = IPV6_PKTINFO;
	cmsgptr->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
	cmsgptr = CMSG_NXTHDR(&msg, cmsgptr);
		
	bzero(buf, sizeof(buf));

	mpa = (struct mip6_prefix_advert *)buf;
	mpa->mip6_pa_type = MIP6_PREFIX_ADVERT;
	mpa->mip6_pa_code = 0;
	mpa->mip6_pa_cksum = 0;
	mpa->mip6_pa_id = mps_id;

	ndopt_pi = (struct nd_opt_prefix_info *)(mpa + 1);
	reqlen += sizeof(*mpa);
	LIST_FOREACH(hpfx, &hpfx_head, hpfx_entry) {
		ndopt_pi->nd_opt_pi_type = ND_OPT_PREFIX_INFORMATION;
		ndopt_pi->nd_opt_pi_len = 4;
		ndopt_pi->nd_opt_pi_prefix_len = hpfx->hpfx_prefixlen;
		ndopt_pi->nd_opt_pi_flags_reserved =
			ND_OPT_PI_FLAG_ONLINK | ND_OPT_PI_FLAG_AUTO;
		ndopt_pi->nd_opt_pi_valid_time = htonl(hpfx->hpfx_vltime);
		ndopt_pi->nd_opt_pi_preferred_time = htonl(hpfx->hpfx_pltime);
		ndopt_pi->nd_opt_pi_reserved2 = 0;
		memcpy(&ndopt_pi->nd_opt_pi_prefix, &hpfx->hpfx_prefix,
		       sizeof(struct in6_addr));

		reqlen += sizeof(struct nd_opt_prefix_info);
		ndopt_pi += sizeof(struct nd_opt_prefix_info);
	}

	iov.iov_base = buf;
	iov.iov_len = reqlen;
	
	if (debug) 
		syslog(LOG_INFO, "send MPA to %s\n", ip6_sprintf(dst));

	if (sendmsg(icmp6sock, &msg, 0) < 0)
		perror ("sendmsg icmp6 @haadreply");
	else
		mip6stat.mip6s_ompa++;

	return (errno);
}
