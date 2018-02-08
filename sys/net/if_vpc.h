/*
 * Copyright (C) 2017 Matthew Macy <matt.macy@joyent.com>
 * Copyright (C) 2017 Joyent Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef __IF_VPC_H_
#define __IF_VPC_H_

#include <netinet/in.h>

#define VPC_VERS 0x20171228
struct vpc_ioctl_header {
	uint64_t vih_magic;
	uint64_t vih_type;
};
struct vpc_listen {
	struct vpc_ioctl_header vl_vih;
	struct sockaddr vl_addr;
};

struct vpc_fte {
	uint32_t vf_vni;
	uint8_t vf_hwaddr[ETHER_ADDR_LEN];
	struct sockaddr vf_protoaddr;
};

struct vpc_fte_update {
	struct vpc_ioctl_header vfu_vih;
	struct vpc_fte vfu_vfte;
};

struct vpc_fte_list {
	struct vpc_ioctl_header vfl_vih;
	uint32_t vfl_count;
	struct vpc_fte vfl_vftes[0];
};

#define VPC_LISTEN								\
	_IOW('k', 1, struct vpc_listen)
#define VPC_FTE_SET								\
	_IOW('k', 2, struct vpc_fte_update)
#define VPC_FTE_DEL								\
	_IOW('k', 3, struct vpc_fte_update)
#define VPC_FTE_ALL								\
	_IOWR('k', 4, struct vpc_fte_list)


struct vpci_attach {
	struct vpc_ioctl_header va_vih;
	char va_ifname[IFNAMSIZ];

};
struct vpci_vni {
	struct vpc_ioctl_header vv_vih;
	uint32_t vv_vni;
};

#define VPCI_ATTACH								\
	_IOW('k', 1, struct vpci_attach)
#define VPCI_ATTACHED_GET			   			\
	_IOWR('k', 2, struct vpci_attach)
#define VPCI_DETACH								\
	_IOW('k', 3, struct vpc_ioctl_header)
#define VPCI_VNI_SET							\
	_IOW('k', 4, struct vpci_vni)
#define VPCI_VNI_GET							\
	_IOWR('k', 5, struct vpci_vni)


struct vpcb_port {
	struct vpc_ioctl_header vp_ioh;
	char vp_if[IFNAMSIZ];
};

#define VPCB_PORT_ADD							\
	_IOW('k', 2, struct vpcb_port)
#define VPCB_PORT_DEL						\
	_IOW('k', 3, struct vpcb_port)


#define VPCB_REQ_NDv4 0x1
#define VPCB_REQ_NDv6 0x2
#define VPCB_REQ_DHCPv4 0x3
#define VPCB_REQ_DHCPv6 0x4
#define VPCB_REQ_MAX VPCB_REQ_DHCPv6 

#define VPCB_VERSION 0x42


struct vpcb_op_header {
	uint32_t voh_version;
	uint32_t voh_op;
};

struct vpcb_op_context {
	uint32_t voc_vni;
	uint16_t voc_vlanid;
	uint8_t voc_smac[ETHER_ADDR_LEN];
};

union vpcb_request_data {
	struct {
		struct in_addr target;
	} vrqd_ndv4;
	struct {
		struct in6_addr target;
	} vrqd_ndv6;
};

struct vpcb_request {
	struct vpcb_op_header vrq_header;
	struct vpcb_op_context vrq_context;
	union vpcb_request_data vrq_data;
};

union vpcb_response_data {
	struct {
		uint8_t ether_addr[ETHER_ADDR_LEN];
	} vrsd_ndv4;
	struct {
		uint8_t ether_addr[ETHER_ADDR_LEN];
	} vrsd_ndv6;
	struct {
		struct in_addr client_addr;
		struct in_addr gw_addr;
		struct in_addr dns_addr;
		uint8_t prefixlen;
	} vrsd_dhcpv4;
	struct {
		struct in6_addr client_addr;
		struct in6_addr gw_addr;
		struct in6_addr dns_addr;
		uint8_t prefixlen;
	} vrsd_dhcpv6;
};

struct vpcb_response {
	struct vpcb_op_header vrs_header;
	struct vpcb_op_context vrs_context;
	union vpcb_response_data vrs_data;
};

#define VPCB_POLL									\
	_IOWR('k', 1, struct vpcb_request)
#define VPCB_RESPONSE_NDv4		   					\
	_IOW('k', 2, struct vpcb_response)
#define VPCB_RESPONSE_NDv6		   					\
	_IOW('k', 3, struct vpcb_response)
#define VPCB_RESPONSE_DHCPv4		  				\
	_IOW('k', 4, struct vpcb_response)
#define VPCB_RESPONSE_DHCPv6		   	  			\
	_IOW('k', 5, struct vpcb_response)


#ifdef _KERNEL
#include <sys/proc.h>
#include <sys/sched.h>
#include <net/art.h>
#include <ck_epoch.h>

struct ifp_cache {
	uint16_t ic_ifindex_max;
	uint16_t ic_size;
	uint32_t ic_pad;
	struct ifnet *ic_ifps[0];
};

struct vpc_pkt_info {
	uint16_t vpi_etype;
	uint8_t vpi_l2_len;
	uint8_t vpi_l3_len;
	uint8_t vpi_l4_len;
	uint8_t vpi_v6:1;
	uint8_t vpi_proto:7;
};

struct ck_epoch_record;
extern struct ck_epoch_record vpc_global_record;
DPCPU_DECLARE(struct ck_epoch_record *, vpc_epoch_record);
extern struct ifp_cache *vpc_ic;
extern struct grouptask vpc_ifp_task;


int parse_pkt(struct mbuf *m0, struct vpc_pkt_info *tpi, int mvec);
int vpc_art_tree_clone(art_tree *src, art_tree **dst, struct malloc_type *type);
void vpc_art_free(art_tree *tree, struct malloc_type *type);

static inline void
vpc_epoch_begin(void)
{
	_critical_enter();
	sched_pin();
	ck_epoch_begin(DPCPU_GET(vpc_epoch_record), NULL);
	_critical_exit();
}

static inline void
vpc_epoch_end(void)
{
	_critical_enter();
	sched_unpin();
	ck_epoch_end(DPCPU_GET(vpc_epoch_record), NULL);
	_critical_exit();
}

int vpc_ifp_cache(struct ifnet *ifp);

#endif

#endif
