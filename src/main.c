/*
 * main.c - Unix Teredo server & relay implementation
 *          command line handling and core functions
 */

/***********************************************************************
 *  Copyright © 2004-2007 Rémi Denis-Courmont.                         *
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif


#include <stdio.h>
#include <stdlib.h> /* strtoul(), clearenv() */
#include <string.h> /* strerror() */

#include <locale.h>

#include <sys/types.h>
#include <sys/time.h> /* for <sys/resource.h> */
#include <sys/resource.h> /* getrlimit() */
#include <sys/stat.h> /* fstat(), mkdir */
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h> /* errno */
#include <fcntl.h> /* O_RDONLY */
#ifdef HAVE_SYS_CAPABILITY_H
# include <sys/capability.h>
#endif

#include <pwd.h> /* getpwnam() */
#include <grp.h> /* setgroups() */

#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#include "miredo.h"

/*
 * RETURN VALUES:
 * 0: ok
 * 1: I/O error
 * 2: command line syntax error
 */

static int
quick_usage (const char *path)
{
	fprintf (stderr, _("Try \"%s -h | more\" for more information.\n"),
	         path);
	return 2;
}


static int
usage (const char *path)
{
	printf (_(
"Usage: %s [OPTIONS] [SERVER_NAME]\n"
"Creates a Teredo tunneling interface for encapsulation of IPv6 over UDP.\n"
"\n"
"  -c, --config     specify an configuration file\n"
"  -f, --foreground run in the foreground\n"
"  -h, --help       display this help and exit\n"
"  -p, --pidfile    override the location of the PID file\n"
"  -u, --user       override the user to set UID to\n"
"  -V, --version    display program version and exit\n"), path);
	return 0;
}


extern int
miredo_version (void)
{
#ifndef VERSION
# define VERSION "unknown version"
#endif
	printf (_("Miredo: Teredo IPv6 tunneling software %s (%s)\n"),
	        VERSION, PACKAGE_HOST);
	puts (_("Written by Remi Denis-Courmont.\n"));

	printf (_("Copyright (C) 2004-%u Remi Denis-Courmont\n"
"This is free software; see the source for copying conditions.\n"
"There is NO warranty; not even for MERCHANTABILITY or\n"
"FITNESS FOR A PARTICULAR PURPOSE.\n"), 2006);
        return 0;
}


static int
error_dup (int opt, const char *already, const char *additionnal)
{
	fprintf (stderr, _(
"Duplicate parameter \"%s\" for option -%c\n"
"would override previous value \"%s\".\n"),
		 additionnal, opt, already);
	return 2;
}


#if 0
static int
error_qty (int opt, const char *qty)
{
	fprintf (stderr, _(
"Invalid number (or capacity exceeded) \"%s\" for option -%c\n"), qty, opt);
	return 2;
}
#endif


static int
error_extra (const char *extra)
{
	fprintf (stderr, _("%s: unexpected extra parameter\n"), extra);
	return 2;
}


static int
error_errno (const char *str)
{
	fprintf (stderr, _("Error (%s): %s\n"), str, strerror (errno));
	return -1;
}


/**
 * Creates a Process-ID file.
 */
#ifndef O_NOFOLLOW
# define O_NOFOLLOW 0
#endif
static int
create_pidfile (const char *path)
{
	int fd = open (path, O_WRONLY|O_CREAT|O_NOFOLLOW|O_CLOEXEC, 0644);
	if (fd == -1)
		return -1;

	struct stat s;
	char buf[4 * sizeof (int)];
	int len = snprintf (buf, sizeof (buf), "%u", (unsigned)getpid ());

	/* Locks and writes the PID file */
	if (fstat (fd, &s) == 0
	 && (errno = EACCES, S_ISREG (s.st_mode))
	 && lockf (fd, F_TLOCK, 0) == 0
	 && ftruncate (fd, 0) == 0
	 && write (fd, buf, len) == len
	 && fdatasync (fd) == 0)
		return fd;

	close (fd);
	return -1;
}


#ifdef MIREDO_DEFAULT_USERNAME
static void
setuid_notice (void)
{
	fputs (_(
"That is usually an indication that you are trying to start\n"
"the program as an user with insufficient system privileges.\n"
"This program should normally be started by root.\n"), stderr);
}
#endif


/**
 * Initialize daemon context.
 */
static int
init_security (const char *username)
{
	int val;

	(void)umask (022);
	if (chdir ("/"))
		return -1;

	/*
	 * Make sure 0, 1 and 2 are open.
	 */
	val = dup (2);
	if (val < 3)
		return -1;
	close (val);

	/* Clears environment */
	(void)clearenv ();

#ifdef MIREDO_DEFAULT_USERNAME
	/* Determines unpriviledged user */
	errno = 0;
	struct passwd *pw = getpwnam (username);
	if (pw == NULL)
	{
		fprintf (stderr, _("User \"%s\": %s\n"), username,
		         errno ? strerror (errno) : _("User not found"));
		return -1;
	}

	if (pw->pw_uid == 0)
	{
		fputs (_("Error: This program is not supposed to keep root\n"
			"privileges. That is potentially very dangerous\n"
			"(all the more as it has never been externally audited).\n"),
			stderr);
		return -1;
	}
	unpriv_uid = pw->pw_uid;

	/* Ensure we have root privilege before initialization */
	if (seteuid (0)
	/* Unpriviledged group */
	 || setgid (pw->pw_gid)
	 || initgroups (username, pw->pw_gid))
	{
		fprintf (stderr, _("SetUID to root: %s\n"), strerror (errno));
		setuid_notice ();
		return -1;
	}

#else
	(void)username;
#endif /* MIREDO_DEFAULT_USERNAME */


#ifdef HAVE_LIBCAP
	/* POSIX.1e capabilities support */
	cap_t s = cap_init ();
	if (s == NULL)
		return error_errno ("cap_init"); // unlikely

	static cap_value_t caps[] =
	{
		CAP_KILL, // required by the signal handler
		CAP_SETUID
	};
	cap_set_flag (s, CAP_PERMITTED, 2, caps, CAP_SET);
	cap_set_flag (s, CAP_EFFECTIVE, 2, caps, CAP_SET);

	cap_set_flag (s, CAP_PERMITTED, miredo_capc,
	              (cap_value_t *)miredo_capv, CAP_SET);
	cap_set_flag (s, CAP_EFFECTIVE, miredo_capc,
	              (cap_value_t *)miredo_capv, CAP_SET);

	val = cap_set_proc (s);
	cap_free (s);

	if (val)
	{
		error_errno ("cap_set_proc");
		setuid_notice ();
		return -1;
	}
#endif

	return 0;
}

static int start_daemon (const char *pidfile)
{
	int fds[2];

	if (pipe (fds))
		exit (1);

	pid_t pid = fork ();
	switch (pid)
	{
		case -1:
			fprintf (stderr, _("Error (%s): %s\n"), "fork", strerror (errno));
			exit (1);

		case 0:
			close (fds[0]);
			setsid ();
			break;

		default:
		{
			unsigned char val;

			close (fds[1]);
			if (read (fds[0], &val, 1) != 1)
				val = 1;
			exit (val);
		}
	}

	/* Opens PID file */
	int fd = create_pidfile (pidfile);
	if (fd == -1)
	{
		fprintf (stderr, _("Cannot create PID file %s:\n %s\n"),
		         pidfile, strerror (errno));
		if ((errno == EAGAIN) || (errno == EACCES))
			fprintf (stderr, "%s\n",
			         _("Please make sure another instance of the program is "
				   "not already running."));
		exit (1);
	}

	/* Detaches */
	if (freopen ("/dev/null", "r", stdin) == NULL
	 || freopen ("/dev/null", "w", stdout) == NULL
	 || freopen ("/dev/null", "w", stderr) == NULL
	 || write (fds[1], &(unsigned char){ 0 }, 1) != 1)
		exit (1);

	close (fds[1]);
	return fd;
}


int miredo_main (int argc, char *argv[])
{
	const char *username = NULL, *conffile = NULL, *servername = NULL,
	           *pidfile = NULL;
	struct
	{
		unsigned foreground:1; /* Run in the foreground */
	} flags;

	static const struct option opts[] =
	{
		{ "conf",       required_argument, NULL, 'c' },
		{ "config",     required_argument, NULL, 'c' },
		{ "foreground", no_argument,       NULL, 'f' },
		{ "help",       no_argument,       NULL, 'h' },
		{ "pidfile",    required_argument, NULL, 'p' },
		{ "user",       required_argument, NULL, 'u' },
		{ "username",   required_argument, NULL, 'u' },
		{ "version",    no_argument,       NULL, 'V' },
		{ NULL,         no_argument,       NULL, '\0'}
	};

	setlocale (LC_ALL, "");
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain (PACKAGE);

#define ONETIME_SETTING( setting ) \
	if (setting != NULL) \
		return error_dup (c, optarg, setting); \
	else \
		setting = optarg;

	memset (&flags, 0, sizeof (flags));

	int c;
	while ((c = getopt_long (argc, argv, "c:fhp:u:V", opts, NULL)) != -1)
		switch (c)
		{

			case 'c':
				ONETIME_SETTING (conffile);
				break;

			case 'f':
				flags.foreground = 1;
				break;

			case 'h':
				return usage (argv[0]);

			case 'p':
				ONETIME_SETTING (pidfile);
				break;

			case 'u':
				ONETIME_SETTING (username);
				break;

			case 'V':
				return miredo_version ();

			case '?':
			default:
				return quick_usage (argv[0]);
		}

	if (optind < argc)
		servername = argv[optind++];

	if (optind < argc)
		return error_extra (argv[optind]);

#ifdef MIREDO_DEFAULT_USERNAME
	if (username == NULL)
		username = MIREDO_DEFAULT_USERNAME;
#else
	if (username != NULL)
		return error_extra (username);
#endif

	size_t str_len = 0;
	if (conffile == NULL)
		str_len = sizeof (SYSCONFDIR"/miredo/.conf")
				+ strlen (miredo_name);

	char conffile_buf[str_len];
	if (conffile == NULL)
	{
		snprintf (conffile_buf, str_len, SYSCONFDIR"/miredo/%s.conf",
		          miredo_name);
		conffile = conffile_buf;
	}

	/* Check if config file is present */
	if ((servername == NULL) && access (conffile, R_OK))
	{
		fprintf (stderr, _("Reading configuration from %s: %s\n"),
				conffile, strerror (errno));
		return 1;
	}

	if (init_security (username))
		return 1;
	else
	{
		int fd = socket (AF_INET6, SOCK_DGRAM, 0);
		if (fd == -1)
		{
			fprintf (stderr, _("IPv6 stack not available: %s\n"),
			         strerror (errno));
			return 1;
		}
	        close (fd);
	}

	char pidfile_buf[(pidfile != NULL)
		? 0 : sizeof (LOCALSTATEDIR"/run/" ".pid") + strlen (miredo_name)];
	int fd = -1;

	if (!flags.foreground)
	{
		sprintf (pidfile_buf, LOCALSTATEDIR"/run/%s.pid", miredo_name);
		pidfile = pidfile_buf;
		fd = start_daemon (pidfile);
	}

	c = miredo (conffile, servername, fd);

	if (fd != -1)
	{
		unlink (pidfile);
		close (fd);
	}

	exit (c ? 1 : 0);
}
