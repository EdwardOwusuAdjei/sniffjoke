/*
 *   SniffJoke is a software able to confuse the Internet traffic analysis,
 *   developed with the aim to improve digital privacy in communications and
 *   to show and test some securiy weakness in traffic analysis software.
 *   
 *   Copyright (C) 2010 vecna <vecna@delirandom.net>
 *                      evilaliv3 <giovanni.pellerano@evilaliv3.org>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "sj_packet.h"
#include "sj_utils.h"

Packet::Packet(int size, const unsigned char* buff, int buff_size) :
	pbuf(size, 0),
	pbuf_size(size),
	packet_id(make_pkt_id(buff)),
	source(SOURCEUNASSIGNED),
	status(STATUSUNASSIGNED),
	wtf(JUDGEUNASSIGNED),
	proto(PROTOUNASSIGNED)
{
	memcpy(&(pbuf[0]), buff, buff_size);	
	updatePointers();
	orig_pktlen = ntohs(ip->tot_len);
}

Packet::Packet(const Packet& pkt) :
	pbuf(pkt.pbuf),
	pbuf_size(pkt.pbuf_size),
	orig_pktlen(pkt.orig_pktlen),
	packet_id(0),
	source(SOURCEUNASSIGNED),
	status(STATUSUNASSIGNED),
	wtf(JUDGEUNASSIGNED),
	proto(PROTOUNASSIGNED)
{
	updatePointers();
}

Packet::~Packet() {}

unsigned int Packet::make_pkt_id(const unsigned char* buf) const
{
	struct iphdr *ip = (struct iphdr *)buf;
	struct tcphdr *tcp;
	if (ip->protocol == IPPROTO_TCP) {
		tcp = (struct tcphdr *)((unsigned char *)(ip) + (ip->ihl * 4));
		return tcp->seq;
	} else
		return 0; /* packet_id == 0 mean no ID check */
}

void Packet::mark(source_t source, status_t status, judge_t judge)
{
	this->source = source;
	this->status = status;
	this->wtf = judge;
}

void Packet::increasePbuf(unsigned int morespace) {
	/* the pbuf can only be incremented safaly, not decremented */
	unsigned int newpbuf_size = pbuf_size + morespace;
	pbuf.resize(newpbuf_size);
	
	updatePointers();
	
	/* fixing the new length */
	pbuf_size = newpbuf_size;
}

void Packet::resizePayload(unsigned int newlen) {
	/* the payload can be incremented or decremented safely */
	int iphlen = ip->ihl * 4;
	int tcphlen = tcp->doff * 4;
	int oldlen = ntohs(ip->tot_len) - (iphlen + tcphlen);
	unsigned int newpbuf_size = pbuf_size - oldlen + newlen;
	vector<unsigned char> newpbuf = vector<unsigned char>(newpbuf_size, 0);
	unsigned newtotallen = iphlen + tcphlen + newlen;
	
	/* IP header copy , TCP header copy, Payload copy, if preserved */
	int copysize = newtotallen > ntohs(ip->tot_len) ? ntohs(ip->tot_len) : newtotallen;
	memcpy(&(newpbuf[0]), &(pbuf[0]), copysize );
	pbuf = newpbuf;

        ip = (struct iphdr *)&(pbuf[0]);
        ip->tot_len = htons(newtotallen);

        /* fixing the new length */
        pbuf_size = newpbuf_size;
	
	updatePointers();
}

void Packet::updatePointers(void) {
	
	ip = (struct iphdr *)&(pbuf[0]);
	if (ip->protocol == IPPROTO_TCP) {
		proto = TCP;
		tcp = (struct tcphdr *)((unsigned char *)(ip) + (ip->ihl * 4));
		icmp = NULL;
		if ((ntohs(ip->tot_len) - ((ip->ihl * 4) + (tcp->doff * 4))) > 0)
			payload = (unsigned char *)tcp + tcp->doff * 4;
		else
			payload = NULL;
	} else if (ip->protocol == IPPROTO_ICMP) {
		proto = ICMP;
		tcp = NULL;
		icmp = (struct icmphdr *)((unsigned char *)(ip) + (ip->ihl * 4));
		payload = NULL;
	} else {
		proto = OTHER_IP;
		tcp = NULL;
		icmp = NULL;
		payload = NULL;
	}
}

unsigned int Packet::half_cksum(const void *pointed_data, int len)
{
	unsigned int sum = 0x00;
	unsigned short carry = 0x00;
	unsigned short *data = (unsigned short *)pointed_data;

	while (len > 1)
	{
		sum += *data++;
		len -= 2;
	}

	if (len == 1)
	{
		*((unsigned short *) &carry) = *(unsigned char *) data;
		sum += carry;
	}

	return sum;
}

unsigned short Packet::compute_sum(unsigned int sum)
{
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	return (unsigned short) ~sum;
}

void Packet::fixIpTcpSum(void)
{
	unsigned int sum;
	unsigned int l4len = ntohs(ip->tot_len) - (ip->ihl * 4);

	ip->check = 0;
	sum = half_cksum((void *)ip, (ip->ihl * 4));
	ip->check = compute_sum(sum);
	tcp->check = 0;
	sum = half_cksum((void *) &ip->saddr, 8);
	sum += htons (IPPROTO_TCP + l4len);
	sum += half_cksum((void *)tcp, l4len);
	tcp->check = compute_sum(sum);
}

HackPacket::HackPacket(const Packet& pkt) :
	Packet(pkt.pbuf_size + MAXOPTINJ, &(pkt.pbuf[0]), pkt.pbuf_size)
{
	packet_id = 0;
}

/*
 * TCP/IP hacks, focus:
 *
 *  suppose the sniffer reconstruction flow, suppose which variable they use, make them
 *  variables fake and send a HackPacket that don't ruin the real flow.
 *
 * SjH__ = sniffjoke hack
 *
 */
void HackPacket::SjH__fake_data(void)
{
	position = ANTICIPATION;
	const int diff = ntohs(ip->tot_len) - ((ip->ihl * 4) + (tcp->doff * 4));

	ip->id = htons(ntohs(ip->id) + (random() % 10));

	for (int i = 0; i < diff - 3; i += 4)
		*(long int *)&(payload[i]) = random();
}

void HackPacket::SjH__fake_seq(void)
{
	position = ANTICIPATION;
	int what = (random() % 3);

	if (what == 0)
		what = 2;

	if (what == 1) 
		tcp->seq = htonl(ntohl(tcp->seq) - (random() % 5000));

	if (what == 2)
		tcp->seq = htonl(ntohl(tcp->seq) + (random() % 5000));
		
	tcp->window = htons((random() % 80) * 64);
	tcp->ack = tcp->ack_seq = 0;

	SjH__fake_data();
}

void HackPacket::SjH__fake_close(void)
{
	position = ANTICIPATION;
	const int original_size = orig_pktlen - (ip->ihl * 4) - (tcp->doff * 4);
	ip->id = htons(ntohs(ip->id) + (random() % 10));
	
	/* fake close could have FIN+ACK or RST+ACK */
	tcp->psh = 0;

	if (1)/* if (random() % 2) FIXME, a fake rst seems to break connection */
		tcp->fin = 1;
	else
		tcp->rst = 1; 

	/* in both case, the sequence number must be shrink as no data is there.
	 * the ack_seq is set because the ACK flag is checked to be 1 */
	tcp->seq = htonl(ntohl(tcp->seq) - original_size + 1);
}

void HackPacket::SjH__zero_window(void)
{
	position = ANTICIPATION;
	tcp->syn = tcp->fin = tcp->rst = 1;
	tcp->psh = tcp->ack = 0;
	tcp->window = 0;
}

void HackPacket::SjH__valid_rst_fake_seq()
{
	/* 
	 * if the session is resetted, the remote box maybe vulnerable to:
	 * Slipping in the window: TCP Reset attacks
	 * http://kerneltrap.org/node/3072
	 */
	position = ANTICIPATION;
	ip->id = htons(ntohs(ip->id) + (random() % 10));
	tcp->seq = htonl(ntohl(tcp->seq) + 65535 + (random() % 12345));
	tcp->window = htons((unsigned short)(-1));
	tcp->rst = tcp->ack = 1;
	tcp->ack_seq = htonl(ntohl(tcp->seq) + 1);
	tcp->fin = tcp->psh = tcp->syn = 0;
}

void HackPacket::SjH__fake_syn(void)
{
	position = ANTICIPATION;
	ip->id = htons(ntohs(ip->id) + (random() % 10));
	
	tcp->psh = 0;
	tcp->syn = 1;
	tcp->seq = htonl(ntohl(tcp->seq) + 65535 + (random() % 5000));

	/* 20% is a SYN ACK */
	if ((random() % 5) == 0) { 
		tcp->ack = 1;
		tcp->ack_seq = random();
	} else {
		tcp->ack = tcp->ack_seq = 0;
	}

	/* payload is always truncated */
	ip->tot_len = htons((ip->ihl * 4) + (tcp->doff * 4));

	/* 20% had source and dest port reversed */
	if ((random() % 5) == 0) {
		unsigned short swap = tcp->source;
		tcp->source = tcp->dest;
		tcp->dest = swap;
	}
}

void HackPacket::SjH__shift_ack(void)
{
	position = ANTICIPATION;
	ip->id = htons(ntohs(ip->id) + (random() % 10));
	tcp->ack_seq = htonl(ntohl(tcp->ack_seq) + 65535);
}

/* ipopt IPOPT_RR inj*/
void HackPacket::SjH__inject_ipopt(void)
{
	position = ANTICIPATION;
	const int route_n = random() % 10;
	const unsigned fakeipopt = ((route_n + 1) * 4);
	
	const int needed_space = fakeipopt;
	const int free_space = pbuf_size - ntohs(ip->tot_len);
	if(free_space < needed_space)
		increasePbuf(needed_space - free_space);	

	int iphlen = ip->ihl * 4;
	int tcphlen = tcp->doff * 4;
	const int l47len = ntohs(ip->tot_len) - iphlen;
	
	unsigned char *endip = &(pbuf[0]) + sizeof(struct iphdr);
	const int startipopt = ip->ihl * 4 - sizeof(struct iphdr);

	/* 1: strip the original ip options, if present, copying payload over */	
	if (iphlen > sizeof(struct iphdr)) 
		memmove(endip, &pbuf[0] + iphlen, l47len);

	iphlen = sizeof(struct iphdr) + fakeipopt;

	/* 2: shift the tcphdr and the payload bytes after the reserved space to IPOPT_RR */
	memmove(endip + fakeipopt, endip, l47len);

	endip[0] = IPOPT_NOP;
	endip[1] = IPOPT_RR;		/* IPOPT_OPTVAL */
	
	/* Here comes the tha hack, 4 more or 4 less the right value*/
	if (random() % 2)
		endip[2] = fakeipopt - 1 - (4 * (random() % 5));	/* IPOPT_OLEN   */
	else
		endip[2] = fakeipopt - 1 + (4 * (random() % 5));	/* IPOPT_OLEN   */
				
	endip[3] = IPOPT_MINOFF;	/* IPOPT_OFFSET = IPOPT_MINOFF = 4 */

	for (int i = 4; i < fakeipopt; i +=4)
		endip[i] = random();

	ip->ihl = iphlen / 4;
	ip->tot_len = htons(iphlen + l47len);
	tcp = (struct tcphdr *)((unsigned char*)(ip) + iphlen);
	payload = (unsigned char *)(tcp) + tcphlen;
}


/* tcpopt TCPOPT_TIMESTAMP inj with bad TCPOLEN_TIMESTAMP */
void HackPacket::SjH__inject_tcpopt(void)
{
	position = ANTICIPATION;
	const int faketcpopt = 4;
	const int needed_space = faketcpopt;
	const int free_space = pbuf_size - ntohs(ip->tot_len);
	if(free_space < needed_space)
		increasePbuf(needed_space - free_space);
	
	int iphlen = ip->ihl * 4;
	int tcphlen = tcp->doff * 4;
	const int l57len = ntohs(ip->tot_len) - (iphlen + tcphlen);
	unsigned char *endtcp = &(pbuf[0]) + iphlen + sizeof(struct tcphdr);
	const int starttcpopt = tcphlen - sizeof(struct tcphdr);
	const time_t now = time(NULL);

	/* 1: strip the original tcp options, if present, copying payload over */
	if (tcphlen > sizeof(struct tcphdr))
		memmove(endtcp, &(pbuf[0]) + tcphlen, l57len);

	tcphlen = sizeof(struct tcphdr) + faketcpopt;
	
	/* 2: shift the payload after the reserved space to faketcpopt */
	memmove(endtcp + faketcpopt, endtcp, l57len);

	endtcp[0] = TCPOPT_NOP;
	endtcp[1] = TCPOPT_NOP;
	endtcp[2] = TCPOPT_TIMESTAMP;
	endtcp[3] = random() % 11;

	/*
	 *  from: /usr/include/netinet/tcp.h:
	 *  # define TCPOLEN_TIMESTAMP	  10
	 *  NOP (1) + NOP (1) + Timestamp Value (TSval) (4) + Timestamp Echo Reply (TSecr) (4)
	 * 
	 *  so the hacks are two:
	 *   - the size indicated could be different than 10
	 *   - there is no space reserved for timestamps
	 */ 

	ip->tot_len = htons(iphlen + tcphlen + faketcpopt + l57len);
	tcp->doff = (sizeof(struct tcphdr) + faketcpopt) & 0xf;
	payload = (unsigned char *)(tcp) + tcphlen;
}

/* SjH__fake_data_anticipation and SjH__fake_data_posticipation
 * are both the same hack, and need to be used together, anyway for 
 * design pourpose, every injected packet require a dedicated 
 * function.
 *
 * -- note: this should not me true, anyway, this is the 0.4alpha2
 *  developing, so optimization is not require 
 */
void HackPacket::SjH__fake_data_anticipation(void)
{
	position = ANTICIPATION;
	const int diff = ntohs(ip->tot_len) - ((ip->ihl * 4) + (tcp->doff * 4));
	memset(payload, 'A', diff);
}

void HackPacket::SjH__fake_data_posticipation(void)
{
	position = POSTICIPATION;
	const int diff = ntohs(ip->tot_len) - ((ip->ihl * 4) + (tcp->doff * 4));
	memset(payload, 'B', diff);
}
