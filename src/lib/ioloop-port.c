/*
 * Solaris and Illumos port based ioloop handler.
 *
 * Copyright (c) 2017 Tavy <sabit@sabit.space>
 */

#include "lib.h"

#ifdef IOLOOP_PORT

#include "array.h"
#include "fd-close-on-exec.h"
#include "ioloop-private.h"

#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <poll.h>
#include <port.h>

struct ioloop_handler_context {
	int port;
};

void io_loop_handler_init(struct ioloop *ioloop, unsigned int initial_fd_count ATTR_UNUSED)
{
	struct ioloop_handler_context *ctx;

	ioloop->handler_context = ctx = i_new(struct ioloop_handler_context, 1);

	ctx->port = port_create();
	if (ctx->port < 0)
		i_fatal("port_create() in io_loop_handler_init() failed: %m");
	fd_close_on_exec(ctx->port, TRUE);
}

void io_loop_handler_deinit(struct ioloop *ioloop)
{
	if (close(ioloop->handler_context->port) < 0)
		i_error("close(port) in io_loop_handler_deinit() failed: %m");
	i_free(ioloop->handler_context);
}

void io_loop_handle_add(struct io_file *io)
{
	struct ioloop_handler_context *ctx = io->io.ioloop->handler_context;

	uintptr_t events = 0;
	if ((io->io.condition & IO_READ) != 0) {
		events |= POLLIN | POLLHUP;
	}
	if ((io->io.condition & IO_WRITE) != 0) {
		events |= POLLOUT;
	}
	if ((io->io.condition & IO_ERROR) != 0) {
		events |= POLLHUP;
	}

	port_associate(ctx->port, PORT_SOURCE_FD, io->fd, events, io);
}

void io_loop_handle_remove(struct io_file *io, bool closed ATTR_UNUSED)
{
	struct ioloop_handler_context *ctx = io->io.ioloop->handler_context;

	io->io.condition = 0;
	port_dissociate(ctx->port, PORT_SOURCE_FD, io->fd);
}

void io_loop_handler_run_internal(struct ioloop *ioloop)
{
	struct ioloop_handler_context *ctx = ioloop->handler_context;
	struct timeval tv;
	timespec_t ts;
	struct io_file *io;
	uint_t n, i;

	/* get the time left for next timeout task */
	io_loop_get_wait_time(ioloop, &tv);
	ts.tv_sec = tv.tv_sec;
	ts.tv_nsec = tv.tv_usec * 1000;

	if (port_getn(ctx->port, NULL, 0, &n, NULL) != 0)
		i_panic("port_getn() failed: %m");

	n = I_MAX(n, 1);
	port_event_t events[n];

	if (port_getn(ctx->port, events, n, &n, &ts) != 0 && errno != ETIME)
		i_panic("port_getn() failed: %m");

	/* execute timeout handlers */
	io_loop_handle_timeouts(ioloop);

	for (i = 0; i < n; i++) {
		io = (void *)events[i].portev_user;

		/* callback is NULL if io_remove() was already called */
		if (io->io.callback != NULL)
			io_loop_call_io(&io->io);

		if (io->io.condition)
			io_loop_handle_add(io);
	}
}

#endif
