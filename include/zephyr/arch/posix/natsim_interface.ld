/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

SECTION_PROLOGUE (.natsim_if,,)
{
	KEEP(*(.natsim_if));
	KEEP(*(.natsim_if.*));
}
