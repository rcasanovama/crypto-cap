/**
 *
 *  Copyright (C) 2020  Raul Casanova Marques
 *
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "verifier.h"

/**
 * Generates a nonce to be used in the proof of key.
 *
 * @param nonce the nonce to be generated
 * @param nonce_length the length of the nonce
 * @return 0 if success else -1
 */
int ve_generate_nonce(void *nonce, size_t nonce_length)
{
    int r;

    if (nonce == NULL || nonce_length != NONCE_LENGTH)
    {
        return -1;
    }

    // random nonce
    r = RAND_bytes(nonce, nonce_length);
    if (r != 1)
    {
        return -2;
    }

    return 0;
}

/**
 * Verifies the proof of key of the user keys.
 *
 * @param sys_parameters the system parameters
 * @param ie_keys the issuer keys
 * @param nonce the nonce generated by the verifier
 * @param nonce_length the length of the nonce
 * @param ue_proof_of_key the proof of key computed by the user
 * @return 0 if success else -1
 */
int ve_verify_proof_of_key(system_par_t sys_parameters, issuer_keys_t ie_keys, const void *nonce, size_t nonce_length, user_proof_of_key_t ue_proof_of_key)
{
    mclBnFr epoch;

    mclBnFr mul_result;
    mclBnG1 mul_result_g1;

    mclBnFr e;

    mclBnG1 t_prime;

    unsigned char value[EC_SIZE] = {0};

    // used to obtain the point data independently of the platform
    char digest_platform_point[192] = {0};

    /*
     * IMPORTANT!
     *
     * We are using SHA1 on the Smart Card. However, because the length
     * of the SHA1 hash is 20 and the size of Fr is 32, it is necessary
     * to enlarge 12 characters and fill them with 0's.
     */
    unsigned char hash[SHA_DIGEST_PADDING + SHA_DIGEST_LENGTH] = {0};
    SHA_CTX ctx;

    int r;

    if (nonce == NULL || nonce_length == 0)
    {
        return -1;
    }

    // set epoch to Fr mcl type
    memset(value, 0, EC_SIZE); // zero memory
    generate_epoch(&value[EPOCH_OFFSET], EPOCH_LENGTH); // generate epoch
    mcl_bytes_to_Fr(&epoch, value, EC_SIZE); // convert to mcl type

    /// t values
    // t
    mclBnG1_mul(&t_prime, &sys_parameters.G1, &ue_proof_of_key.s); // t_prime = G1·s

    mclBnFr_mul(&mul_result, &ue_proof_of_key.e, &ie_keys.issuer_key_0.sk); // mul_result = e·K(0)
    mclBnFr_neg(&mul_result, &mul_result); // mul_result = -e·K(0)
    mclBnG1_mul(&mul_result_g1, &ue_proof_of_key.key_hat, &mul_result); // mul_result_g1 = key_hat·mul_result
    mclBnG1_add(&t_prime, &t_prime, &mul_result_g1); // t_prime = t_prime + mul_result_g1

    mclBnFr_mul(&mul_result, &ie_keys.issuer_key_1.sk, &ue_proof_of_key.s_id); // mul_result = K(1)·s_id
    mclBnG1_mul(&mul_result_g1, &ue_proof_of_key.key_hat, &mul_result); // mul_result_g1 = key_hat·mul_result
    mclBnG1_add(&t_prime, &t_prime, &mul_result_g1); // t_prime = t_prime + mul_result_g1

    mclBnFr_mul(&mul_result, &ue_proof_of_key.e, &ie_keys.issuer_key_2.sk); // mul_result = e·K(2)
    mclBnFr_mul(&mul_result, &mul_result, &epoch); // mul_result = mul_result·epoch
    mclBnFr_neg(&mul_result, &mul_result); // mul_result = -e·K(2)·epoch
    mclBnG1_mul(&mul_result_g1, &ue_proof_of_key.key_hat, &mul_result); // mul_result_g1 = key_hat·mul_result
    mclBnG1_add(&t_prime, &t_prime, &mul_result_g1); // t_prime = t_prime + mul_result_g1

    mclBnG1_normalize(&t_prime, &t_prime);
    r = mclBnG1_isValid(&t_prime);
    if (r != 1)
    {
        return -2;
    }

#ifndef NDEBUG
    mcl_display_G1("key_hat", ue_proof_of_key.key_hat);
    mcl_display_G1("t", t_prime);
#endif

    /// e <-- H(...)
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, digest_get_platform_point_data(digest_platform_point, ue_proof_of_key.key_hat), digest_get_platform_point_size());
    SHA1_Update(&ctx, digest_get_platform_point_data(digest_platform_point, t_prime), digest_get_platform_point_size());
    SHA1_Update(&ctx, nonce, nonce_length);
    SHA1_Final(&hash[SHA_DIGEST_PADDING], &ctx);

    /*
     * IMPORTANT!
     *
     * We are using SHA1 on the Smart Card. However, because the length
     * of the SHA1 hash is 20 and the size of Fr is 32, it is necessary
     * to enlarge 12 characters and fill them with 0's.
     */
    mcl_bytes_to_Fr(&e, hash, EC_SIZE);
    r = mclBnFr_isValid(&e);
    if (r != 1)
    {
        return -3;
    }

#ifndef NDEBUG
    mcl_display_Fr("e", e);
#endif

    r = mclBnFr_isEqual(&ue_proof_of_key.e, &e);
    if (r != 1)
    {
        return -4;
    }

    return 0;
}
