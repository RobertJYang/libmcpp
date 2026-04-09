#include <mc/crypto/ecdsa_verify_sha256.h>

namespace mc::crypto {

int32_t ecdsa_verify_sha256(std::string& data, std::string& signature, std::string& public_key)
{
    MC_UNUSED(data);
    MC_UNUSED(signature);
    MC_UNUSED(public_key);
    return ECDSA_VERIFY_FAILED;
}

} // namespace mc::crypto
