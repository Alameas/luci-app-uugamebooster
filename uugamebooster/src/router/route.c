/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <ctype.h>
#include <libmnl/libmnl.h>
#include <libubox/ulog.h>
#include <linux/rtnetlink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>

#include "router.h"

struct route_query {
	unsigned int table;
	unsigned int interface;
};

static int route_attribute(const struct nlattr *attribute, void *data)
{
	const struct nlattr **attributes = data;
	unsigned int type = mnl_attr_get_type(attribute);

	if (type == RTA_TABLE || type == RTA_OIF) {
		if (mnl_attr_validate(attribute, MNL_TYPE_U32)) return MNL_CB_ERROR;
		attributes[type] = attribute;
	}
	return MNL_CB_OK;
}

static int read_route(const struct nlmsghdr *header, void *data)
{
	const struct nlattr *attributes[RTA_MAX + 1] = { 0 };
	struct route_query *query = data;
	const struct rtmsg *route;
	unsigned int table;

	if (mnl_nlmsg_get_payload_len(header) < sizeof(*route)) return MNL_CB_ERROR;
	route = mnl_nlmsg_get_payload(header);
	if (route->rtm_family != AF_INET || route->rtm_dst_len || route->rtm_type != RTN_UNICAST) {
		return MNL_CB_OK;
	}
	if (mnl_attr_parse(header, sizeof(*route), route_attribute, attributes) < 0) return MNL_CB_ERROR;
	table = attributes[RTA_TABLE] ? mnl_attr_get_u32(attributes[RTA_TABLE]) : route->rtm_table;
	if (table != query->table || !attributes[RTA_OIF]) return MNL_CB_OK;

	query->interface = mnl_attr_get_u32(attributes[RTA_OIF]);
	return MNL_CB_STOP;
}

static bool find_tunnel(unsigned int table, char *name)
{
	struct mnl_socket *socket = mnl_socket_open2(NETLINK_ROUTE, SOCK_CLOEXEC);
	struct route_query query = { .table = table };
	struct timeval timeout = { .tv_sec = 2 };
	char buffer[8192];
	struct nlmsghdr *header;
	struct rtmsg *route;
	unsigned int sequence = (unsigned int)time(NULL), port;
	int length, result;
	bool ok = false;

	if (!socket) return false;
	if (mnl_socket_bind(socket, 0, MNL_SOCKET_AUTOPID) ||
	    setsockopt(mnl_socket_get_fd(socket), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))) {
		goto out;
	}
	port = mnl_socket_get_portid(socket);
	header = mnl_nlmsg_put_header(buffer);
	header->nlmsg_type = RTM_GETROUTE;
	header->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	header->nlmsg_seq = sequence;
	route = mnl_nlmsg_put_extra_header(header, sizeof(*route));
	route->rtm_family = AF_INET;
	mnl_attr_put_u32(header, RTA_TABLE, table);
	if (mnl_socket_sendto(socket, header, header->nlmsg_len) < 0) goto out;

	while ((length = mnl_socket_recvfrom(socket, buffer, sizeof(buffer))) > 0) {
		result = mnl_cb_run(buffer, length, sequence, port, read_route, &query);
		if (result <= MNL_CB_STOP) {
			ok = result == MNL_CB_STOP && query.interface && if_indextoname(query.interface, name);
			break;
		}
	}
out:
	mnl_socket_close(socket);
	return ok;
}

static bool valid_tail(const char *tail)
{
	while (isspace((unsigned char)*tail)) tail++;
	return !*tail || !strcmp(tail, ">/dev/null 2>&1") || !strcmp(tail, "> /dev/null 2>&1");
}

int uu_router_route(const char *command, int (*execute)(const char *))
{
	const struct uu_router_device *router = uu_router_device_get();
	char action[8], source[20], tunnel[IF_NAMESIZE], *replacement;
	unsigned int table;
	int offset = 0, end = 0, result;
	bool marked, add;

	if (sscanf(command, "ip rule %7s from %19s %n", action, source, &offset) != 2 || !offset ||
	    strcmp(source, router->address) || (strcmp(action, "add") && strcmp(action, "del"))) {
		return -2;
	}
	marked = sscanf(command + offset, "fwmark %*31s lookup %u%n", &table, &end) == 1;
	if (!marked && sscanf(command + offset, "lookup %u%n", &table, &end) != 1) return -2;
	if (!end || !valid_tail(command + offset + end)) return -2;
	add = !strcmp(action, "add");

	if (asprintf(&replacement, "ip rule %s from %s iif %s %s", action,
		     router->network, router->interface, command + offset) < 0) {
		return 1 << 8;
	}
	if (marked && add && (!find_tunnel(table, tunnel) || !uu_router_nat(table, tunnel))) {
		ULOG_ERR("UU router: could not prepare shared NAT\n");
		free(replacement);
		return 1 << 8;
	}

	result = execute(replacement);
	free(replacement);
	if (marked && !add) uu_router_nat(table, NULL);
	return result;
}
