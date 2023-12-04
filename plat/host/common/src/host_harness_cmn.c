/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: Copyright TF-RMM Contributors.
 */

#include <arch.h>
#include <arch_helpers.h>
#include <debug.h>
#include <errno.h>
#include <host_utils.h>
#include <rmm_el3_ifc.h>
#include <spinlock.h>
#include <string.h>

#define ATTEST_KEY_CURVE_ECC_SECP384R1	0

/* Hardcoded platform token value */
static uint8_t platform_token[] = {
	0xD2, 0x84, 0x44, 0xA1, 0x01, 0x38, 0x22, 0xA0,
	0x59, 0x02, 0x33, 0xA9, 0x19, 0x01, 0x09, 0x78,
	0x1C, 0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F, 0x2F,
	0x61, 0x72, 0x6D, 0x2E, 0x63, 0x6F, 0x6D, 0x2F,
	0x43, 0x43, 0x41, 0x2D, 0x53, 0x53, 0x44, 0x2F,
	0x31, 0x2E, 0x30, 0x2E, 0x30, 0x0A, 0x58, 0x20,
	0xB5, 0x97, 0x3C, 0xB6, 0x8B, 0xAA, 0x9F, 0xC5,
	0x55, 0x58, 0x78, 0x6B, 0x7E, 0xC6, 0x7F, 0x69,
	0xE4, 0x0D, 0xF5, 0xBA, 0x5A, 0xA9, 0x21, 0xCD,
	0x0C, 0x27, 0xF4, 0x05, 0x87, 0xA0, 0x11, 0xEA,
	0x19, 0x09, 0x5C, 0x58, 0x20, 0x7F, 0x45, 0x4C,
	0x46, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x3E,
	0x00, 0x01, 0x00, 0x00, 0x00, 0x50, 0x58, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x01, 0x00,
	0x58, 0x21, 0x01, 0x07, 0x06, 0x05, 0x04, 0x03,
	0x02, 0x01, 0x00, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B,
	0x0A, 0x09, 0x08, 0x17, 0x16, 0x15, 0x14, 0x13,
	0x12, 0x11, 0x10, 0x1F, 0x1E, 0x1D, 0x1C, 0x1B,
	0x1A, 0x19, 0x18, 0x19, 0x09, 0x61, 0x58, 0x21,
	0x01, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
	0x00, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09,
	0x08, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11,
	0x10, 0x1F, 0x1E, 0x1D, 0x1C, 0x1B, 0x1A, 0x19,
	0x18, 0x19, 0x09, 0x5B, 0x19, 0x30, 0x03, 0x19,
	0x09, 0x62, 0x67, 0x73, 0x68, 0x61, 0x2D, 0x32,
	0x35, 0x36, 0x19, 0x09, 0x5F, 0x84, 0xA5, 0x01,
	0x62, 0x42, 0x4C, 0x05, 0x58, 0x20, 0x07, 0x06,
	0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0x0F, 0x0E,
	0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x17, 0x16,
	0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x1F, 0x1E,
	0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18, 0x04, 0x65,
	0x33, 0x2E, 0x34, 0x2E, 0x32, 0x02, 0x58, 0x20,
	0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00,
	0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08,
	0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10,
	0x1F, 0x1E, 0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18,
	0x06, 0x74, 0x54, 0x46, 0x2D, 0x4D, 0x5F, 0x53,
	0x48, 0x41, 0x32, 0x35, 0x36, 0x4D, 0x65, 0x6D,
	0x50, 0x72, 0x65, 0x58, 0x49, 0x50, 0xA4, 0x01,
	0x62, 0x4D, 0x31, 0x05, 0x58, 0x20, 0x07, 0x06,
	0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0x0F, 0x0E,
	0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x17, 0x16,
	0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x1F, 0x1E,
	0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18, 0x04, 0x63,
	0x31, 0x2E, 0x32, 0x02, 0x58, 0x20, 0x07, 0x06,
	0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0x0F, 0x0E,
	0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x17, 0x16,
	0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x1F, 0x1E,
	0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18, 0xA4, 0x01,
	0x62, 0x4D, 0x32, 0x05, 0x58, 0x20, 0x07, 0x06,
	0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0x0F, 0x0E,
	0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x17, 0x16,
	0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x1F, 0x1E,
	0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18, 0x04, 0x65,
	0x31, 0x2E, 0x32, 0x2E, 0x33, 0x02, 0x58, 0x20,
	0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00,
	0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08,
	0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10,
	0x1F, 0x1E, 0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18,
	0xA4, 0x01, 0x62, 0x4D, 0x33, 0x05, 0x58, 0x20,
	0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00,
	0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08,
	0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10,
	0x1F, 0x1E, 0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18,
	0x04, 0x61, 0x31, 0x02, 0x58, 0x20, 0x07, 0x06,
	0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0x0F, 0x0E,
	0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x17, 0x16,
	0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x1F, 0x1E,
	0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18, 0x19, 0x09,
	0x60, 0x6C, 0x77, 0x68, 0x61, 0x74, 0x65, 0x76,
	0x65, 0x72, 0x2E, 0x63, 0x6F, 0x6D, 0x58, 0x60,
	0xE6, 0xB6, 0x38, 0x4F, 0xAE, 0x3F, 0x6E, 0x67,
	0xF5, 0xD4, 0x97, 0x4B, 0x3F, 0xFD, 0x0A, 0xFA,
	0x1D, 0xF0, 0x2F, 0x73, 0xB8, 0xFF, 0x5F, 0x02,
	0xC0, 0x0F, 0x40, 0xAC, 0xF3, 0xA2, 0x9D, 0xB5,
	0x31, 0x50, 0x16, 0x4F, 0xFA, 0x34, 0x3D, 0x0E,
	0xAF, 0xE0, 0xD0, 0xD1, 0x6C, 0xF0, 0x9D, 0xC1,
	0x01, 0x42, 0xA2, 0x3C, 0xCE, 0xD4, 0x4A, 0x59,
	0xDC, 0x29, 0x0A, 0x30, 0x93, 0x5F, 0xB4, 0x98,
	0x61, 0xBA, 0xE3, 0x91, 0x22, 0x95, 0x24, 0xF4,
	0xAE, 0x47, 0x93, 0xD3, 0x84, 0xA3, 0x76, 0xD0,
	0xC1, 0x26, 0x96, 0x53, 0xA3, 0x60, 0x3F, 0x6C,
	0x75, 0x96, 0x90, 0x6A, 0xF9, 0x4E, 0xDA, 0x30
};

static uint8_t sample_attest_priv_key[] = {
	0x20, 0x11, 0xC7, 0xF0, 0x3C, 0xEE, 0x43, 0x25, 0x17, 0x6E,
	0x52, 0x4F, 0x03, 0x3C, 0x0C, 0xE1, 0xE2, 0x1A, 0x76, 0xE6,
	0xC1, 0xA4, 0xF0, 0xB8, 0x39, 0xAA, 0x1D, 0xF6, 0x1E, 0x0E,
	0x8A, 0x5C, 0x8A, 0x05, 0x74, 0x0F, 0x9B, 0x69, 0xEF, 0xA7,
	0xEB, 0x1A, 0x41, 0x85, 0xBD, 0x11, 0x7F, 0x68
};

bool host_memcpy_ns_read(void *dest, const void *ns_src, unsigned long size)
{
	(void)memcpy(dest, ns_src, size);
	return true;
}

bool host_memcpy_ns_write(void *ns_dest, const void *src, unsigned long size)
{
	(void)memcpy(ns_dest, src, size);
	return true;
}

unsigned long host_monitor_call(unsigned long id,
			unsigned long arg0,
			unsigned long arg1,
			unsigned long arg2,
			unsigned long arg3,
			unsigned long arg4,
			unsigned long arg5)
{
	/* Avoid MISRA C:2102-2.7 warnings */
	(void)arg1;
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;

	switch (id) {
	case SMC_RMM_GTSI_DELEGATE:
		return host_gtsi_delegate(arg0);
	case SMC_RMM_GTSI_UNDELEGATE:
		return host_gtsi_undelegate(arg0);
	default:
		return 0UL;
	}
}

static int attest_get_platform_token(uint64_t buf_pa, uint64_t *buf_size,
				     uint64_t c_size)
{
	(void)c_size; /* The challenge is ignored */

	if (*buf_size < sizeof(platform_token)) {
		ERROR("Failed to get platform token: Buffer is too small.\n");
		return -ENOMEM;
	}

	(void)memcpy((void *)buf_pa, (void *)platform_token, sizeof(platform_token));
	*buf_size = sizeof(platform_token);

	return 0;
}

static int attest_get_signing_key(uint64_t buf_pa, uint64_t *buf_size,
				  uint64_t ecc_curve)
{
	if (ecc_curve != ATTEST_KEY_CURVE_ECC_SECP384R1) {
		ERROR("Invalid ECC curve specified\n");
		return -EINVAL;
	}

	if (*buf_size < sizeof(sample_attest_priv_key)) {
		return -EINVAL;
	}

	(void)memcpy((void *)buf_pa, (void *)sample_attest_priv_key,
		     sizeof(sample_attest_priv_key));
	*buf_size = sizeof(sample_attest_priv_key);

	return 0;
}

void host_monitor_call_with_res(unsigned long id,
			unsigned long arg0,
			unsigned long arg1,
			unsigned long arg2,
			unsigned long arg3,
			unsigned long arg4,
			unsigned long arg5,
			struct smc_result *res)
{
	/* Avoid MISRA C:2102-2.7 warnings */
	(void)arg3;
	(void)arg4;
	(void)arg5;

	switch (id) {
	case SMC_RMM_GET_PLAT_TOKEN:
		res->x[0] = attest_get_platform_token(arg0, &arg1, arg2);
		res->x[1] = arg1;
		break;
	case SMC_RMM_GET_REALM_ATTEST_KEY:
		res->x[0] = attest_get_signing_key(arg0, &arg1, arg2);
		res->x[1] = arg1;
		break;
	default:
		VERBOSE("Unimplemented monitor call id %lx\n", id);
	}
}

int host_run_realm(unsigned long *regs)
{
	return host_util_rec_run(regs);
}

void host_spinlock_acquire(spinlock_t *l)
{
	l->val = 1;
}

void host_spinlock_release(spinlock_t *l)
{
	l->val = 0;
}

u_register_t host_read_sysreg(char *reg_name)
{
	struct sysreg_cb *callbacks = host_util_get_sysreg_cb(reg_name);

	/*
	 * Return 0UL as default value for registers which do not have
	 * a read callback installed.
	 */
	if (callbacks == NULL) {
		return 0UL;
	}

	if (callbacks->rd_cb == NULL) {
		return 0UL;
	}

	return callbacks->rd_cb(callbacks->reg);
}

void host_write_sysreg(char *reg_name, u_register_t v)
{
	struct sysreg_cb *callbacks = host_util_get_sysreg_cb(reg_name);

	/*
	 * Ignore the write if the register does not have a write
	 * callback installed.
	 */
	if (callbacks != NULL) {
		if (callbacks->wr_cb != NULL) {
			callbacks->wr_cb(v, callbacks->reg);
		}
	}
}
