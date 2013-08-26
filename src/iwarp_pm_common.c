/*
 * Copyright (c) 2013 Intel Corporation.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "iwarp_pm.h"

extern iwpm_client client_list[IWARP_PM_MAX_CLIENTS];

/**
 * create_iwpm_socket_v4 - Create an ipv4 socket for the iwarp port mapper
 * @bind_port: UDP port to bind the socket
 *
 * Return a handle of ipv4 socket
 */
int create_iwpm_socket_v4(__u16 bind_port)
{
	sockaddr_union bind_addr;
	struct sockaddr_in *bind_in4;
	int pm_sock;
	socklen_t sockname_len;
	char ip_address_text[INET6_ADDRSTRLEN];

	/* create a socket */
	pm_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (pm_sock < 0) {
		syslog(LOG_WARNING, "create_iwpm_socket_v4: Unable to create socket. %s.\n",
				strerror(errno));
		pm_sock = -errno;
		goto create_socket_v4_exit;
	}
	/* bind the socket to the given port */
	memset(&bind_addr, 0, sizeof(bind_addr));
	bind_in4 = &bind_addr.v4_sockaddr;
	bind_in4->sin_family = AF_INET;
	bind_in4->sin_addr.s_addr = INADDR_ANY;
	bind_in4->sin_port = htons(bind_port);

	if (bind(pm_sock, &bind_addr.sock_addr, sizeof(struct sockaddr_in))) {
		syslog(LOG_WARNING, "create_iwpm_socket_v4: Unable to bind socket (port = %u). %s.\n",
				bind_port, strerror(errno));
		close(pm_sock);
		pm_sock = -errno;
		goto create_socket_v4_exit;
	}

	/* get the socket name (local port number) */
	sockname_len = sizeof(struct sockaddr_in);
	if (getsockname(pm_sock, &bind_addr.sock_addr, &sockname_len)) {
		syslog(LOG_WARNING, "create_iwpm_socket_v4: Unable to get socket name. %s.\n", 
				strerror(errno));
		close(pm_sock);
		pm_sock = -errno;
		goto create_socket_v4_exit;
	}

	iwpm_debug(IWARP_PM_WIRE_DBG, "create_iwpm_socket_v4: Socket IP address:port %s:%u\n",
		inet_ntop(bind_in4->sin_family, &bind_in4->sin_addr.s_addr, ip_address_text,
			INET6_ADDRSTRLEN), ntohs(bind_in4->sin_port));
create_socket_v4_exit:
	return pm_sock;
}

/**
 * create_iwpm_socket_v6 - Create an ipv6 socket for the iwarp port mapper
 * @bind_port: UDP port to bind the socket
 *
 * Return a handle of ipv6 socket
 */
int create_iwpm_socket_v6(__u16 bind_port)
{
	sockaddr_union bind_addr;	
	struct sockaddr_in6 *bind_in6;
	int pm_sock, ret_value, ipv6_only;
	socklen_t sockname_len;
	char ip_address_text[INET6_ADDRSTRLEN];

	/* create a socket */
	pm_sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (pm_sock < 0) {
		syslog(LOG_WARNING, "create_iwpm_socket_v6: Unable to create socket. %s.\n", 
				strerror(errno));
		pm_sock = -errno;
		goto create_socket_v6_exit;
	}

	ipv6_only = 1;
	ret_value = setsockopt(pm_sock, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6_only, sizeof(ipv6_only));
	if (ret_value < 0) {
		syslog(LOG_WARNING, "create_iwpm_socket_v6: Unable to set sock options. %s.\n",
				strerror(errno));
		close(pm_sock);
		pm_sock = -errno;
		goto create_socket_v6_exit;
	}

	/* bind the socket to the given port */
	memset(&bind_addr, 0, sizeof(bind_addr));
	bind_in6 = &bind_addr.v6_sockaddr;
	bind_in6->sin6_family = AF_INET6;
	bind_in6->sin6_addr = in6addr_any;
	bind_in6->sin6_port = htons(bind_port);

	if (bind(pm_sock, &bind_addr.sock_addr, sizeof(struct sockaddr_in6))) {
		syslog(LOG_WARNING, "create_iwpm_socket_v6: Unable to bind socket (port = %u). %s.\n", 
				bind_port, strerror(errno));
		close(pm_sock);
		pm_sock = -errno;
		goto create_socket_v6_exit;
	}

	/* get the socket name (local port number) */
	sockname_len = sizeof(struct sockaddr_in6);
	if (getsockname(pm_sock, &bind_addr.sock_addr, &sockname_len)) {
		syslog(LOG_WARNING, "create_iwpm_socket_v6: Unable to get socket name. %s.\n",
				strerror(errno));
		close(pm_sock);
		pm_sock = -errno;
		goto create_socket_v6_exit;
	}

	iwpm_debug(IWARP_PM_WIRE_DBG, "create_iwpm_socket_v6: Socket IP address:port %s:%04X\n",
		inet_ntop(bind_in6->sin6_family, &bind_in6->sin6_addr, ip_address_text,
			INET6_ADDRSTRLEN), ntohs(bind_in6->sin6_port));
create_socket_v6_exit:
	return pm_sock;
}

/**
 * create_netlink_socket - Create netlink socket for the iwarp port mapper
 */
int create_netlink_socket()
{
	sockaddr_union bind_addr;
	struct sockaddr_nl *bind_nl;
	int nl_sock;

	/* create a socket */
	nl_sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_RDMA);
	if (nl_sock < 0) {
		syslog(LOG_WARNING, "create_netlink_socket: Unable to create socket. %s.\n",
				strerror(errno));
		nl_sock = -errno;
		goto create_nl_socket_exit;
	}

	/* bind the socket */
	memset(&bind_addr, 0, sizeof(bind_addr));
	bind_nl = &bind_addr.nl_sockaddr;
	bind_nl->nl_family = AF_NETLINK;
	bind_nl->nl_pid = getpid();
	bind_nl->nl_groups = 3; /* != 0 support multicast */

	if (bind(nl_sock, &bind_addr.sock_addr, sizeof(struct sockaddr_nl))) {
		syslog(LOG_WARNING, "create_netlink_socket: Unable to bind socket. %s.\n",
				strerror(errno));
		close(nl_sock);
		nl_sock = -errno;
	}

create_nl_socket_exit:
	return nl_sock;
}

/**
 * destroy_iwpm_socket - Close socket
 */
void destroy_iwpm_socket(int pm_sock)
{
	if (pm_sock > 0)
		close(pm_sock);
	pm_sock = -1;
}

/**
 * check_iwpm_nlattr - Check for NULL netlink attribute
 */ 
int check_iwpm_nlattr(struct nlattr *nltb[], int nla_count)
{ 
        int i, ret = 0;          
        for (i = 1; i < nla_count; i++) {
                if (!nltb[i]) { 
			iwpm_debug(IWARP_PM_NETLINK_DBG, "check_iwpm_nlattr: NULL (attr idx = %d)\n", i); 
			ret = -EINVAL;
		}
	}    
	return ret;
}      

/**
 * parse_iwpm_nlmsg - Parse a netlink message
 * @req_nlh: netlink header of the received message to parse
 * @policy_max: the number of attributes in the policy
 * @nlmsg_policy: the attribute policy
 * @nltb: array to store the parsed attributes
 * @msg_type: netlink message type (dbg purpose)
 */
int parse_iwpm_nlmsg(struct nlmsghdr *req_nlh, int policy_max,
			struct nla_policy *nlmsg_policy, struct nlattr *nltb [],
			const char *msg_type) 
{
	const char *str_err;
	int ret;
	
	if ((ret = nlmsg_validate(req_nlh, 0, policy_max-1, nlmsg_policy))) {
		str_err = "nlmsg_validate error";
		goto parse_nlmsg_error;
	}
	if ((ret = nlmsg_parse(req_nlh, 0, nltb, policy_max-1, nlmsg_policy))) {
		str_err = "nlmsg_parse error";
		goto parse_nlmsg_error;
	}
	if (check_iwpm_nlattr(nltb, policy_max)) {
		ret = -EINVAL;
		str_err = "NULL nlmsg attribute";
		goto parse_nlmsg_error;
	}
	return 0;
parse_nlmsg_error:
	syslog(LOG_WARNING, "parse_iwpm_nlmsg: msg type = %s (%s ret = %d)\n", 
			msg_type, str_err, ret);		
	return ret;
}

/**
 * send_iwpm_nlmsg - Send a netlink message
 * @nl_sock:  netlink socket to use for sending the message
 * @nlmsg:    netlink message to send
 * @dest_pid: pid of the destination of the nlmsg 
 */
int send_iwpm_nlmsg(int nl_sock, struct nl_msg *nlmsg, int dest_pid) 
{
	struct sockaddr_nl dest_addr;
	struct nlmsghdr *nlh = nlmsg_hdr(nlmsg);
	__u32 nlmsg_len = nlh->nlmsg_len;
	int len;

	/* fill in the netlink address of the client */
	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.nl_groups = 0;
	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pid = dest_pid;

	/* send response to the client */
	len = sendto(nl_sock, (char *)nlh, nlmsg_len, 0,
		     	(struct sockaddr *)&dest_addr, sizeof(dest_addr));
	if (len != nlmsg_len)
		return -errno;
	return 0;
}

/**
 * create_iwpm_nlmsg - Create a netlink message
 * @nlmsg_type: type of the netlink message
 * @client:     the port mapper client to receive the message
 */
struct nl_msg *create_iwpm_nlmsg(__u16 nlmsg_type, int client_idx)
{
	struct nl_msg *nlmsg;
	struct nlmsghdr *nlh;
	__u32 seq = 0;

	nlmsg = nlmsg_alloc();
	if (!nlmsg) 
		return NULL;	
	if (client_idx > 0)
		seq = client_list[client_idx].nl_seq++;
	
	nlh = nlmsg_put(nlmsg, getpid(), seq, nlmsg_type, 0, NLM_F_REQUEST);
	if (!nlh) {
		nlmsg_free(nlmsg);
		return NULL;	
	}
	return nlmsg;
}

/**
 * parse_iwpm_msg - Parse iwarp port mapper wire message
 * @pm_msg: iwpm message to be parsed
 * @msg_parms: contains the parameters of the iwpm message after parsing
 */ 
int parse_iwpm_msg(iwpm_wire_msg *pm_msg, iwpm_msg_parms *msg_parms)
{
	int ret_value = 0;

	msg_parms->pmtime = pm_msg->pmtime;
	msg_parms->assochandle = __be64_to_cpu(pm_msg->assochandle);
	msg_parms->ip_ver = (pm_msg->magic & IWARP_PM_IPVER_MASK) >> IWARP_PM_IPVER_SHIFT;
	switch (msg_parms->ip_ver) {
	case 4:
		msg_parms->address_family = AF_INET;
		break;
	case 6:
		msg_parms->address_family = AF_INET6;
		break;
	default:
		syslog(LOG_WARNING, "parse_iwpm_msg: Invalid IP version = %d.\n", 
			msg_parms->ip_ver);
		return -EINVAL;
	}
	/* port mapper protocol version */
	msg_parms->ver = (pm_msg->magic & IWARP_PM_VER_MASK) >> IWARP_PM_VER_SHIFT;
	/* message type */
	msg_parms->mt = (pm_msg->magic & IWARP_PM_MT_MASK) >> IWARP_PM_MT_SHIFT;
	msg_parms->apport = pm_msg->apport; /* accepting peer port */
	msg_parms->cpport = pm_msg->cpport; /* connecting peer port */
	/* copy accepting peer IP address */
	memcpy(&msg_parms->apipaddr, &pm_msg->apipaddr, IWARP_PM_IPADDR_LENGTH);
	/* copy connecting peer IP address */
	memcpy(&msg_parms->cpipaddr, &pm_msg->cpipaddr, IWARP_PM_IPADDR_LENGTH);

	return ret_value;
}

/**
 * form_iwpm_msg - Form iwarp port mapper wire message
 * @pm_msg: iwpm message to be formed
 * @msg_parms: the parameters to be packed in a iwpm message
 */ 
static void form_iwpm_msg(iwpm_wire_msg *pm_msg, iwpm_msg_parms *msg_parms)
{
	memset(pm_msg, 0, sizeof(struct iwpm_wire_msg));
	pm_msg->pmtime = msg_parms->pmtime;
	pm_msg->assochandle = __cpu_to_be64(msg_parms->assochandle);
	/* record IP version, port mapper version, message type */
	pm_msg->magic = (msg_parms->ip_ver << IWARP_PM_IPVER_SHIFT) & IWARP_PM_IPVER_MASK;
	pm_msg->magic |= (msg_parms->ver << IWARP_PM_VER_SHIFT) & IWARP_PM_VER_MASK;
	pm_msg->magic |= (msg_parms->mt << IWARP_PM_MT_SHIFT) & IWARP_PM_MT_MASK;

	pm_msg->apport = msg_parms->apport;
	pm_msg->cpport = msg_parms->cpport;
	memcpy(&pm_msg->apipaddr, &msg_parms->apipaddr, IWARP_PM_IPADDR_LENGTH);
	memcpy(&pm_msg->cpipaddr, &msg_parms->cpipaddr, IWARP_PM_IPADDR_LENGTH);
}

/**
 * form_iwpm_request - Form iwarp port mapper request message
 * @pm_msg: iwpm message to be formed
 * @msg_parms: the parameters to be packed in a iwpm message
 **/
void form_iwpm_request(struct iwpm_wire_msg *pm_msg,
		      struct iwpm_msg_parms  *msg_parms)
{
	msg_parms->mt = IWARP_PM_MT_REQ;
	form_iwpm_msg(pm_msg, msg_parms);
}

/**
 * form_iwpm_accept - Form iwarp port mapper accept message
 * @pm_msg: iwpm message to be formed
 * @msg_parms: the parameters to be packed in a iwpm message
 **/
void form_iwpm_accept(struct iwpm_wire_msg *pm_msg,
		     struct iwpm_msg_parms  *msg_parms)
{
	msg_parms->mt = IWARP_PM_MT_ACC;
	form_iwpm_msg(pm_msg, msg_parms);
}

/**
 * form_iwpm_ack - Form iwarp port mapper ack message
 * @pm_msg: iwpm message to be formed
 * @msg_parms: the parameters to be packed in a iwpm message
 **/
void form_iwpm_ack(struct iwpm_wire_msg *pm_msg,
		  struct iwpm_msg_parms  *msg_parms)
{
	msg_parms->mt = IWARP_PM_MT_ACK;
	form_iwpm_msg(pm_msg, msg_parms);
}

/**
 * form_iwpm_reject - Form iwarp port mapper reject message
 * @pm_msg: iwpm message to be formed
 * @msg_parms: the parameters to be packed in a iwpm message
 */
void form_iwpm_reject(struct iwpm_wire_msg *pm_msg,
		     struct iwpm_msg_parms  *msg_parms)
{
	msg_parms->mt = IWARP_PM_MT_REJ;
	form_iwpm_msg(pm_msg, msg_parms);
}

/**
 * copy_iwpm_sockaddr - Copy (IP address and Port) from src to dst 
 * @address_family: Internet address family
 * @src_sockaddr: socket address to copy (if NULL, use src_addr) 
 * @dst_sockaddr: socket address to update (if NULL, use dst_addr)
 * @src_addr: IP address to copy (if NULL, use src_sockaddr)
 * @dst_addr: IP address to update (if NULL, use dst_sockaddr)  
 * @src_port: port to copy in dst_sockaddr, if src_sockaddr = NULL
 * 	      port to update, if src_sockaddr != NULL and dst_sockaddr = NULL	
 */ 
void copy_iwpm_sockaddr(__u16 addr_family, struct sockaddr_storage *src_sockaddr,
		      struct sockaddr_storage *dst_sockaddr,
		      char *src_addr, char *dst_addr, __be16 *src_port)
{
	char *src = NULL, *dst = NULL;

	if (src_addr)
		src = src_addr;
	if (dst_addr)
		dst = dst_addr;

	switch (addr_family) {
	case AF_INET:
		if (src_sockaddr) {
			src = (char *)&((struct sockaddr_in *)src_sockaddr)->sin_addr.s_addr;
			*src_port = ((struct sockaddr_in *)src_sockaddr)->sin_port;
		}
		if (dst_sockaddr) {
			dst = (char *)&(((struct sockaddr_in *)dst_sockaddr)->sin_addr.s_addr);
			((struct sockaddr_in *)dst_sockaddr)->sin_port = *src_port;
			((struct sockaddr_in *)dst_sockaddr)->sin_family = AF_INET;
		}
		break;
	case AF_INET6:
		if (src_sockaddr) {
			src = (char *)&((struct sockaddr_in6 *)src_sockaddr)->sin6_addr.s6_addr;
			*src_port = ((struct sockaddr_in6 *)src_sockaddr)->sin6_port;
		}
		if (dst_sockaddr) {
			dst = (char *)&(((struct sockaddr_in6 *)dst_sockaddr)->sin6_addr.s6_addr);
			((struct sockaddr_in6 *)dst_sockaddr)->sin6_port = *src_port;
			((struct sockaddr_in6 *)dst_sockaddr)->sin6_family = AF_INET6;
		}
		break;
	default:
		return;
	}

	memcpy(dst, src, sizeof(IWPM_IPADDR_SIZE));
}

/**
 * print_iwpm_sockaddr - Print socket address (IP address and Port)
 * @sockaddr: socket address to print
 * @msg: message to print
 */ 
void print_iwpm_sockaddr(struct sockaddr_storage *sockaddr, char *msg)
{
	struct sockaddr_in6 *sockaddr_v6;
	struct sockaddr_in *sockaddr_v4;
	char ip_address_text[INET6_ADDRSTRLEN];

	switch (sockaddr->ss_family) {
	case AF_INET:
		sockaddr_v4 = (struct sockaddr_in *)sockaddr;
		iwpm_debug(IWARP_PM_ALL_DBG, "%s IPV4 %s:%u(0x%04X)\n", msg,
			inet_ntop(AF_INET, &sockaddr_v4->sin_addr, ip_address_text, INET6_ADDRSTRLEN),
			ntohs(sockaddr_v4->sin_port), ntohs(sockaddr_v4->sin_port));
		break;
	case AF_INET6:
		sockaddr_v6 = (struct sockaddr_in6 *)sockaddr;
		iwpm_debug(IWARP_PM_ALL_DBG, "%s IPV6 %s:%u(0x%04X)\n", msg,
			inet_ntop(AF_INET6, &sockaddr_v6->sin6_addr, ip_address_text, INET6_ADDRSTRLEN),
			ntohs(sockaddr_v6->sin6_port), ntohs(sockaddr_v6->sin6_port));
		break;
	default:
		break;
	}
}
