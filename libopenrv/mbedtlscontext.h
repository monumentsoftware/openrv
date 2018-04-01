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

