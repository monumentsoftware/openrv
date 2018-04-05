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

#ifndef OPENRV_MBEDTLSCONTEXT_H
#define OPENRV_MBEDTLSCONTEXT_H

#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

namespace openrv {

class MbedTlsContext
{
public:
    MbedTlsContext();
    virtual ~MbedTlsContext();

    bool init();
    bool isInitialized() const;

    mbedtls_entropy_context* entropyContext();
    mbedtls_ctr_drbg_context* ctrDrbgContext();

    void shutdown();

private:
    bool mInitialized = false;
    mbedtls_entropy_context mEntropy;
    /**
     * The Counter mode Deterministic Random Byte Generator, see also
     * https://tls.mbed.org/ctr-drbg-source-code (standardized by NIST)
     **/
    mbedtls_ctr_drbg_context mCtrDrbg;
};

inline bool MbedTlsContext::isInitialized() const
{
    return mInitialized;
}

inline mbedtls_entropy_context* MbedTlsContext::entropyContext()
{
    if (mInitialized) {
        return &mEntropy;
    }
    return nullptr;
}

inline mbedtls_ctr_drbg_context* MbedTlsContext::ctrDrbgContext()
{
    if (mInitialized) {
        return &mCtrDrbg;
    }
    return nullptr;
}

} // namespace openrv

#endif

