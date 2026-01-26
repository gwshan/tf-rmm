/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: Copyright TF-RMM Contributors.
 */

#include <assert.h>
#include <buffer.h>
#include <debug.h>
#include <granule.h>
#include <granule_types.h>
#include <rec.h>
#include <smc-rmi.h>
#include <smc.h>
#include <sro_context.h>
#include <status.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <utils_def.h>
#include <xlat_defs.h>

/*
 * Maximum number of entries we allow the address list to have.
 * Note that this implementation does not allow the list to cross
 * granule, so in that case, the operation will just return INCOMPLETE.
 */
#define MAX_LIST_ENTRIES	(GRANULE_SIZE / sizeof(unsigned long))

struct rmi_handles {
	sro_handle_cb cb;
};

#define SRO_HANDLE(_id, _cb)[RMI_HANDLER_ID(SMC_RMI_##_id)] = {		\
	.cb = (_cb)							\
}

struct rmi_handles sro_handles[] = {
	{NULL}
};
COMPILER_ASSERT(ARRAY_SIZE(sro_handles) <= SMC64_NUM_FIDS_IN_RANGE(RMI));

static void rmi_op_dispatch(struct smc_args *args, struct smc_result *res)
{
	struct sro_context *sro = my_sro_ctx();
	unsigned long fid = args->v[0];
	unsigned int handle_id;

	assert(sro != NULL);

	handle_id = RMI_HANDLER_ID(sro->init_command);
	assert(handle_id < ARRAY_SIZE(sro_handles));
	assert(sro_handles[handle_id].cb != NULL);

	if ((fid != SMC_RMI_OP_CANCEL) && (sro->expected_fid != fid)) {
		/*
		 * This is not the expected SRO RMI, so try again.
		 *
		 * In this case we can just return again the values for
		 * the previous command, just setting res->x[1] = 0
		 * as that value is used as 'donated_count' or
		 * 'reclaim_count' for donation or reclaim and we don't
		 * want the host to donate nor reclaim any memory.
		 */

		/* @TODO: Check this behavior with ATG */
		*res = sro->prev_res;
		res->x[1] = 0UL;

		(void)sro_ctx_seal();
		sro->prev_res = *res;
		return;
	}

	sro_handles[handle_id].cb(args, res);

	if (unpack_return_code(res->x[0]).status == RMI_INCOMPLETE) {

		/* Update the memory flags on the SRO context if needed */
		if (unpack_return_code(res->x[0]).status == RMI_INCOMPLETE) {
			sro->can_cancel =
				!!(EXTRACT(RMI_OP_CAN_CANCEL_BIT, res->x[0]));

			if (EXTRACT(RMI_OP_MEM_REQ, res->x[0]) == RMI_OP_MEM_REQ_DONATE) {
				sro->is_contig =
					!!(EXTRACT(RMI_OP_DONATE_MEM_CONTIG, res->x[2]));
			}
		}

		/* Need to return back to Host to continue */
		sro->prev_res = *res;
		(void)sro_ctx_seal();
	} else {
		/* Command finalized */
		sro_ctx_release();
	}
}

static void do_rmi_mem_op(unsigned long fid,
			  unsigned long handle,
			  unsigned long list_addr,
			  unsigned long list_count,
			  struct smc_result *res)
{
	/*
	 * @TODO: Validate the following failure conditions for
	 * RMI_OP_MEM_DONATE.
	 *
	 * 	- complete
	 * 	- mem_state
	 */
	struct smc_args args;

	args.v[0] = fid;
	args.v[1] = handle;
	args.v[2] = list_addr;
	args.v[3] = list_count;
	rmi_op_dispatch(&args, res);
}

/* Return the size requested by an entry on the RmiAddrRangeDesc4KB list */
static unsigned long calc_entry_size(unsigned long entry)
{
	unsigned long blk_size = EXTRACT(RMI_ADDR_RDESC_4K_SZ, entry);

	return ((XLAT_BLOCK_SIZE(XLAT_TABLE_LEVEL_MAX - blk_size)) *
				EXTRACT(RMI_ADDR_RDESC_4K_CNT, entry));
}

/* Return the amount of memory requested in a donate operation */
static unsigned long calc_total_mem(unsigned long *list, unsigned int count)
{
	unsigned long size = 0UL;

	assert(count > 0U);

	for (unsigned int i = 0U; i < count; i++) {
		size += calc_entry_size(*(list + i));
	}

	return size;
}

/*
 * Return the minimum alignment required by a RmiAddrRangeDesc4KB list.
 *
 * Each entry  in the list is an RMI address descriptor that encodes both
 * an address and a sized field.
 * The size field indicates how large a memory region that entry describes
 * (e.g. a 4KB page, a 2MB block or a 1GB block). The minimum alignment is
 * the biggest block size found accross all entries. For instance, if one
 * entry describes a 2MB block and another describes a 4KB page, the minimum
 * alignment would be 2MB.
 */
static unsigned long min_alignment_req(unsigned long *list, unsigned int count)
{
	unsigned long alignment = XLAT_BLOCK_SIZE(XLAT_TABLE_LEVEL_MAX -
			(EXTRACT(RMI_ADDR_RDESC_4K_SZ, (*list))));

	assert(count > 0U);

	for (unsigned int i = 1U; i < count; i++) {
		unsigned long new_alignment = XLAT_BLOCK_SIZE(XLAT_TABLE_LEVEL_MAX -
			(EXTRACT(RMI_ADDR_RDESC_4K_SZ, (*(list + i)))));

		if (new_alignment > alignment) {
			/* Required alignment is larger than the current one */
			alignment = new_alignment;
		}
	}

	return alignment;
}

void smc_op_mem_donate(unsigned long handle,
			unsigned long list_addr,
			unsigned long list_count,
			struct smc_result *res)
{
	bool found;
	struct sro_context *sro;
	struct granule *list_gr;
	unsigned long list_ptr;
	unsigned long entry_addr;
	unsigned long total_xfer_memory;
	unsigned long min_alignment;
	unsigned long list_offset;
	unsigned long list_buffer[MAX_LIST_ENTRIES] __aligned(GRANULE_SIZE);

	if (!ALIGNED(list_addr, sizeof(unsigned long))) {
		res->x[0] = RMI_ERROR_INPUT;
		return;
	}

	found = sro_ctx_find(handle);
	if (!found) {
		res->x[0] = RMI_ERROR_INPUT;
		return;
	}

	sro = my_sro_ctx();
	assert(sro != NULL);

	if (list_count == 0UL) {
		/* Nothing to donate, so just return */
		res->x[0] = (RMI_INCOMPLETE |
			INPLACE(RMI_OP_MEM_REQ, RMI_OP_MEM_REQ_NONE) |
			INPLACE(RMI_OP_CAN_CANCEL_BIT, SRO_CAN_CANCEL_FLAG(sro)));
		res->x[1] = 0UL;
		res->x[2] = 0UL;

		(void)sro_ctx_seal();
		return;
	}

	/*
	 * The list can start at any place inside a given granule (provided
	 * that is aligned to 8 bytes).
	 *
	 * Calculate the offset of the first entry with regards to the start
	 * of the granule where the list is.
	 */
	list_offset = ((list_addr & ~GRANULE_MASK) >> 3U);

	/*
	 * Limit the number of entries to not cross granules.
	 * The command will terminate with INCOMPLETE if there is a granule cross.
	 */
	list_count = (((list_count + list_offset) > MAX_LIST_ENTRIES) ?
				(MAX_LIST_ENTRIES - list_offset) : list_count);

	list_gr = find_lock_granule((list_addr & GRANULE_MASK), GRANULE_STATE_NS);

	if (list_gr == NULL) {
		/* @TODO: Double check this fault cond is in the spec */
		res->x[0] = RMI_ERROR_INPUT;

		(void)sro_ctx_seal();
		return;
	}

	if (sro->is_contig && (list_count > 1UL)) {
		/*
		 * If the memory is contiguous, the list should only have
		 * one entry.
		 */
		/*
		 * @TODO: Is this the right behavior or should we just ignore
		 * all but the first entry?
		 */
		res->x[0] = RMI_ERROR_INPUT;

		(void)sro_ctx_seal();
		return;
	}

	if (!ns_buffer_read(SLOT_NS, list_gr,
			    (list_offset * sizeof(unsigned long)),
			    (list_count * sizeof(unsigned long)),
			    (void *)&list_buffer[0])) {
		res->x[0] = RMI_ERROR_INPUT;

		(void)sro_ctx_seal();
		return;
	}

	granule_unlock(list_gr);

	sro->current_transfer = calc_total_mem(&list_buffer[0], list_count);
	if (sro->current_transfer > sro->transfer_req) {
		res->x[0] = RMI_ERROR_INPUT;
		(void)sro_ctx_seal();
		return;
	}

	/*
	 * Enforce a uniform alignment constraint across all address entries
	 * in the donation list, ensuring they are all aligned to the largest
	 * block size described by any entry in the list.
	 */
	min_alignment = min_alignment_req(&list_buffer[0], list_count);
	for (unsigned long i = 0UL; i < list_count; i++) {
		entry_addr = RMI_ADDR_RDESC_4K_GET_ADDR(list_buffer[i]);

		if (!ALIGNED(entry_addr, min_alignment)) {
			res->x[0] = RMI_ERROR_INPUT;

			(void)sro_ctx_seal();
			return;
		}
	}

	list_ptr = (unsigned long)&list_buffer[0];
	do_rmi_mem_op(SMC_RMI_OP_MEM_DONATE, handle, list_ptr, list_count, res);

	/* Update the amount of memory left to transfer */
	total_xfer_memory = calc_total_mem(&list_buffer[0], res->x[1]);

	/*
	 * Note that at this point the SRO is already sealed. However the
	 * current reference we own still is valid as we got it before sealing.
	 */
	assert(sro->transfer_req >= total_xfer_memory);
	sro->transfer_req -= total_xfer_memory;
	sro->current_transfer = 0UL;
}

void smc_op_mem_reclaim(unsigned long handle,
			unsigned long list_addr,
			unsigned long list_count,
			struct smc_result *res)
{
	struct granule *list_gr;
	unsigned long list_ptr;
	unsigned long list_offset;
	bool found;
	unsigned long list_buffer[MAX_LIST_ENTRIES] __aligned(GRANULE_SIZE);

	if (!ALIGNED(list_addr, sizeof(unsigned long))) {
		res->x[0] = RMI_ERROR_INPUT;
		return;
	}

	/*
	 * The list can start at any place inside a given granule (provided
	 * that is aligned to 8 bytes.
	 *
	 * Calculate the offset of the first entry with regards to the start
	 * of the granule where the list is. We will use this to find out if
	 * we can cross granules when walking the list.
	 */
	list_offset = ((list_addr & ~GRANULE_MASK) >> 3U);

	/*
	 * Limit the number of entries to not cross granules.
	 * The command will terminate with INCOMPLETE if there is a granule cross.
	 */
	list_count = (((list_count + list_offset) > MAX_LIST_ENTRIES) ?
				(MAX_LIST_ENTRIES - list_offset) : list_count);

	list_ptr = (unsigned long)&list_buffer[0];

	found = sro_ctx_find(handle);
	if (!found) {
		res->x[0] = RMI_ERROR_INPUT;
		return;
	}

	do_rmi_mem_op(SMC_RMI_OP_MEM_RECLAIM, handle, list_ptr, list_count, res);

	/*
	 * Ensure that the reclaim handler hasn't provided more entries than
	 * the size of the list.
	 */
	assert(res->x[1] <= list_count);

	list_gr = find_lock_granule((list_addr & GRANULE_MASK), GRANULE_STATE_NS);
	if (list_gr == NULL) {
		/* @TODO: Double check this fault cond is in the spec */
		res->x[0] = RMI_ERROR_INPUT;
		return;
	}

	if (!ns_buffer_write(SLOT_NS, list_gr,
			     (list_offset * sizeof(unsigned long)),
			     (res->x[1] * sizeof(unsigned long)),
			     (void *)&list_buffer[0])) {
		res->x[0] = RMI_ERROR_INPUT;
		return;
	}

	granule_unlock(list_gr);
}

void smc_op_continue(unsigned long flags,
		unsigned long handle,
		struct smc_result *res)
{
	struct smc_args args;
	struct sro_context *sro __unused;
	bool found = sro_ctx_find(handle);

	if (found == false) {
		res->x[0] = RMI_ERROR_INPUT;
		return;
	}

	sro = my_sro_ctx();
	assert(sro != NULL);

	args.v[0] = SMC_RMI_OP_CONTINUE;
	args.v[1] = flags;
	args.v[2] = handle;
	rmi_op_dispatch(&args, res);
}

void smc_op_cancel(unsigned long flags,
		unsigned long handle,
		struct smc_result *res)
{
	struct smc_args args;
	struct sro_context *sro;
	bool found = sro_ctx_find(handle);

	if (found == false) {
		res->x[0] = RMI_ERROR_INPUT;
		return;
	}

	sro = my_sro_ctx();
	assert(sro != NULL);

	if (!sro->can_cancel) {
		res->x[0] = RMI_ERROR_INPUT;
		(void)sro_ctx_seal();
		return;
	}

	args.v[0] = SMC_RMI_OP_CANCEL;
	args.v[1] = flags;
	args.v[2] = handle;
	rmi_op_dispatch(&args, res);
}
