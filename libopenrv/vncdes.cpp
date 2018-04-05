/*
 * Copyright (C) 2018 Monument-Software GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "vncdes.h"

#include <tomcrypt.h>

#include <algorithm>

/**
 * @param response Output buffer that can hold 16 bytes.
 * @param challenge Input buffer that is encrypted using DES and @p password.
 * @param password The password to use for encryption.
 *        NOTE: DES uses only up to 8 bytes of @p password. Longer passwords are truncated, shorter
 *        passwords are filled with zeroes.
 *
 * @return FALSE on internal error, otherwise TRUE.
 **/
bool VncDES::encrypt(uint8_t* response, const uint8_t* challenge, const char* password, size_t passwordLength)
{
    // DES requires an 8 byte key, no more, no less.
    // VNC uses the first 8 chars of the password, if the password is shorter, we fill with
    // zeroes.
    unsigned char desPassword[8] = {};
    passwordLength = std::min((size_t)8, passwordLength);
    for (int i = 0; i < (int)passwordLength; i++) {
        // VNC apparently uses a DES variant where the bits of all password bytes are
        // reversed.
        unsigned char v1 = (unsigned char)password[i];
        unsigned char v2 = 0;
        for (int j = 0; j < 8; j++) {
            if (v1 & (0x1 << j)) {
                v2 |= (0x1 << (8 - j - 1));
            }
        }
        desPassword[i] = v2;
    }
    symmetric_key key;
    if (des_setup(desPassword, 8, 0, &key) != CRYPT_OK) {
        return false;
    }
    if (des_ecb_encrypt(challenge + 0, response + 0, &key) != CRYPT_OK) {
        des_done(&key);
        return false;
    }
    if (des_ecb_encrypt(challenge + 8, response + 8, &key) != CRYPT_OK) {
        des_done(&key);
        return false;
    }
    des_done(&key);
    return true;
}

