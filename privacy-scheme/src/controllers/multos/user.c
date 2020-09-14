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

#include "user.h"

/**
 * Gets the user identifier.
 *
 * @param reader the reader to be used
 * @param identifier the user identifier
 * @return 0 if success else -1
 */
int ue_get_user_identifier(reader_t reader, user_identifier_t *identifier)
{
    if (identifier == NULL)
    {
        return -1;
    }

    memcpy(identifier->buffer, (uint8_t[]) {
            0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    }, USER_MAX_ID_LENGTH);
    identifier->buffer_length = USER_MAX_ID_LENGTH;

    return 0;
}

/**
 * Sets the user identifier and the issuer signatures of the user's keys.
 *
 * @param reader the reader to be used
 * @param identifier the user identifier
 * @param ie_signature the issuer signature
 * @return 0 if success else -1
 */
int ue_set_user_identifier_issuer_signatures(reader_t reader, user_identifier_t identifier, issuer_signature_t ie_signature)
{
    uint8_t pbSendBuffer[MAX_APDU_LENGTH_T0] = {0};
    uint8_t pbRecvBuffer[MAX_APDU_LENGTH_T0] = {0};
    uint32_t dwSendLength;
    uint32_t dwRecvLength;

    uint8_t data[256] = {0};

    uint8_t lc;

    int r;

    lc = 0;

    // user identifier
    memcpy(&data[lc], identifier.buffer, USER_MAX_ID_LENGTH);
    lc += USER_MAX_ID_LENGTH;

    // ie_signature.user_key
    mcl_G1_to_smartcard_G1(&data[lc], sizeof(elliptic_curve_point_t), ie_signature.user_key);
    lc += sizeof(elliptic_curve_point_t);

    // ie_signature.user_key_prime
    mcl_G1_to_smartcard_G1(&data[lc], sizeof(elliptic_curve_point_t), ie_signature.user_key_prime);
    lc += sizeof(elliptic_curve_point_t);

    dwSendLength = sizeof(pbSendBuffer);
    r = apdu_build_command(CASE3S, CLA_APPLICATION, INS_SET_USER_IDENTIFIER_ISSUER_SIGNATURE, 0x00, 0x00, lc, data, 0, pbSendBuffer, &dwSendLength);
    if (r < 0)
    {
        return -1;
    }

    dwRecvLength = sizeof(pbRecvBuffer);
    r = sc_transmit_data(reader, pbSendBuffer, dwSendLength, pbRecvBuffer, &dwRecvLength, NULL);
    if (r < 0)
    {
        fprintf(stderr, "Error: %s\n", sc_get_error(r));
        return r;
    }

    return 0;
}

/**
 * Computes the proof of key of the user keys.
 *
 * @param reader the reader to be used
 * @param sys_parameters the system parameters
 * @param ie_signature the issuer signature
 * @param nonce the nonce generated by the verifier
 * @param nonce_length the length of the nonce
 * @param ue_identifier the user identifier
 * @param proof_of_key the proof of key to be computed by the user
 * @return 0 if success else -1
 */
int ue_compute_proof_of_key(reader_t reader, system_par_t sys_parameters, issuer_signature_t ie_signature, const void *nonce, size_t nonce_length,
                            user_identifier_t ue_identifier, user_proof_of_key_t *proof_of_key)
{
    uint8_t pbSendBuffer[MAX_APDU_LENGTH_T0] = {0};
    uint8_t pbRecvBuffer[MAX_APDU_LENGTH_T0] = {0};
    uint32_t dwSendLength;
    uint32_t dwRecvLength;

    uint8_t data[256] = {0};
    size_t data_length;

    uint8_t lc, le;

    /*
     * IMPORTANT!
     *
     * We are using SHA1 on the Smart Card. However, because the length
     * of the SHA1 hash is 20 and the size of Fr is 32, it is necessary
     * to enlarge 12 characters and fill them with 0's.
     */
    unsigned char hash[SHA_DIGEST_PADDING + SHA_DIGEST_LENGTH] = {0};

    double elapsed_time;

    int r;

    if (nonce == NULL || nonce_length == 0 || proof_of_key == NULL)
    {
        return -1;
    }

    lc = NONCE_LENGTH;
    memcpy(data, nonce, NONCE_LENGTH);

    le = sizeof(elliptic_curve_point_t) + SHA_DIGEST_LENGTH + sizeof(elliptic_curve_multiplier_t) + sizeof(elliptic_curve_fr_t);

    dwSendLength = sizeof(pbSendBuffer);
    r = apdu_build_command(CASE4S, CLA_APPLICATION, INS_COMPUTE_PROOF_OF_KEY, 0x00, 0x00, lc, data, le, pbSendBuffer, &dwSendLength);
    if (r < 0)
    {
        return -1;
    }

    // compute proof of key
    dwRecvLength = sizeof(pbRecvBuffer);
    r = sc_transmit_data(reader, pbSendBuffer, dwSendLength, pbRecvBuffer, &dwRecvLength, &elapsed_time);
    if (r < 0)
    {
        fprintf(stderr, "Error: %s\n", sc_get_error(r));
        return r;
    }

    printf("[!] Elapsed time (compute_proof_of_key) = %f\n", elapsed_time);

    // 0x61XX
    assert(pbRecvBuffer[0] == 0x61);
    le = pbRecvBuffer[1]; // XX

    dwSendLength = sizeof(pbSendBuffer);
    r = apdu_build_command(CASE2S, 0x00, 0xC0, 0x00, 0x00, 00, NULL, le, pbSendBuffer, &dwSendLength);
    if (r < 0)
    {
        return -1;
    }

    // get proof of key
    dwRecvLength = sizeof(pbRecvBuffer);
    r = sc_transmit_data(reader, pbSendBuffer, dwSendLength, pbRecvBuffer, &dwRecvLength, &elapsed_time);
    if (r < 0)
    {
        fprintf(stderr, "Error: %s\n", sc_get_error(r));
        return r;
    }

    printf("[!] Elapsed time (communication_proof_of_key) = %f\n", elapsed_time);

    data_length = 0;

    /// signatures
    // key_hat
    smartcard_G1_to_mcl_G1(&proof_of_key->key_hat, &pbRecvBuffer[data_length], sizeof(elliptic_curve_point_t));
    r = mclBnG1_isValid(&proof_of_key->key_hat);
    if (r != 1)
    {
        return -2;
    }
    data_length += sizeof(elliptic_curve_point_t);

    /// e <-- H(...)
    memcpy(&hash[SHA_DIGEST_PADDING], &pbRecvBuffer[data_length], SHA_DIGEST_LENGTH);
    smartcard_Fr_to_mcl_Fr(&proof_of_key->e, hash, sizeof(elliptic_curve_fr_t));
    r = mclBnFr_isValid(&proof_of_key->e);
    if (r != 1)
    {
        return -3;
    }
    data_length += SHA_DIGEST_LENGTH;

    /// s values
    // s
    smartcard_Multiplier_to_mcl_Fr(&proof_of_key->s, &pbRecvBuffer[data_length], sizeof(elliptic_curve_multiplier_t));
    r = mclBnFr_isValid(&proof_of_key->s);
    if (r != 1)
    {
        return -4;
    }
    data_length += sizeof(elliptic_curve_multiplier_t);

    // s_id
    smartcard_Fr_to_mcl_Fr(&proof_of_key->s_id, &pbRecvBuffer[data_length], sizeof(elliptic_curve_fr_t));
    r = mclBnFr_isValid(&proof_of_key->s_id);
    if (r != 1)
    {
        return -5;
    }
    data_length += sizeof(elliptic_curve_fr_t);

    // amount of data processed = amount of data received
    assert(data_length == le);

    return 0;
}