/*
 * Copyright © 2010 Alexander Kerner <lunohod@openinkpot.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <liblops.h>

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

#define EDICT "edict"

/*
 * /tmp/.ecore-$ENV{'USER'}/[name]/[port]
 */

char *
service_path(const char *name, int port)
{
    char *user = getenv("USER");
    if (!user)
        user = getenv("LOGNAME");
    if (!user) {
        errno = EINVAL;
        return NULL;
    }

    char *ret;
    if (asprintf(&ret, "/tmp/.ecore_service-%s/%s/%d", user, name, port) ==
        -1)
        return NULL;

    return ret;
}

int
send_text(const char *text)
{
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s == -1) {
        perror("open socket");
        return 1;
    }

    char *path = service_path(EDICT, 0);
    if (!path)
        return 1;

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, UNIX_PATH_MAX - 1);

    free(path);

    if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("connect");
        close(s);
        return 1;
    }

    if (writen(s, text, strlen(text)) < 0) {
        perror("writen");
        close(s);
        return 1;
    }

    if (close(s) != 0) {
        perror("close socket");
        return 0;
    }

    return 0;
}

int
main(int argc, char **argv)
{
    char *str = "";
    if(argc > 1)
        str = argv[1];

    if (send_text(str) != 0)
        execlp(EDICT, EDICT, str, NULL);

    return 0;
}
