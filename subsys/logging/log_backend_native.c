/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2018 Intel Corporation
 * Copyright (c) 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <logging/log_backend.h>
#include <logging/log_core.h>
#include <logging/log_msg.h>
#include <logging/log_output.h>
#include "posix_trace.h"

#define _STDOUT_BUF_SIZE 256
static char stdout_buff[_STDOUT_BUF_SIZE];
static int n_pend; /* Number of pending characters in buffer */

static void preprint_char(int c)
{
	int printnow = 0;

	if (c == '\r') {
		/* Discard carriage returns */
		return;
	}
	if (c != '\n') {
		stdout_buff[n_pend++] = c;
		stdout_buff[n_pend] = 0;
	} else {
		printnow = 1;
	}

	if (n_pend >= _STDOUT_BUF_SIZE - 1) {
		printnow = 1;
	}

	if (printnow) {
		posix_print_trace("%s\n", stdout_buff);
		n_pend = 0;
		stdout_buff[0] = 0;
	}
}

static u8_t buf[_STDOUT_BUF_SIZE];

int char_out(u8_t *data, size_t length, void *ctx)
{
	ARG_UNUSED(ctx);

	for (size_t i = 0; i < length; i++) {
		preprint_char(data[i]);
	}

	return length;
}

static struct log_output_ctx ctx = {
	.func = char_out,
	.data = buf,
	/*
	 * Note: The logging code does not seem to support backends with buffers
	 * longer than 1 char yet, when that is fixed, increase to buf size
	 */
	.length = 1,
	.offset = 0
};

static void put(const struct log_backend *const backend,
		struct log_msg *msg)
{
	log_msg_get(msg);

	u32_t flags = 0;

	flags |= posix_trace_over_tty(0);
	flags |= LOG_OUTPUT_FLAG_FORMAT_TIMESTAMP;

	log_output_msg_process(msg, &ctx, flags);

	log_msg_put(msg);

}

static void panic(struct log_backend const *const backend)
{
	/* Nothing to be done, this backend can always process logs */
}

const struct log_backend_api log_backend_native_api = {
	.put = put,
	.panic = panic,
};
