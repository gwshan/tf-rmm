/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: Copyright TF-RMM Contributors.
 */

#include <app_common.h>
#include <console.h>
#include <debug.h>
#include <el0_app_helpers.h>
#include <errno.h>
#include <limits.h>

int app_console_putc(int character, const struct console *console);
static void app_console_flush(const struct console *console);
void init_console(void);
void mbedtls_exit_panic(unsigned int reason);

static struct console app_console = {
	.flush = app_console_flush,
	.putc = app_console_putc
};

int app_console_putc(int character, const struct console *console)
{
	signed char *shared_buf = (signed char *)get_shared_mem_start();
	unsigned long ret;

	(void) console;
	shared_buf[0] = (signed char)character;
	ret = el0_app_service_call(APP_SERVICE_PRINT, 1, 0, 0, 0);

	if (ret > (unsigned long)INT_MAX) {
		return -1;
	} else {
		return (int)ret;
	}

}

static void app_console_flush(const struct console *console)
{
	(void)console;
	/* Do nothing */
}

void init_console(void)
{
	(void)console_register(&app_console);
}

/* Used by Mbed TLS buffer alloc */
void mbedtls_exit_panic(unsigned int reason)
{
	(void) reason;
	panic();
}
