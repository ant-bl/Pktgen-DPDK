/*-
 * Copyright (c) <2010-2019>, Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* Created 2010 by Keith Wiles @ intel.com */

#include <cli_scrn.h>
#include <lua_config.h>

#include "pktgen.h"

#include "pktgen-ipv6.h"

/**************************************************************************//**
 *
 * pktgen_ipv6_ctor - IPv6 packet header constructor routine.
 *
 * DESCRIPTION
 * Construct the IPv6 header constructor routine.
 *
 * RETURNS: N/A
 *
 * SEE ALSO:
 */

void
pktgen_ipv6_ctor(pkt_seq_t *pkt, void *hdr)
{
	struct pg_ipv6_hdr *ip = hdr;
	uint32_t addr;
	uint16_t tlen;

	/* IPv6 Header constructor */
	memset(ip, 0, sizeof(struct pg_ipv6_hdr));

	ip->vtc_flow = htonl(IPv6_VERSION << 28);
	tlen = pkt->pktSize - (pkt->ether_hdr_size + sizeof(struct pg_ipv6_hdr));

	ip->payload_len = htons(tlen);
	ip->hop_limits = 4;
	ip->proto = pkt->ipProto;

	addr = htonl(pkt->ip_dst_addr.addr.ipv4.s_addr);
	(void)rte_memcpy(&ip->dst_addr[8], &addr, sizeof(uint32_t));
	addr = htonl(pkt->ip_src_addr.addr.ipv4.s_addr);
	(void)rte_memcpy(&ip->src_addr[8], &addr, sizeof(uint32_t));
}

/**************************************************************************//**
 *
 * pktgen_process_ping6 - Process a IPv6 ICMP echo request packet.
 *
 * DESCRIPTION
 * Process a IPv6 ICMP echo request packet and send response if needed.
 *
 * RETURNS: N/A
 *
 * SEE ALSO:
 */

void
pktgen_process_ping6(struct rte_mbuf *m __rte_unused,
		     uint32_t pid __rte_unused, uint32_t vlan __rte_unused)
{
#if 0	/* Broken needs to be updated to do IPv6 packets */
	port_info_t     *info = &pktgen.info[pid];
	struct pg_ether_hdr *eth = rte_pktmbuf_mtod(m, struct pg_ether_hdr *);
	struct pg_ipv6_hdr       *ip = (struct pg_ipv6_hdr *)&eth[1];

	/* Adjust for a vlan header if present */
	if (vlan)
		ip = (struct pg_ipv6_hdr *)((char *)ip + sizeof(struct pg_vlan_hdr));

	/* Look for a ICMP echo requests, but only if enabled. */
	if ( (rte_atomic32_read(&info->port_flags) & ICMP_ECHO_ENABLE_FLAG) &&
	     (ip->next_header == PG_IPPROTO_ICMPV6) ) {
#if !defined(RTE_ARCH_X86_64)
		struct pg_icmp_hdr *icmp =
			(struct pg_icmp_hdr *)((uint32_t)ip + sizeof(struct pg_ipv4_hdr));
#else
		struct pg_icmp_hdr *icmp =
			(struct pg_icmp_hdr *)((uint64_t)ip + sizeof(struct pg_ipv4_hdr));
#endif
		/* We do not handle IP options, which will effect the IP header size. */
		if (rte_ipv6_cksum(icmp,
			  (m->pkt.data_len - sizeof(struct pg_ether_hdr) -
			   sizeof(struct pg_ipv4_hdr))) ) {
			rte_printf_status("ICMP checksum failed\n");
			goto leave :
		}

		if (icmp->type == ICMP4_ECHO) {
			/* Toss all broadcast addresses and requests not for this port */
			if ( (ip->dst == INADDR_BROADCAST) ||
			     (ip->dst != info->ip_src_addr) ) {
				char buff[24];
				rte_printf_status("IP address %s != ",
						  inet_ntop4(buff, sizeof(buff),
							     ip->dst,
							     INADDR_BROADCAST));
				rte_printf_status("%s\n",
						  inet_ntop4(buff, sizeof(buff),
							     htonl(info->
								   ip_src_addr),
							     INADDR_BROADCAST));
				goto leave;
			}

			info->echo_pkts++;

			icmp->type  = ICMP4_ECHO_REPLY;

			/* Recompute the ICMP checksum */
			icmp->cksum = 0;
			icmp->cksum =
				rte_raw_cksum(icmp,
				      (m->pkt.data_len -
				       sizeof(struct pg_ether_hdr) -
				       sizeof(struct pg_ipv4_hdr)));

			/* Swap the IP addresses. */
			inetAddrSwap(&ip->src, &ip->dst);

			/* Bump the ident value */
			ip->ident   = htons(ntohs(ip->ident) + m->pkt.data_len);

			/* Recompute the IP checksum */
			ip->cksum   = 0;
			ip->cksum   = rte_raw_cksum(ip, sizeof(struct pg_ipv4_hdr));

			/* Swap the MAC addresses */
			ethAddrSwap(&eth->d_addr, &eth->s_addr);

			pktgen_send_mbuf(m, pid, 0);

			pktgen_set_q_flags(info, 0, DO_TX_FLUSH);

			/* No need to free mbuf as it was reused */
			return;
		}
	}
leave:
#else
#endif
}
