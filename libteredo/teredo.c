/*
 * teredo.c - Common Teredo helper functions
 * $Id$
 *
 * See "Teredo: Tunneling IPv6 over UDP through NATs"
 * for more information
 */

/***********************************************************************
 *  Copyright © 2004-2007 Rémi Denis-Courmont.                         *
 *  This program is free software; you can redistribute and/or modify  *
 *  it under the terms of the GNU General Public License as published  *
 *  by the Free Software Foundation; version 2 of the license.         *
 *                                                                     *
 *  This program is distributed in the hope that it will be useful,    *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of     *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.               *
 *  See the GNU General Public License for more details.               *
 *                                                                     *
 *  You should have received a copy of the GNU General Public License  *
 *  along with this program; if not, you can get it from:              *
 *  http://www.gnu.org/copyleft/gpl.html                               *
 ***********************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h> // memcpy()

#include <inttypes.h> /* for Mac OS X */
#include <sys/types.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netinet/ip6.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <errno.h>

#ifndef SOL_IP
# define SOL_IP IPPROTO_IP
#endif

#include "teredo.h"
#include "teredo-udp.h"

/*
 * Teredo addresses
 */
const struct in6_addr teredo_restrict =
	/* Vista variant */
{ { { 0xfe, 0x80, 0, 0, 0, 0, 0, 0,
	0, 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } } };

	/* XP variant */
//	{ { { 0xfe, 0x80, 0, 0, 0, 0, 0, 0,
//		    0, 0, 'T', 'E', 'R', 'E', 'D', 'O' } } };

const struct in6_addr teredo_cone =
	{ { { 0xfe, 0x80, 0, 0, 0, 0, 0, 0,
		    0x80, 0, 'T', 'E', 'R', 'E', 'D', 'O' } } };

/**
 * Opens a Teredo UDP/IPv4 socket.
 * Thread-safe, not cancellation-safe.
 *
 * @return -1 on error.
 */
int teredo_socket (uint32_t bind_ip, uint16_t port)
{
	struct sockaddr_in myaddr =
	{
		.sin_family = AF_INET,
#ifdef HAVE_SA_LEN
		.sin_len = sizeof (struct sockaddr_in),
#endif
		.sin_port = port,
		.sin_addr.s_addr = bind_ip
	};

	int fd = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (fd == -1)
		return -1; // failure

	fcntl (fd, F_SETFD, FD_CLOEXEC);
	setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof (int));

	if (bind (fd, (struct sockaddr *)&myaddr, sizeof (myaddr)))
	{
		close (fd);
		return -1;
	}

#ifdef IP_PMTUDISC_DONT
	/* 
	 * This tells the (Linux) kernel not to set the Don't Fragment flags
	 * on UDP packets we send. This is recommended by the Teredo
	 * specifiation.
	 */
	setsockopt (fd, SOL_IP, IP_MTU_DISCOVER, &(int){ IP_PMTUDISC_DONT },
	            sizeof (int));
#endif
	/*
	 * Teredo multicast packets always have a TTL of 1.
	 */
	setsockopt (fd, SOL_IP, IP_MULTICAST_TTL, &(int){ 1 }, sizeof (int));
	return fd;
}


/**
 * Sends an UDP/IPv4 datagram.
 * Thread-safe, cancellation-safe, cancellation point.
 *
 * @param fd socket from which to send.
 * @param iov scatter-gather array containing the datagram payload.
 * @param count number of entry in the scatter-gather array.
 * @param dest_ip destination IPv4 (network byte order).
 * @param dest_port destination UDP port (network byte order).
 *
 * @return number of bytes sent or -1 on error.
 */

int teredo_sendv (int fd, const struct iovec *iov, size_t count,
                  uint32_t dest_ip, uint16_t dest_port)
{
	struct sockaddr_in addr =
	{
		.sin_family = AF_INET,
#ifdef HAVE_SA_LEN
		.sin_len = sizeof (struct sockaddr_in),
#endif
		.sin_port = dest_port,
		.sin_addr.s_addr = dest_ip
	};

	struct msghdr msg =
	{
		.msg_name = &addr,
		.msg_namelen = sizeof (addr),
		.msg_iov = (struct iovec *)iov,
		.msg_iovlen = count
	};

	for (int tries = 0; tries < 10; tries++)
	{
		int res = sendmsg (fd, &msg, 0);
		if (res != -1)
			return res;
		/*
		 * NOTE:
		 * We must ignore ICMP errors returned by sendto() because they are
		 * asynchronous, so that in most case they refer to a packet which was
		 * sent earlier already, most likely to another destination.
		 * That means we also ignore EHOSTUNREACH when it is generated by the
		 * kernel routing table (meaning we are not attached to the network);
		 * while it would have been a good idea to handle that case properly,
		 * it's never been implemented in Miredo, and it turns out the ICMP
		 * errors issue prevents any future implementation.
		 *
		 * NOTE 2:
		 * To prevent an infinite loop in case of a really unreachable
		 * destination, we must have a limit on the number of sendto()
		 * attempts.
		 */
		switch (errno)
		{
			/*case EMSGSIZE:*/ /* ICMP fragmentation needed *
			 * given we don't ensure that the length is below 65507, we
			 * must not ignore that error */
			case ENETUNREACH: /* ICMP address unreachable */
			case EHOSTUNREACH: /* ICMP destination unreachable */
			case ENOPROTOOPT: /* ICMP protocol unreachable */
			case ECONNREFUSED: /* ICMP port unreachable */
			case EOPNOTSUPP: /* ICMP source route failed
							- should not happen */
			case EHOSTDOWN: /* ICMP host unknown */
#ifdef ENONET
			case ENONET: /* ICMP host isolated */
#endif
				continue;
	
			default:
				return -1; /* hard error */
		}
	}

	return -1;
}


/**
 * Sends an UDP/IPv4 datagram.
 * Thread-safe, cancellation safe, cancellation point.
 *
 * @return number of bytes sent, or -1 on error.
 */
int teredo_send (int fd, const void *packet, size_t plen,
                 uint32_t dest_ip, uint16_t dest_port)
{
	struct iovec iov = { (void *)packet, plen };
	return teredo_sendv (fd, &iov, 1, dest_ip, dest_port);
}


static int teredo_recv_inner (int fd, struct teredo_packet *p, int flags)
{
	// Receive a UDP packet
	struct sockaddr_in ad;
	int length = recvfrom (fd, p->buf, sizeof (p->buf), flags,
	                       (struct sockaddr *)&ad,
	                       &(socklen_t){ sizeof (ad) });

	if (length < 2) // too small or error
		return -1;

	p->source_ipv4 = ad.sin_addr.s_addr;
	p->source_port = ad.sin_port;

	uint8_t *ptr = p->buf;

	p->auth_nonce = NULL;
	p->auth_conf_byte = 0;
	p->orig_ipv4 = 0;
	p->orig_port = 0;

	// Teredo Authentication header
	if ((ptr[0] == 0) && (ptr[1] == teredo_auth_hdr))
	{
		uint8_t id_len, au_len;

		length -= 13;
		if (length < 0)
			return -1; // too small
		ptr += 2;

		/* ID and Auth */
		id_len = *ptr++;
		au_len = *ptr++;

		/* TODO: secure qualification */
		length -= id_len + au_len;
		if (length < 0)
			return -1;
		ptr += id_len + au_len;


		/* Nonce + confirmation byte */
		p->auth_nonce = ptr;
		ptr += 8;
		p->auth_conf_byte = *ptr++;
	}

	// Teredo Origin Indication
	if ((ptr[0] == 0) && (ptr[1] == teredo_orig_ind))
	{
		uint32_t addr;
		uint16_t port;

		length -= 8;
		if (length < 0)
			return -1; /* too small */
		ptr += 2;

		/* Obfuscated port */
		memcpy (&port, ptr, 2);
		ptr += 2;
		p->orig_port = ~port;

		/* Obfuscated IPv4 */
		memcpy (&addr, ptr, 4);
		ptr += 4;
		p->orig_ipv4 = ~addr;
	}

	/* length <= 65507 = sizeof(buf) */
	p->ip6_len = length;
	p->ip6 = ptr;

	return 0;
}


/**
 * Receives and parses a Teredo packet from a socket.
 * Blocks if the socket is blocking, don't block if not.
 * Thread-safe, cancellation-safe, cancellation point.
 *
 * @param fd socket file descriptor
 * @param p teredo_packet receive buffer
 *
 * @return 0 on success, -1 in error.
 * Errors might be caused by :
 *  - lower level network I/O,
 *  - malformatted packets,
 *  - no data pending while using a non-blocking socket.
 */
int teredo_recv (int fd, struct teredo_packet *p)
{
	return teredo_recv_inner (fd, p, MSG_DONTWAIT);
}


#ifdef __FreeBSD__
# include <sys/poll.h>
#endif
/**
 * Waits for, receives and parses a Teredo packet from a socket.
 * Thread-safe, cancellation-safe, cancellation point.
 *
 * @param fd socket file descriptor
 * @param p teredo_packet receive buffer
 *
 * @return 0 on success, -1 in error.
 * Errors might be caused by :
 *  - lower level network I/O,
 *  - malformatted packets,
 *  - a race condition if two thread are waiting on the same
 *    non-blocking socket for receiving.
 */
int teredo_wait_recv (int fd, struct teredo_packet *p)
{
#ifdef __FreeBSD__
	// recvfrom() is not a cancellation point on FreeBSD 6.1...
	struct pollfd ufd = { .fd = fd, .events = POLLIN };
	if (poll (&ufd, 1, -1) == -1)
		return -1;
#endif

	return teredo_recv_inner (fd, p, 0);
}


/* This does not fit anywhere and is needed by both relay and server */
#include <stdbool.h>

/**
 * Computes an Internet checksum over a scatter-gather array.
 * Buffers need not be aligned neither of even length.
 * Jumbograms are supported (though you probably don't care).
 */
static uint16_t in_cksum (const struct iovec *iov, size_t n)
{
	uint32_t sum = 0;
	union
	{
		uint16_t word;
		uint8_t  bytes[2];
	} w;
	bool odd = false;

	while (n > 0)
	{
		const uint8_t *ptr = iov->iov_base;

		for (size_t len = iov->iov_len; len > 0; len--)
		{
			if (odd)
			{
				w.bytes[1] = *ptr++;
				sum += w.word;
				if (sum > 0xffff)
					sum -= 0xffff;
			}
			else
				w.bytes[0] = *ptr++;
			odd = !odd;
		}

		iov++;
		n--;
	}

	if (odd)
	{
		w.bytes[1] = 0;
		sum += w.word;
		if (sum > 0xffff)
			sum -= 0xffff;
	}

	return sum ^ 0xffff;
}


/**
 * Computes an IPv6 layer-3 checksum.
 * The input buffers do not need to be aligned neither of even length.
 * Jumbo datagrams are supported.
 */
uint16_t
teredo_cksum (const void *src, const void *dst, uint8_t protocol,
              const struct iovec *data, size_t n)
{
	struct iovec iov[3 + n];
	size_t plen = 0;
	for (size_t i = 0; i < n; i++)
	{
		iov[3 + i].iov_base = data[i].iov_base;
		plen += (iov[3 + i].iov_len = data[i].iov_len);
	}

	uint32_t pseudo[4] = { htonl (plen), htonl (protocol) };
	iov[0].iov_base = (void *)src;
	iov[0].iov_len = 16;
	iov[1].iov_base = (void *)dst;
	iov[1].iov_len = 16;
	iov[2].iov_base = pseudo;
	iov[2].iov_len = 8;

	return in_cksum (iov, 3 + n);
}

