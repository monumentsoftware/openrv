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

