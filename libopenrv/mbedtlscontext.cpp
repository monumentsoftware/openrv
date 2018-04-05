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

#include "mbedtlscontext.h"

namespace openrv {

MbedTlsContext::MbedTlsContext()
{
}

MbedTlsContext::~MbedTlsContext()
{
    if (mInitialized) {
        mbedtls_ctr_drbg_free(&mCtrDrbg);
        mbedtls_entropy_free(&mEntropy);
    }
}

bool MbedTlsContext::init()
{
    if (isInitialized()) {
        return true;
    }
    // NOTE: atm we do not use personalizationData.
    const unsigned char* personalizationData = nullptr;
    const size_t personalizationDataLen = 0;

    mbedtls_entropy_init(&mEntropy);
    mbedtls_ctr_drbg_init(&mCtrDrbg);
    int ret = mbedtls_ctr_drbg_seed(&mCtrDrbg, mbedtls_entropy_func, &mEntropy, personalizationData, personalizationDataLen);
    if (ret != 0) {
        mbedtls_ctr_drbg_free(&mCtrDrbg);
        mbedtls_entropy_free(&mEntropy);
        return false;
    }
    mInitialized = true;
    return true;
}

void MbedTlsContext::shutdown()
{
    // TODO
}

} // namespace openrv

