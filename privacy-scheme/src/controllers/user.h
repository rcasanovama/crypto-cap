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

#ifndef __PRIVACY_SCHEME_CONTROLLER_USER_H_
#define __PRIVACY_SCHEME_CONTROLLER_USER_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <string.h>

#include <mcl/bn_c256.h>
#include <openssl/sha.h>

#include "models/issuer.h"
#include "models/user.h"
#include "system.h"

#include "helpers/mcl_helper.h"

typedef void *reader_t;

/**
 * Gets the user identifier.
 *
 * @param reader the reader to be used
 * @param identifier the user identifier
 * @return 0 if success else -1
 */
extern int ue_get_user_identifier(reader_t reader, user_identifier_t *identifier);

/**
 * Sets the user identifier and the issuer signatures of the user's keys.
 *
 * @param reader the reader to be used
 * @param identifier the user identifier
 * @param ie_signature the issuer signature
 * @return 0 if success else -1
 */
extern int ue_set_user_identifier_issuer_signatures(reader_t reader, user_identifier_t identifier, issuer_signature_t ie_signature);

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
extern int ue_compute_proof_of_key(reader_t reader, system_par_t sys_parameters, issuer_signature_t ie_signature, const void *nonce, size_t nonce_length,
                                   user_identifier_t ue_identifier, user_proof_of_key_t *proof_of_key);

#ifdef __cplusplus
}
#endif

#endif /* __PRIVACY_SCHEME_CONTROLLER_USER_H_ */