/*
 * ISC License
 *
 * Copyright (c) 2021, Tommi Leino <namhas@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <X11/Xlib.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

static void reparent(const char *);
static int manageable(Window);

static Display *dpy;
static Window vbox;
static size_t nclients;

#define HEIGHT 200

static void
open_dpy()
{
	char *denv;

	denv = getenv("DISPLAY");
	if (denv == NULL && errno != 0)
		err(1, "getenv DISPLAY");
	dpy = XOpenDisplay(denv);
	if (dpy == NULL) {
		if (denv == NULL)
			errx(1, "X11 connection failed; "
			    "DISPLAY environment variable not set?");
               	else
                       	errx(1, "failed X11 connection to '%s'", denv);
	}
}

static int
manageable(Window window)
{
	XWindowAttributes wa;
	int mapped, redirectable;

	XGetWindowAttributes(dpy, window, &wa);
	mapped = (wa.map_state != IsUnmapped);
	redirectable = (wa.override_redirect != True);

	if (!redirectable)
		return -1;
	if (mapped)
		return 1;
	else
		return 2;
}

static void
reparent_window(Window window)
{
	assert(vbox != 0);

	printf("Reparenting %lx\n", window);
	XReparentWindow(dpy, window, vbox, 0, 0);
	XSync(dpy, False);
	XMoveWindow(dpy, window, 0, HEIGHT * nclients);
	XResizeWindow(dpy, window, 200, HEIGHT);
	XMapSubwindows(dpy, vbox);
	XRaiseWindow(dpy, window);
	XSync(dpy, False);
	nclients++;
}

static void
create_vbox()
{
	int x, y, w, h;
	XSetWindowAttributes a;
	unsigned long v;

	w = 200;
	h = 800;
	x = 0;
	y = 0;
	v = CWBackPixel;
	a.background_pixel = BlackPixel(dpy, DefaultScreen(dpy));
	vbox = XCreateWindow(dpy, DefaultRootWindow(dpy), x, y, w, h, 0,
	    CopyFromParent, InputOutput, CopyFromParent, v, &a);
	XStoreName(dpy, vbox, "vbox");
	XMapWindow(dpy, vbox);
	XRaiseWindow(dpy, vbox);
}

static void
reparent(const char *wid)
{
	Window root, parent, *children;
	int i, m;
	unsigned int nchildren;
	char buf[64];

	if (XQueryTree(dpy, DefaultRootWindow(dpy),
	               &root, &parent, &children, &nchildren) == 0) {
		warnx("did not capture %s", wid);
		return;
	}

	for (i = 0; i < nchildren; i++) {
		if ((m = manageable(children[i]))) {
			snprintf(buf, sizeof(buf), "0x%lx", children[i]);
			if (strcmp(buf, wid) == 0) {
				if (m == 1) {
					XUnmapWindow(dpy, children[i]);
					XSync(dpy, False);
				}
				reparent_window(children[i]);
			} else {
				printf("Did not match %s\n", buf);
			}
		}
	}

	if (nchildren > 0)
		XFree(children);
}

int
main(int argc, char **argv)
{
	int running;
	XEvent event;

	open_dpy();
	create_vbox();

	if (argc < 2) {
		fprintf(stderr, "usage: %s WindowID ...\n", *argv);
		return 1;
	}

	argc--;
	while (argc--)
		reparent(*++argv);

	XSync(dpy, False);
	XSelectInput(dpy, vbox, SubstructureNotifyMask);

	running = 1;
	while (running) {
		XNextEvent(dpy, &event);
	}

	XSync(dpy, False);
	XCloseDisplay(dpy);
	return 0;
}
