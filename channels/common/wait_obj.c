/*
   Copyright (c) 2009-2010 Jay Sorg

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>
#include "wait_obj.h"

#define LOG_LEVEL 1
#define LLOG(_level, _args) \
  do { if (_level < LOG_LEVEL) { printf _args ; } } while (0)
#define LLOGLN(_level, _args) \
  do { if (_level < LOG_LEVEL) { printf _args ; printf("\n"); } } while (0)

static int g_wait_obj_seq = 0;

struct wait_obj
{
	int sock;
	struct sockaddr_un sa;
};

struct wait_obj *
wait_obj_new(const char * name)
{
	struct wait_obj * obj;
	int pid;
	int size;

	obj = (struct wait_obj *) malloc(sizeof(struct wait_obj));

	pid = getpid();
	obj->sock = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (obj->sock < 0)
	{
		LLOGLN(0, ("init_wait_obj: socket failed"));
		free(obj);
		return NULL;
	}
	obj->sa.sun_family = AF_UNIX;
	size = sizeof(obj->sa.sun_path) - 1;
	snprintf(obj->sa.sun_path, size, "/tmp/%s%8.8x.%d", name, pid, g_wait_obj_seq++);
	obj->sa.sun_path[size] = 0;
	size = sizeof(obj->sa);
	if (bind(obj->sock, (struct sockaddr*)(&(obj->sa)), size) < 0)
	{
		LLOGLN(0, ("init_wait_obj: bind failed"));
		close(obj->sock);
		unlink(obj->sa.sun_path);
		free(obj);
		return NULL;
	}
	return obj;
}

int
wait_obj_free(struct wait_obj * obj)
{
	if (obj)
	{
		if (obj->sock != -1)
		{
			close(obj->sock);
			obj->sock = -1;
			unlink(obj->sa.sun_path);
		}
		free(obj);
	}
	return 0;
}

int
wait_obj_is_set(struct wait_obj * obj)
{
	fd_set rfds;
	int num_set;
	struct timeval time;

	FD_ZERO(&rfds);
	FD_SET(obj->sock, &rfds);
	memset(&time, 0, sizeof(time));
	num_set = select(obj->sock + 1, &rfds, 0, 0, &time);
	return (num_set == 1);
}

int
wait_obj_set(struct wait_obj * obj)
{
	int len;

	if (wait_obj_is_set(obj))
	{
		return 0;
	}
	len = sendto(obj->sock, "sig", 4, 0, (struct sockaddr*)(&(obj->sa)),
		sizeof(obj->sa));
	if (len != 4)
	{
		LLOGLN(0, ("set_wait_obj: error"));
		return 1;
	}
	return 0;
}

int
wait_obj_clear(struct wait_obj * obj)
{
	int len;

	while (wait_obj_is_set(obj))
	{
		len = recvfrom(obj->sock, &len, 4, 0, 0, 0);
		if (len != 4)
		{
			LLOGLN(0, ("chan_man_clear_ev: error"));
			return 1;
		}
	}
	return 0;
}

int
wait_obj_select(struct wait_obj ** listobj, int numobj, int * listr, int numr,
	int timeout)
{
	int max;
	int rv;
	int index;
	int sock;
	struct timeval time;
	struct timeval * ptime;
	fd_set fds;

	ptime = 0;
	if (timeout >= 0)
	{
		time.tv_sec = timeout / 1000;
		time.tv_usec = (timeout * 1000) % 1000000;
		ptime = &time;
	}
	max = 0;
	FD_ZERO(&fds);
	if (listobj)
	{
		for (index = 0; index < numobj; index++)
		{
			sock = listobj[index]->sock;
			FD_SET(sock, &fds);
			if (sock > max)
			{
				max = sock;
			}
		}
	}
	if (listr)
	{
		for (index = 0; index < numr; index++)
		{
			sock = listr[index];
			FD_SET(sock, &fds);
			if (sock > max)
			{
				max = sock;
			}
		}
	}
	rv = select(max + 1, &fds, 0, 0, ptime);
	return rv;
}

