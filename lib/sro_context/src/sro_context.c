/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: Copyright TF-RMM Contributors.
 */

#include <assert.h>
#include <cpuid.h>
#include <rmm_el3_ifc.h>
#include <smc-rmi.h>
#include <spinlock.h>
#include <sro_context.h>
#include <string.h>

static struct sro_context *sro_ctx_table;
static struct cpu_sro_ctx cpu_sro_ctx_table[MAX_CPUS];
static enum sro_state *sro_state_table;
static unsigned long sro_ctx_count;
static bool sro_ctx_initialized;
static spinlock_t sro_spinlock;

static inline void sro_ctx_zero(unsigned int cpuid)
{
	struct sro_context *ctx = cpu_sro_ctx_table[cpuid].ctx;

	assert(cpuid < MAX_CPUS);
	assert(ctx != NULL);

	memset((void *)ctx, 0, sizeof(struct sro_context));
}

void sro_ctx_init(uintptr_t va, size_t sz)
{
	unsigned int max_cpus = rmm_el3_ifc_get_total_cpus();
	uintptr_t state_end __unused;

	assert(sro_ctx_initialized == false);
	assert(va != 0UL);
	assert(sz != (size_t)0);

	(void)sz;

	sro_ctx_count = max_cpus * SRO_CTX_PER_CPU;

	assert(va != 0UL);

	/* Pointer to the sro_ctx structure array */
	sro_ctx_table = (struct sro_context *)va;

	/* Get the pointer to sro_state, porperly aligned */
	sro_state_table = (enum sro_state *)round_up(((uintptr_t)va +
						(sizeof(struct sro_context) * sro_ctx_count)),
						sizeof(enum sro_state));

	state_end = (uintptr_t)sro_state_table +
				(sizeof(enum sro_state) * (uintptr_t)max_cpus);
	assert(state_end < (va + sz));

	sro_ctx_initialized = true;
}

/*
 * Assigns a command context to the current CPU
 * It returns:
 * - RMI_BLOCKED: All SRO contexts are sealed.
 * - RMI_BUSY:    Some SRO contexts are reserved, the rest is sealed.
 * - RMI_SUCCESS: SRO context is assigned to the current CPU.
 */
unsigned long sro_ctx_reserve(unsigned long command, unsigned long xfer,
			      bool can_cancel, bool is_contig)
{
	unsigned int sealed = 0U;
	unsigned int cpuid = my_cpuid();
	unsigned long ret = RMI_BUSY;

	assert(sro_ctx_initialized == true);
	assert(cpu_sro_ctx_table[cpuid].ctx == NULL);

	spinlock_acquire(&sro_spinlock);

	for (unsigned int i = 0U; i < sro_ctx_count; i++) {
		if (sro_state_table[i] == SRO_STATE_FREE) {
			struct sro_context *current_ctx = &sro_ctx_table[i];

			sro_state_table[i] = SRO_STATE_RESERVED;
			cpu_sro_ctx_table[cpuid].ctx = current_ctx;
			cpu_sro_ctx_table[cpuid].op_handler = i;
			sro_ctx_zero(cpuid);
			current_ctx->init_command = command;
			current_ctx->can_cancel = can_cancel;
			current_ctx->is_contig = is_contig;
			current_ctx->transfer_req = xfer;
			ret = RMI_SUCCESS;
			break;
		} else if (sro_state_table[i] == SRO_STATE_SEALED) {
			sealed++;
		}
	}

	if (sealed == sro_ctx_count) {
		ret = RMI_BLOCKED;
	}

	spinlock_release(&sro_spinlock);

	return ret;
}

/*
 * Seals the CPU's Context and returns its identifier.
 */
unsigned int sro_ctx_seal(void)
{
	unsigned int index, cpuid = my_cpuid();
	struct cpu_sro_ctx *current_cpu_ctx = &cpu_sro_ctx_table[cpuid];

	assert(sro_ctx_initialized == true);
	assert(current_cpu_ctx->ctx != NULL);
	index = current_cpu_ctx->op_handler;

	spinlock_acquire(&sro_spinlock);
	assert(sro_state_table[index] != SRO_STATE_FREE);

	sro_state_table[index] = SRO_STATE_SEALED;
	spinlock_release(&sro_spinlock);

	current_cpu_ctx->ctx = NULL;

	return current_cpu_ctx->op_handler;
}

/*
 * Finds the SRO context that matches the input identifier, transition it
 * from SRO_STATE_SEALED to SRO_STATE_RESERVED and assign it to the
 * current CPU.
 */
bool sro_ctx_find(unsigned long op_handle)
{
	bool ret;
	unsigned int cpuid = my_cpuid();
	struct cpu_sro_ctx *current_cpu_ctx = &cpu_sro_ctx_table[cpuid];

	assert(sro_ctx_initialized == true);

	spinlock_acquire(&sro_spinlock);

	if (op_handle >= sro_ctx_count) {
		ret = false;
		goto out;

	}

	if (sro_state_table[op_handle] != SRO_STATE_SEALED) {
		ret = false;
		goto out;
	}

	sro_state_table[op_handle] = SRO_STATE_RESERVED;
	current_cpu_ctx->ctx = &sro_ctx_table[op_handle];
	current_cpu_ctx->op_handler = op_handle;
	ret = true;
out:
	spinlock_release(&sro_spinlock);
	return ret;
}

/*
 * Returns pointer to the CPU's command context.
 */
struct sro_context *my_sro_ctx(void)
{
	unsigned int cpuid = my_cpuid();

	assert(sro_ctx_initialized == true);
	assert(cpu_sro_ctx_table[cpuid].ctx != NULL);
	return cpu_sro_ctx_table[cpuid].ctx;
}

/*
 * Releases the CPU's command context.
 */
void sro_ctx_release(void)
{
	unsigned int index, cpuid = my_cpuid();
	struct cpu_sro_ctx *current_cpu_ctx = &cpu_sro_ctx_table[cpuid];

	assert(sro_ctx_initialized == true);
	assert(current_cpu_ctx->ctx != NULL);

	index = current_cpu_ctx->op_handler;

	spinlock_acquire(&sro_spinlock);

	assert(sro_state_table[index] == SRO_STATE_RESERVED);

	sro_state_table[index] = SRO_STATE_FREE;
	current_cpu_ctx->ctx = NULL;

	spinlock_release(&sro_spinlock);
}

/*
 * Configure the next expected FID on the SRO flow.
 */
void sro_ctx_next_cmd(unsigned long fid)
{
	struct sro_context *sro = my_sro_ctx();

	assert(sro_ctx_initialized == true);
	assert((fid == SMC_RMI_OP_CONTINUE) || ((fid >= SMC_RMI_OP_MEM_DONATE) &&
						(fid <= SMC_RMI_OP_CANCEL)));
	sro->expected_fid = fid;
}
