/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: Copyright TF-RMM Contributors.
 */

#include <assert.h>
#include <cpuid.h>
#include <runtime_ctrl.h>
#include <smc-rmi.h>
#include <smc.h>

/* List of current FID being executed by each CPU */
static unsigned long current_fid[MAX_CPUS] = {0UL};

struct rtc_handlers {
	rtc_check_exit_cb cb;
};

#define RTC_HANDLER(_id, _cb)[RMI_HANDLER_ID(SMC_RMI_##_id)] = {	\
	.cb = (_cb)							\
}

struct rtc_handlers rtc_handlers[] = {
	{NULL}
};
COMPILER_ASSERT(ARRAY_SIZE(rtc_handlers) <= SMC64_NUM_FIDS_IN_RANGE(RMI));


void rtc_set_command(unsigned long fid)
{
	assert(current_fid[my_cpuid()] == 0UL);
	assert(fid < SMC64_NUM_FIDS_IN_RANGE(RMI));

	current_fid[my_cpuid()] = fid;
}

bool rtc_exit(void *args)
{
	unsigned int handler_id =
		(unsigned int)RMI_HANDLER_ID(current_fid[my_cpuid()]);
	bool retval;

	assert(handler_id < ARRAY_SIZE(rtc_handlers));
	assert(rtc_handlers[handler_id].cb != NULL);

	retval = rtc_handlers[handler_id].cb(args);

	if (retval) {
		/*
		 * Reset the FID for the current CPU. It is the CONTINUE
		 * handler for the current operation responsibility to call
		 * rtc_set_command() again upon entry if later queries for
		 * exit are needed.
		 */
		current_fid[my_cpuid()] = 0UL;
	}

	return retval;
}
