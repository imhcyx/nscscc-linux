/*
 * net/tipc/node.c: TIPC node management routines
 * 
 * Copyright (c) 2000-2006, Ericsson AB
 * Copyright (c) 2005, Wind River Systems
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "core.h"
#include "config.h"
#include "node.h"
#include "cluster.h"
#include "net.h"
#include "addr.h"
#include "node_subscr.h"
#include "link.h"
#include "port.h"
#include "bearer.h"
#include "name_distr.h"
#include "net.h"

void node_print(struct print_buf *buf, struct node *n_ptr, char *str);
static void node_lost_contact(struct node *n_ptr);
static void node_established_contact(struct node *n_ptr);

struct node *nodes = NULL;	/* sorted list of nodes within cluster */

u32 tipc_own_tag = 0;

struct node *node_create(u32 addr)
{
	struct cluster *c_ptr;
	struct node *n_ptr;
        struct node **curr_node;

	n_ptr = kmalloc(sizeof(*n_ptr),GFP_ATOMIC);
        if (n_ptr != NULL) {
                memset(n_ptr, 0, sizeof(*n_ptr));
                n_ptr->addr = addr;
                n_ptr->lock =  SPIN_LOCK_UNLOCKED;	
                INIT_LIST_HEAD(&n_ptr->nsub);
	
		c_ptr = cluster_find(addr);
                if (c_ptr == NULL)
                        c_ptr = cluster_create(addr);
                if (c_ptr != NULL) {
                        n_ptr->owner = c_ptr;
                        cluster_attach_node(c_ptr, n_ptr);
                        n_ptr->last_router = -1;

                        /* Insert node into ordered list */
                        for (curr_node = &nodes; *curr_node; 
			     curr_node = &(*curr_node)->next) {
                                if (addr < (*curr_node)->addr) {
                                        n_ptr->next = *curr_node;
                                        break;
                                }
                        }
                        (*curr_node) = n_ptr;
                } else {
                        kfree(n_ptr);
                        n_ptr = NULL;
                }
        }
	return n_ptr;
}

void node_delete(struct node *n_ptr)
{
	if (!n_ptr)
		return;

#if 0
	/* Not needed because links are already deleted via bearer_stop() */

	u32 l_num;

	for (l_num = 0; l_num < MAX_BEARERS; l_num++) {
		link_delete(n_ptr->links[l_num]);
	}
#endif

	dbg("node %x deleted\n", n_ptr->addr);
	kfree(n_ptr);
}


/**
 * node_link_up - handle addition of link
 * 
 * Link becomes active (alone or shared) or standby, depending on its priority.
 */

void node_link_up(struct node *n_ptr, struct link *l_ptr)
{
	struct link **active = &n_ptr->active_links[0];

	info("Established link <%s> on network plane %c\n",
	     l_ptr->name, l_ptr->b_ptr->net_plane);
	
	if (!active[0]) {
		dbg(" link %x into %x/%x\n", l_ptr, &active[0], &active[1]);
		active[0] = active[1] = l_ptr;
		node_established_contact(n_ptr);
		return;
	}
	if (l_ptr->priority < active[0]->priority) { 
		info("Link is standby\n");
		return;
	}
	link_send_duplicate(active[0], l_ptr);
	if (l_ptr->priority == active[0]->priority) { 
		active[0] = l_ptr;
		return;
	}
	info("Link <%s> on network plane %c becomes standby\n",
	     active[0]->name, active[0]->b_ptr->net_plane);
	active[0] = active[1] = l_ptr;
}

/**
 * node_select_active_links - select active link
 */

static void node_select_active_links(struct node *n_ptr)
{
	struct link **active = &n_ptr->active_links[0];
	u32 i;
	u32 highest_prio = 0;

        active[0] = active[1] = 0;

	for (i = 0; i < MAX_BEARERS; i++) {
                struct link *l_ptr = n_ptr->links[i];

		if (!l_ptr || !link_is_up(l_ptr) ||
		    (l_ptr->priority < highest_prio))
			continue;

		if (l_ptr->priority > highest_prio) {
                        highest_prio = l_ptr->priority;
			active[0] = active[1] = l_ptr;
		} else {
			active[1] = l_ptr;
		}
	}
}

/**
 * node_link_down - handle loss of link
 */

void node_link_down(struct node *n_ptr, struct link *l_ptr)
{
	struct link **active;

	if (!link_is_active(l_ptr)) {
		info("Lost standby link <%s> on network plane %c\n",
		     l_ptr->name, l_ptr->b_ptr->net_plane);
		return;
	}
	info("Lost link <%s> on network plane %c\n",
		l_ptr->name, l_ptr->b_ptr->net_plane);

	active = &n_ptr->active_links[0];
	if (active[0] == l_ptr)
		active[0] = active[1];
	if (active[1] == l_ptr)
		active[1] = active[0];
	if (active[0] == l_ptr)
		node_select_active_links(n_ptr);
	if (node_is_up(n_ptr)) 
		link_changeover(l_ptr);
	else 
		node_lost_contact(n_ptr);
}

int node_has_active_links(struct node *n_ptr)
{
	return (n_ptr && 
		((n_ptr->active_links[0]) || (n_ptr->active_links[1])));
}

int node_has_redundant_links(struct node *n_ptr)
{
	return (node_has_active_links(n_ptr) &&
		(n_ptr->active_links[0] != n_ptr->active_links[1]));
}

int node_has_active_routes(struct node *n_ptr)
{
	return (n_ptr && (n_ptr->last_router >= 0));
}

int node_is_up(struct node *n_ptr)
{
	return (node_has_active_links(n_ptr) || node_has_active_routes(n_ptr));
}

struct node *node_attach_link(struct link *l_ptr)
{
	struct node *n_ptr = node_find(l_ptr->addr);

	if (!n_ptr)
		n_ptr = node_create(l_ptr->addr);
        if (n_ptr) {
		u32 bearer_id = l_ptr->b_ptr->identity;
		char addr_string[16];

                assert(bearer_id < MAX_BEARERS);
                if (n_ptr->link_cnt >= 2) {
			char addr_string[16];

                        err("Attempt to create third link to %s\n",
			    addr_string_fill(addr_string, n_ptr->addr));
                        return 0;
                }

                if (!n_ptr->links[bearer_id]) {
                        n_ptr->links[bearer_id] = l_ptr;
                        net.zones[tipc_zone(l_ptr->addr)]->links++;
                        n_ptr->link_cnt++;
                        return n_ptr;
                }
                err("Attempt to establish second link on <%s> to <%s> \n",
                    l_ptr->b_ptr->publ.name, 
		    addr_string_fill(addr_string, l_ptr->addr));
        }
	return 0;
}

void node_detach_link(struct node *n_ptr, struct link *l_ptr)
{
	n_ptr->links[l_ptr->b_ptr->identity] = 0;
	net.zones[tipc_zone(l_ptr->addr)]->links--;
	n_ptr->link_cnt--;
}

/*
 * Routing table management - five cases to handle:
 *
 * 1: A link towards a zone/cluster external node comes up.
 *    => Send a multicast message updating routing tables of all 
 *    system nodes within own cluster that the new destination 
 *    can be reached via this node. 
 *    (node.establishedContact()=>cluster.multicastNewRoute())
 *
 * 2: A link towards a slave node comes up.
 *    => Send a multicast message updating routing tables of all 
 *    system nodes within own cluster that the new destination 
 *    can be reached via this node. 
 *    (node.establishedContact()=>cluster.multicastNewRoute())
 *    => Send a  message to the slave node about existence 
 *    of all system nodes within cluster:
 *    (node.establishedContact()=>cluster.sendLocalRoutes())
 *
 * 3: A new cluster local system node becomes available.
 *    => Send message(s) to this particular node containing
 *    information about all cluster external and slave
 *     nodes which can be reached via this node.
 *    (node.establishedContact()==>network.sendExternalRoutes())
 *    (node.establishedContact()==>network.sendSlaveRoutes())
 *    => Send messages to all directly connected slave nodes 
 *    containing information about the existence of the new node
 *    (node.establishedContact()=>cluster.multicastNewRoute())
 *    
 * 4: The link towards a zone/cluster external node or slave
 *    node goes down.
 *    => Send a multcast message updating routing tables of all 
 *    nodes within cluster that the new destination can not any
 *    longer be reached via this node.
 *    (node.lostAllLinks()=>cluster.bcastLostRoute())
 *
 * 5: A cluster local system node becomes unavailable.
 *    => Remove all references to this node from the local
 *    routing tables. Note: This is a completely node
 *    local operation.
 *    (node.lostAllLinks()=>network.removeAsRouter())
 *    => Send messages to all directly connected slave nodes 
 *    containing information about loss of the node
 *    (node.establishedContact()=>cluster.multicastLostRoute())
 *
 */

static void node_established_contact(struct node *n_ptr)
{
	struct cluster *c_ptr;

	dbg("node_established_contact:-> %x\n", n_ptr->addr);
	if (!node_has_active_routes(n_ptr)) { 
		k_signal((Handler)named_node_up, n_ptr->addr);
	}

        /* Syncronize broadcast acks */
        n_ptr->bclink.acked = bclink_get_last_sent();

	if (is_slave(tipc_own_addr))
		return;
	if (!in_own_cluster(n_ptr->addr)) {
		/* Usage case 1 (see above) */
		c_ptr = cluster_find(tipc_own_addr);
		if (!c_ptr)
			c_ptr = cluster_create(tipc_own_addr);
                if (c_ptr)
                        cluster_bcast_new_route(c_ptr, n_ptr->addr, 1, 
						tipc_max_nodes);
		return;
	} 

	c_ptr = n_ptr->owner;
	if (is_slave(n_ptr->addr)) {
		/* Usage case 2 (see above) */
		cluster_bcast_new_route(c_ptr, n_ptr->addr, 1, tipc_max_nodes);
		cluster_send_local_routes(c_ptr, n_ptr->addr);
		return;
	}

	if (n_ptr->bclink.supported) {
		nmap_add(&cluster_bcast_nodes, n_ptr->addr);
		if (n_ptr->addr < tipc_own_addr)
			tipc_own_tag++;
	}

	/* Case 3 (see above) */
	net_send_external_routes(n_ptr->addr);
	cluster_send_slave_routes(c_ptr, n_ptr->addr);
	cluster_bcast_new_route(c_ptr, n_ptr->addr, LOWEST_SLAVE,
				highest_allowed_slave);
}

static void node_lost_contact(struct node *n_ptr)
{
	struct cluster *c_ptr;
	struct node_subscr *ns, *tns;
	char addr_string[16];
	u32 i;

        /* Clean up broadcast reception remains */
        n_ptr->bclink.gap_after = n_ptr->bclink.gap_to = 0;
        while (n_ptr->bclink.deferred_head) {
                struct sk_buff* buf = n_ptr->bclink.deferred_head;
                n_ptr->bclink.deferred_head = buf->next;
                buf_discard(buf);
        }
        if (n_ptr->bclink.defragm) {
                buf_discard(n_ptr->bclink.defragm);  
                n_ptr->bclink.defragm = NULL;
        }            
        if (in_own_cluster(n_ptr->addr) && n_ptr->bclink.supported) { 
                bclink_acknowledge(n_ptr, mod(n_ptr->bclink.acked + 10000));
        }

        /* Update routing tables */
	if (is_slave(tipc_own_addr)) {
		net_remove_as_router(n_ptr->addr);
	} else {
		if (!in_own_cluster(n_ptr->addr)) { 
			/* Case 4 (see above) */
			c_ptr = cluster_find(tipc_own_addr);
			cluster_bcast_lost_route(c_ptr, n_ptr->addr, 1,
						 tipc_max_nodes);
		} else {
			/* Case 5 (see above) */
			c_ptr = cluster_find(n_ptr->addr);
			if (is_slave(n_ptr->addr)) {
				cluster_bcast_lost_route(c_ptr, n_ptr->addr, 1,
							 tipc_max_nodes);
			} else {
				if (n_ptr->bclink.supported) {
					nmap_remove(&cluster_bcast_nodes, 
						    n_ptr->addr);
					if (n_ptr->addr < tipc_own_addr)
						tipc_own_tag--;
				}
				net_remove_as_router(n_ptr->addr);
				cluster_bcast_lost_route(c_ptr, n_ptr->addr,
							 LOWEST_SLAVE,
							 highest_allowed_slave);
			}
		}
	}
	if (node_has_active_routes(n_ptr))
		return;

	info("Lost contact with %s\n", 
	     addr_string_fill(addr_string, n_ptr->addr));

	/* Abort link changeover */
	for (i = 0; i < MAX_BEARERS; i++) {
		struct link *l_ptr = n_ptr->links[i];
		if (!l_ptr) 
			continue;
		l_ptr->reset_checkpoint = l_ptr->next_in_no;
		l_ptr->exp_msg_count = 0;
		link_reset_fragments(l_ptr);
	}

	/* Notify subscribers */
	list_for_each_entry_safe(ns, tns, &n_ptr->nsub, nodesub_list) {
                ns->node = 0;
		list_del_init(&ns->nodesub_list);
		k_signal((Handler)ns->handle_node_down,
			 (unsigned long)ns->usr_handle);
	}
}

/**
 * node_select_next_hop - find the next-hop node for a message
 * 
 * Called by when cluster local lookup has failed.
 */

struct node *node_select_next_hop(u32 addr, u32 selector)
{
	struct node *n_ptr;
	u32 router_addr;

        if (!addr_domain_valid(addr))
                return 0;

	/* Look for direct link to destination processsor */
	n_ptr = node_find(addr);
	if (n_ptr && node_has_active_links(n_ptr))
                return n_ptr;

	/* Cluster local system nodes *must* have direct links */
	if (!is_slave(addr) && in_own_cluster(addr))
		return 0;

	/* Look for cluster local router with direct link to node */
	router_addr = node_select_router(n_ptr, selector);
	if (router_addr) 
                return node_select(router_addr, selector);

	/* Slave nodes can only be accessed within own cluster via a 
	   known router with direct link -- if no router was found,give up */
	if (is_slave(addr))
		return 0;

	/* Inter zone/cluster -- find any direct link to remote cluster */
	addr = tipc_addr(tipc_zone(addr), tipc_cluster(addr), 0);
	n_ptr = net_select_remote_node(addr, selector);
	if (n_ptr && node_has_active_links(n_ptr))
                return n_ptr;

	/* Last resort -- look for any router to anywhere in remote zone */
	router_addr =  net_select_router(addr, selector);
	if (router_addr) 
                return node_select(router_addr, selector);

        return 0;
}

/**
 * node_select_router - select router to reach specified node
 * 
 * Uses a deterministic and fair algorithm for selecting router node. 
 */

u32 node_select_router(struct node *n_ptr, u32 ref)
{
	u32 ulim;
	u32 mask;
	u32 start;
	u32 r;

        if (!n_ptr)
                return 0;

	if (n_ptr->last_router < 0)
		return 0;
	ulim = ((n_ptr->last_router + 1) * 32) - 1;

	/* Start entry must be random */
	mask = tipc_max_nodes;
	while (mask > ulim)
		mask >>= 1;
	start = ref & mask;
	r = start;

	/* Lookup upwards with wrap-around */
	do {
		if (((n_ptr->routers[r / 32]) >> (r % 32)) & 1)
			break;
	} while (++r <= ulim);
	if (r > ulim) {
		r = 1;
		do {
			if (((n_ptr->routers[r / 32]) >> (r % 32)) & 1)
				break;
		} while (++r < start);
		assert(r != start);
	}
	assert(r && (r <= ulim));
	return tipc_addr(own_zone(), own_cluster(), r);
}

void node_add_router(struct node *n_ptr, u32 router)
{
	u32 r_num = tipc_node(router);

	n_ptr->routers[r_num / 32] = 
		((1 << (r_num % 32)) | n_ptr->routers[r_num / 32]);
	n_ptr->last_router = tipc_max_nodes / 32;
	while ((--n_ptr->last_router >= 0) && 
	       !n_ptr->routers[n_ptr->last_router]);
}

void node_remove_router(struct node *n_ptr, u32 router)
{
	u32 r_num = tipc_node(router);

	if (n_ptr->last_router < 0)
		return;		/* No routes */

	n_ptr->routers[r_num / 32] =
		((~(1 << (r_num % 32))) & (n_ptr->routers[r_num / 32]));
	n_ptr->last_router = tipc_max_nodes / 32;
	while ((--n_ptr->last_router >= 0) && 
	       !n_ptr->routers[n_ptr->last_router]);

	if (!node_is_up(n_ptr))
		node_lost_contact(n_ptr);
}

#if 0
void node_print(struct print_buf *buf, struct node *n_ptr, char *str)
{
	u32 i;

	tipc_printf(buf, "\n\n%s", str);
	for (i = 0; i < MAX_BEARERS; i++) {
		if (!n_ptr->links[i]) 
			continue;
		tipc_printf(buf, "Links[%u]: %x, ", i, n_ptr->links[i]);
	}
	tipc_printf(buf, "Active links: [%x,%x]\n",
		    n_ptr->active_links[0], n_ptr->active_links[1]);
}
#endif

u32 tipc_available_nodes(const u32 domain)
{
	struct node *n_ptr;
	u32 cnt = 0;

	for (n_ptr = nodes; n_ptr; n_ptr = n_ptr->next) {
		if (!in_scope(domain, n_ptr->addr))
			continue;
		if (node_is_up(n_ptr))
			cnt++;
	}
	return cnt;
}

struct sk_buff *node_get_nodes(const void *req_tlv_area, int req_tlv_space)
{
	u32 domain;
	struct sk_buff *buf;
	struct node *n_ptr;
        struct tipc_node_info node_info;

	if (!TLV_CHECK(req_tlv_area, req_tlv_space, TIPC_TLV_NET_ADDR))
		return cfg_reply_error_string(TIPC_CFG_TLV_ERROR);

	domain = *(u32 *)TLV_DATA(req_tlv_area);
	domain = ntohl(domain);
	if (!addr_domain_valid(domain))
		return cfg_reply_error_string(TIPC_CFG_INVALID_VALUE
					      " (network address)");

        if (!nodes)
                return cfg_reply_none();

	/* For now, get space for all other nodes 
	   (will need to modify this when slave nodes are supported */

	buf = cfg_reply_alloc(TLV_SPACE(sizeof(node_info)) *
			    (tipc_max_nodes - 1));
	if (!buf)
		return NULL;

	/* Add TLVs for all nodes in scope */

	for (n_ptr = nodes; n_ptr; n_ptr = n_ptr->next) {
		if (!in_scope(domain, n_ptr->addr))
			continue;
                node_info.addr = htonl(n_ptr->addr);
                node_info.up = htonl(node_is_up(n_ptr));
		cfg_append_tlv(buf, TIPC_TLV_NODE_INFO, 
			       &node_info, sizeof(node_info));
	}

	return buf;
}

struct sk_buff *node_get_links(const void *req_tlv_area, int req_tlv_space)
{
	u32 domain;
	struct sk_buff *buf;
	struct node *n_ptr;
        struct tipc_link_info link_info;

	if (!TLV_CHECK(req_tlv_area, req_tlv_space, TIPC_TLV_NET_ADDR))
		return cfg_reply_error_string(TIPC_CFG_TLV_ERROR);

	domain = *(u32 *)TLV_DATA(req_tlv_area);
	domain = ntohl(domain);
	if (!addr_domain_valid(domain))
		return cfg_reply_error_string(TIPC_CFG_INVALID_VALUE
					      " (network address)");

        if (!nodes)
                return cfg_reply_none();

	/* For now, get space for 2 links to all other nodes + bcast link 
	   (will need to modify this when slave nodes are supported */

	buf = cfg_reply_alloc(TLV_SPACE(sizeof(link_info)) *
			    (2 * (tipc_max_nodes - 1) + 1));
	if (!buf)
		return NULL;

	/* Add TLV for broadcast link */

        link_info.dest = tipc_own_addr & 0xfffff00;
	link_info.dest = htonl(link_info.dest);
        link_info.up = htonl(1);
        sprintf(link_info.str, bc_link_name);
	cfg_append_tlv(buf, TIPC_TLV_LINK_INFO, &link_info, sizeof(link_info));

	/* Add TLVs for any other links in scope */

	for (n_ptr = nodes; n_ptr; n_ptr = n_ptr->next) {
                u32 i;

		if (!in_scope(domain, n_ptr->addr))
			continue;
                for (i = 0; i < MAX_BEARERS; i++) {
                        if (!n_ptr->links[i]) 
                                continue;
                        link_info.dest = htonl(n_ptr->addr);
                        link_info.up = htonl(link_is_up(n_ptr->links[i]));
                        strcpy(link_info.str, n_ptr->links[i]->name);
			cfg_append_tlv(buf, TIPC_TLV_LINK_INFO, 
				       &link_info, sizeof(link_info));
                }
	}

	return buf;
}