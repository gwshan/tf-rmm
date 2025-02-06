/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: Copyright TF-RMM Contributors.
 */

#ifndef ATTESTATION_PRIV_H
#define ATTESTATION_PRIV_H

#include <psa/crypto.h>
#include <t_cose/q_useful_buf.h>

/*
 * Get a pointer the handle of the key for signing realm attestation token.
 *
 * Arguments:
 * key_handle - Pointer to the key handle for signing token.
 *
 * Returns 0 on success, negative error code on error.
 */
int attest_get_realm_signing_key(psa_key_handle_t *key_handle);

/*
 * Query the attestation private key from monitor and generate the public
 * key by using MbedCryto lib. The key is cached internally for future
 * use. The function returns early if the key has been initialized.
 *
 * FPU context must be saved and FPU access should be enabled by caller.
 *
 * Returns 0 on success, negative error code on error.
 */
int attest_init_realm_attestation_key(void);

/*
 * Get the realm attestation public key.
 *
 * Arguments:
 * public_key - Get the buffer address and size which holds the realm
 *              attestation public key.
 *
 * Returns 0 on success, negative error code on error.
 */
int attest_get_realm_public_key(struct q_useful_buf_c *public_key);

/*
 * Get the platform token from monitor. This function needs to be called
 * after the Realm attestation key has been initialized.
 *
 * Returns 0 on success, negative error code on error.
 */
int attest_setup_platform_token(void);

/*
 * Return the platform token that was previously retrieved from the monitor.
 *
 * Arguments:
 * buf - pointer to a buffer where the address of the platform token will
 *       be returned.
 * len - pointer to an unsigned integer where the length of the platform token
 *       will be returned.
 *
 * Returns 0 on success, and a negative error code otherwise.
 */
int attest_get_platform_token(const void **buf, size_t *len);

/*
 * Get the hash algorithm to use for computing the hash of the realm public key.
 */
/* coverity[misra_c_2012_rule_2_4_violation:SUPPRESS] */
enum hash_algo attest_get_realm_public_key_hash_algo_id(void);

/*
 * Initialise PRNGs for all the CPUs
 *
 * FPU context must be saved and FPU access should be enabled by caller.
 *
 * Returns 0 on success, negative error code otherwise.
 *
 * This function creates a separate PRNG object for all the CPUs. The PRNGs are
 * used by Mbed TLS when it needs random data. The PRNGs are seeded with values
 * generated by a temporary PRNG, which is in turn is seeded with a real random
 * value.
 */
int attest_rnd_prng_init(void);

#endif /* ATTESTATION_PRIV_H */
