#!/bin/sh
# $Id$
#
# Miredo start/stop script for Debian GNU/Linux
# Author: Rémi Denis-Courmont <rdenis (at) simphalempin (dot) com>
#
# chkconfig: 345 17 83
# description: Starts and stops the Miredo daemon \
#	       used to provide IPv6 tunneling over UDP through NAT.
#
### BEGIN INIT INFO
# Provides: teredo ipv6
# Requires: $local_fs $network $syslog $time
# Short-Description: Teredo IPv6 tunnel
# Description: Miredo daemon for tunneling of IPv6 through NAT
#	within UDP/IPv4, as specified by the Teredo mechanism.
# Default-Start: 3 4 5
# Default-Stop: 0 1 2 6
### END INIT INFO


PATH=/sbin:/usr/sbin:/bin:/usr/bin
NAME=miredo
DAEMON=/usr/sbin/$NAME
DESC="Teredo IPv6 tunneling daemon"
DAEMON_ARGS=""
PIDFILE=/var/run/$NAME.pid
SCRIPTNAME=/etc/init.d/$NAME

# Source defaults.
[ -r /etc/default/$NAME ] && . /etc/default/$NAME

# Ensure configuration is readable.
test -r /etc/$NAME.conf || exit 0

test -x $DAEMON || exit 0

case "$1" in
  start)
	if [ "x$STARTMIREDO" != "xtrue" ]; then
		echo "STARTMIREDO is set to false in /etc/default/$NAME"
		echo "$DAEMON not starting"
		exit 0
	fi
	echo -n "Starting $DESC: $NAME"
	start-stop-daemon --start --quiet --pidfile $PIDFILE \
		--exec $DAEMON -- $DAEMON_ARGS
	echo "."
	;;
  stop)
	echo -n "Stopping $DESC: $NAME"
	start-stop-daemon --stop --quiet --pidfile $PIDFILE --retry 1
	echo "."
	;;
  reload|force-reload)
	echo -n "Reloading $DESC: $NAME"
	start-stop-daemon --stop --signal 1 --quiet --exec $DAEMON
	echo "."
	;;
  restart)
	$0 stop
	sleep 1
	$0 start
	;;
  *)
	echo "Usage: $SCRIPTNAME {start|stop|restart|reload|force-reload}" >&2
	exit 1
	;;
esac

exit 0

