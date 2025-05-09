/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: Copyright TF-RMM Contributors.
 */

#include <buffer.h>
#include <debug.h>
#include <dev_granule.h>
#include <granule.h>
#include <rmm_el3_ifc.h>
#include <smc-handler.h>
#include <smc-rmi.h>
#include <smc.h>

#ifdef RMM_V1_1
static unsigned long dev_granule_delegate(unsigned long addr)
{
	enum dev_coh_type type;

	/* Try to find device granule */
	struct dev_granule *g = find_dev_granule(addr, &type);

	if (g == NULL) {
		return RMI_ERROR_INPUT;
	}

	if (!dev_granule_lock_on_state_match(g, DEV_GRANULE_STATE_NS)) {
		return RMI_ERROR_INPUT;
	}

	/*
	 * It is possible that the device granule was delegated by EL3
	 * to Secure on request from SPM and hence this request can fail.
	 */
	if (rmm_el3_ifc_gtsi_delegate(addr) != SMC_SUCCESS) {
		dev_granule_unlock(g);
		return RMI_ERROR_INPUT;
	}

	dev_granule_set_state(g, DEV_GRANULE_STATE_DELEGATED);
	dev_granule_unlock(g);
	return RMI_SUCCESS;
}

static unsigned long dev_granule_undelegate(unsigned long addr)
{
	enum dev_coh_type type;

	/* Try to find device granule */
	struct dev_granule *g = find_dev_granule(addr, &type);

	if (g == NULL) {
		return RMI_ERROR_INPUT;
	}

	if (!dev_granule_lock_on_state_match(g, DEV_GRANULE_STATE_DELEGATED)) {
		return RMI_ERROR_INPUT;
	}

	/*
	 * A delegated device granule should only be undelegated on request from RMM.
	 * If this call fails, we have an unrecoverable error in EL3/RMM.
	 */
	if (rmm_el3_ifc_gtsi_undelegate(addr) != SMC_SUCCESS) {
		ERROR("Granule 0x%lx undelegate call failed\n", addr);
		dev_granule_unlock(g);
		panic();
	}

	dev_granule_set_state(g, DEV_GRANULE_STATE_NS);
	dev_granule_unlock(g);
	return RMI_SUCCESS;
}
#else
static unsigned long dev_granule_delegate(unsigned long addr)
{
	(void)addr;

	return RMI_ERROR_INPUT;
}

static unsigned long dev_granule_undelegate(unsigned long addr)
{
	(void)addr;

	return RMI_ERROR_INPUT;
}
#endif /* RMM_V1_1 */

unsigned long smc_granule_delegate(unsigned long addr)
{
	/* Try to find memory granule */
	struct granule *g = find_granule(addr);

	if (g != NULL) {
		if (!granule_lock_on_state_match(g, GRANULE_STATE_NS)) {
			return RMI_ERROR_INPUT;
		}

		/*
		 * It is possible that the memory granule was delegated by EL3
		 * to Secure on request from SPM and hence this request can fail.
		 */
		if (rmm_el3_ifc_gtsi_delegate(addr) != SMC_SUCCESS) {
			granule_unlock(g);
			return RMI_ERROR_INPUT;
		}

		granule_set_state(g, GRANULE_STATE_DELEGATED);
		buffer_granule_memzero(g, SLOT_DELEGATED);
		granule_unlock(g);
		return RMI_SUCCESS;
	}

	/* Delegate device granule */
	return dev_granule_delegate(addr);
}

unsigned long smc_granule_undelegate(unsigned long addr)
{
	/* Try to find memory granule */
	struct granule *g = find_granule(addr);

	if (g != NULL) {
		if (!granule_lock_on_state_match(g, GRANULE_STATE_DELEGATED)) {
			return RMI_ERROR_INPUT;
		}
		/*
		 * A delegated memoty granule should only be undelegated on request from RMM.
		 * If this call fails, we have an unrecoverable error in EL3/RMM.
		 */
		if (rmm_el3_ifc_gtsi_undelegate(addr) != SMC_SUCCESS) {
			ERROR("Granule 0x%lx undelegate call failed\n", addr);
			granule_unlock(g);
			panic();
		}

		granule_set_state(g, GRANULE_STATE_NS);
		granule_unlock(g);
		return RMI_SUCCESS;
	}

	/* Undelegate device granule */
	return dev_granule_undelegate(addr);
}
