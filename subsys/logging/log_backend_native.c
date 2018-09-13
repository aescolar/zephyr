/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stddef.h>
#include <logging/log_backend.h>
#include <logging/log_core.h>
#include <logging/log_msg.h>
#include <logging/log_output.h>
#include <device.h>
#include <uart.h>

static u8_t buf[1024];

int char_out(u8_t *data, size_t length, void *ctx)
{
	ARG_UNUSED(ctx);

	for (size_t i = 0; i < length; i++) {
		putchar(data[i]);
	}

	return length;
}

static struct log_output_ctx ctx = {
	.func = char_out,
	.data = buf,
	.length = 1024,
	.offset = 0
};

static void put(const struct log_backend *const backend,
		struct log_msg *msg)
{
	log_msg_get(msg);

	u32_t flags = 0;

	if (IS_ENABLED(CONFIG_LOG_BACKEND_UART_SHOW_COLOR)) {
		flags |= LOG_OUTPUT_FLAG_COLORS;
	}

	if (IS_ENABLED(CONFIG_LOG_BACKEND_UART_FORMAT_TIMESTAMP)) {
		flags |= LOG_OUTPUT_FLAG_FORMAT_TIMESTAMP;
	}

	log_output_msg_process(msg, &ctx, flags);

	log_msg_put(msg);

}

static void panic(struct log_backend const *const backend)
{
	/* Nothing to be done, we always process in place in this backend */
}

const struct log_backend_api log_backend_native_api = {
	.put = put,
	.panic = panic,
};
