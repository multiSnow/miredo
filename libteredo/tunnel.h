/**
 * @file tunnel.h
 * @brief libteredo public C API
 *
 * @mainpage libteredo documentation
 *
 * Libteredo is an implementation of the Teredo protocol for
 * unmanaged tunneling of IPv6 over UDP/IPv4 through NAT devices.
 * Libteredo is primilarly used by the miredo daemon.
 * 
 * Refer to tunnel.h for the external API documentation.
 */

/***********************************************************************
 *  Copyright © 2004-2009 Rémi Denis-Courmont and contributors.        *
 *  This program is free software; you can redistribute and/or modify  *
 *  it under the terms of the GNU General Public License as published  *
 *  by the Free Software Foundation; version 2 of the license, or (at  *
 *  your option) any later version.                                    *
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

#ifndef LIBTEREDO_TUNNEL_H
# define LIBTEREDO_TUNNEL_H

# include <stdbool.h>
# include <stddef.h>

# ifdef __cplusplus
extern "C" {
# endif

# ifdef __GNUC__
#  define LIBTEREDO_DEPRECATED __attribute__ ((deprecated))
# else
#  define LIBTEREDO_DEPRECATED
# endif

# if __STDC_VERSION__ < 199901L
#  ifndef restrict
#   define restrict
#  endif
# endif

struct in6_addr;
struct ip6_hdr;

/**
 * Teredo tunnel instance.
 */
typedef struct teredo_tunnel teredo_tunnel;

/**
 * Creates a teredo_tunnel instance. teredo_preinit() must have been
 * called first.
 *
 * @note This function is thread-safe.
 *
 * @param ipv4 IPv4 (network byte order) to bind to, or 0 if unspecified.
 * @param port UDP/IPv4 port number (network byte order) or 0 if unspecified.
 *
 * Note that some campus firewall drop certain UDP ports (typically those used
 * by some P2P application); in that case, you should use a fixed port so that
 * the kernel does not select a possibly blocked port. Also note that some
 * severely broken NAT devices might fail if multiple NAT-ed computers use the
 * same source UDP port number at the same time, so avoid you should
 * paradoxically avoid querying a fixed port.
 *
 * @return NULL in case of failure.
 */
teredo_tunnel *teredo_create (uint32_t ipv4, uint16_t port);

/**
 * Releases all resources (sockets, memory chunks...) and terminates all
 * threads associated with a teredo_tunnel instance.
 *
 * @warning This function must obviously not be called from within a callback
 * associated with the same tunnel instance.
 *
 * @param t tunnel to be destroyed. No longer useable thereafter.
 *
 * @return nothing (always succeeds).
 */
void teredo_destroy (teredo_tunnel *t);

/**
 * Spawns a new thread to perform Teredo packet reception in the background.
 * The thread will be automatically terminated when the tunnel is destroyed.
 * This function has to be called after the Teredo tunnel is configured
 * (with the the teredo_set_*() functions); otherwise, incoming Teredo packets
 * would not be processed.
 *
 * @note This function can safely be called multiple times; any extra
 * invocation will have no effects. All calls for the same tunnnel must be
 * serialized.
 *
 * @param t Teredo tunnel instance
 *
 * @return 0 on success, -1 on error or if the tunnel is already running.
 */
int teredo_run_async (teredo_tunnel *t);

/**
 * Defines the cone flag of the Teredo tunnel.
 * This only works for Teredo relays.
 *
 * @warning This function must <b>not</b> be used after teredo_transmit() or
 * teredo_run_async() the specified tunnel. That is undefined.
 *
 * @param t Teredo tunnel instance
 * @param flag true to disable sending of direct Teredo bubble,
 *             false to enable it.
 *
 * @return 0 on success, -1 on error (in which case the teredo_tunnel
 * instance is not modified).
 */
int teredo_set_cone_flag (teredo_tunnel *t, bool flag);

/**
 * Enables Teredo relay mode (this is the default).
 *
 * @warning This function must <b>not</b> be used after teredo_transmit() or
 * teredo_run_async() the specified tunnel. That is undefined.
 *
 * @param t Teredo tunnel instance
 *
 * @return 0 on success, -1 on error.
 */
int teredo_set_relay_mode (teredo_tunnel *t);

/**
 * Enables Teredo client mode for a teredo_tunnel and starts the Teredo
 * client maintenance procedure in a separate thread.
 *
 * @note This function will return an error (and have no further effects)
 * if the same tunnel is already in client mode.
 *
 * @warning This function must <b>not</b> be used after teredo_transmit() or
 * teredo_run_async() the specified tunnel. That is undefined.
 *
 * @param t Teredo tunnel instance
 * @param s1 Teredo server's host name or “dotted quad” primary IPv4 address.
 * @param s2 Teredo server's secondary address (or host name), or NULL to
 * infer it from @p s1.
 *
 * @return 0 on success, -1 in case of error.
 * In case of error, the teredo_tunnel instance is not modifed.
 */
int teredo_set_client_mode (teredo_tunnel *restrict t, const char *s1,
                            const char *s2);

/**
 * Enables the Teredo local client discovery procedure.
 * This function has no effects if the tunnel is not in client mode.
 *
 * @note This function must <b>not</b> be used after teredo_transmit() or
 * teredo_run_async() the specified tunnel. That is undefined.
 *
 * @param t Tereo tunnel instance
 * @param on whether to enable (true) or disable (false) local discovery
 */
void teredo_set_local_discovery (teredo_tunnel *restrict t, bool on);

/**
 * Sets the private data pointer of a Teredo tunnel instance.
 * This value is passed to callbacks.
 *
 * @param t Teredo tunnel instance
 * @param opaque private data pointer
 *
 * @return the previous private data pointer value (defaults to NULL)
 */
void *teredo_set_privdata (teredo_tunnel *t, void *opaque);

/**
 * Gets the private data pointer of a Teredo tunnel instance.
 *
 * @param t Teredo tunnel instance
 *
 * @return private data pointer value set by teredo_set_privdata()
 * (defaults to NULL)
 */
void *teredo_get_privdata (const teredo_tunnel *t);

/**
 * Prototype for callback to receive decapsulated IPv6 packets.
 *
 * @param opaque private data pointer, set by teredo_set_privdata()
 * @param data IPv6 header and payload
 * @param len byte length the IPv6 packet
 */
typedef void (*teredo_recv_cb) (void *opaque, const void *data, size_t len);

/**
 * Sets a callback to receive IPv6 packets decapsulated from the Teredo
 * tunnel. If not set, incoming packets are dropped.
 *
 * @note This function must <b>not</b> be used after teredo_transmit() or
 * teredo_run_async() the specified tunnel. That is undefined.
 *
 * @param t Teredo tunnel instance
 * @param cb callback (or NULL to ignore packets)
 */
void teredo_set_recv_callback (teredo_tunnel *restrict t, teredo_recv_cb cb);

/**
 * Transmits a packet coming from the IPv6 Internet, toward a Teredo node
 * (as specified per paragraph 5.4.1). That's what the specification calls
 * “Packet transmission”.
 *
 * It is assumed that the IPv6 packet is valid (if not, it will be dropped by
 * the receiving Teredo peer). It is furthermore assumed that the packet is at
 * least 40 bytes long (room for the IPv6 header and that it is properly
 * aligned.
 *
 * The packet size should not exceed the MTU (1280 bytes by default).
 * In any case, sending will fail if the packets size exceeds 65507 bytes
 * (maximum size for a UDP packet's payload).
 *
 * Thread-safety: This function is thread-safe.
 *
 * @return 0 on success, -1 on error.
 */
int teredo_transmit (teredo_tunnel *restrict t,
                     const struct ip6_hdr *restrict buf, size_t n);

/**
 * Prototype for callback to process ICMPv6 messages generated by the Teredo
 * tunnel.
 *
 * @param opaque private data pointer, set by teredo_set_privdata()
 * @param data ICMPv6 header and payload
 * @param len byte length the ICMPv6 header and payload
 * @param dst IPv6 address toward which the ICMPv6 error is directed
 */
typedef void (*teredo_icmpv6_cb) (void *opaque, const void *data, size_t len,
                                  const struct in6_addr *dst);

/**
 * Registers a callback to emit ICMPv6 messages when the Teredo tunnel wants
 * to report an error back. If not set, error messages are not sent.
 *
 * @note This function must <b>not</b> be used after teredo_transmit() or
 * teredo_run_async() the specified tunnel. That is undefined.
 *
 * @param t Teredo tunnel instance
 * @param cb callback (ot NULL to ignore ICMPv6 errors)
 */
void teredo_set_icmpv6_callback (teredo_tunnel *restrict t,
                                 teredo_icmpv6_cb cb);

/**
 * Prototype for Teredo tunnel readiness event notification.
 * @param opaque private data pointer, set by teredo_set_privdata()
 * @param addr Teredo address assigned to the tunnel
 * @param mtu Maximum Transmission Unit set for the tunnel
 */
typedef void (*teredo_state_up_cb) (void *opaque, const struct in6_addr *addr,
                                    uint16_t mtu);

/**
 * Prototype for Teredo tunnel loss-of-connectivity event notification.
 * @param opaque private data pointer, set by teredo_set_privdata()
 */
typedef void (*teredo_state_down_cb) (void *opaque);

/**
 * Registers callbacks to be called when the Teredo client maintenance
 * procedure detects that the tunnel becomes usable (or has got a new IPv6
 * address, or a new MTU), or unusable respectively.
 * These callbacks are ignored for a Teredo relay tunnel.
 *
 * Any packet sent when the relay/client is down will be ignored.
 * The callbacks function might be called from a separate thread.
 *
 * @note This function must <b>not</b> be used after teredo_transmit() or
 * teredo_run_async() the specified tunnel. That is undefined.
 *
 * @param t Teredo tunnel instance
 * @param up usability event callback (or NULL to ignore event)
 * @param down unusability event callback (or NULL to ignore event)
 */
void teredo_set_state_cb (teredo_tunnel *restrict t, teredo_state_up_cb up,
                          teredo_state_down_cb down);

# ifdef __cplusplus
}
# endif /* ifdef __cplusplus */
#endif /* ifndef MIREDO_TUNNEL_H */
