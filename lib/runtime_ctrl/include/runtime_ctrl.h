/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: Copyright TF-RMM Contributors.
 */

#ifndef RUNTIME_CTL_H
#define RUNTIME_CTL_H

#include <stdbool.h>

/*
 * The `runtime control` library monitors specific conditions on RMI handlesr
 * and decides if it is the time to return to the Host.
 * Usage:
 *  - rtc_start:       The RMM should call it at the start of the running.
 *  - rtc_set_command: The RMM  should call it once it knows what handler
 *		       is running. For rmi_op_*, this is after the SRO context
 *		       is open.
 *  - rtc_exit:        The handlers should call it periodically to check if it
 *		       is time to return to the Host.
 */

/*
 * Callback prototype to be invoked to check if a command can continue or not.
 *
 *	Input:
 *		- args: Pointer to a generic data structure passed down from
 *			the caller to rtc_exit(). There is no sanitisation
 *			of the input so it is expected that the caller of
 *			rtc_exit() passes a pointer to a data structure which
 *			is compatible with the one expected by the callback.
 *			The pointer can be NULL if no arguments are needed.
 *
 *	Return: 'True' if the command must exit (e.g. RMI_CONTINUE) or 'False'
 *		otherwise.
 */
typedef bool (*rtc_check_exit_cb)(void *args);

/*
 * Set the current command being processed by the current CPU.
 * If a command will need to query the library, this API will need to be
 * called upon RMI entry as well as on its continue handlers.
 */
void rtc_set_command(unsigned long fid);

/*
 * Query if a command needs to exit (e.g with RMI_CONTINUE) or can
 * continue execution.
 *
 *	Input:
 *		- args: Pointer to a generic data structure which
 *			will be passed to the callbacks registered for
 *			the current command in progress.
 *			Note that there is no sanitisation of the input
 *			so it is expected that the caller passes a pointer
 *			to the same structure type which will be used as
 *			parameter for the callbacks registered with
 *			rtc_register_callback(). Note that the pointer is
 *			allowed to be NULL if no args are required.
 *
 *	Return: 'True' if the command must exit (e.g. RMI_CONTINUE) or 'False'
 *		if it can continue execution.
 */
bool rtc_exit(void *args);

#endif /* RUNTIME_CTL_H */
