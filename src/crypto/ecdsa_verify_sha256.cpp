/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <mc/crypto/ecdsa_verify_sha256.h>
#include <mc/log.h>

#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

// 抑制 OpenSSL 3.0 废弃 API 警告
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

namespace mc::crypto {
namespace {

constexpr uint32_t SHA256_DIGEST_LEN = 32;

// 使用 OpenSSL 标准 DER 解码，正确处理 r/s 长度可变的情况（31~33 字节均合法）
int32_t construct_ecdsa_sig(ECDSA_SIG* ec_sig, const unsigned char* sign_data, uint32_t sign_data_len)
{
    if (ec_sig == nullptr || sign_data == nullptr || sign_data_len == 0) {
        elog("construct_ecdsa_sig: null input");
        return ECDSA_VERIFY_FAILED;
    }
    const unsigned char* p      = sign_data;
    ECDSA_SIG*           parsed = d2i_ECDSA_SIG(nullptr, &p, static_cast<long>(sign_data_len));
    if (parsed == nullptr) {
        elog("construct_ecdsa_sig: d2i_ECDSA_SIG failed, len=%d", sign_data_len);
        return ECDSA_VERIFY_FAILED;
    }
    const BIGNUM* r = nullptr;
    const BIGNUM* s = nullptr;
    ECDSA_SIG_get0(parsed, &r, &s);

    BIGNUM* r_dup = BN_dup(r);
    BIGNUM* s_dup = BN_dup(s);
    ECDSA_SIG_free(parsed);

    if (r_dup == nullptr || s_dup == nullptr) {
        BN_free(r_dup);
        BN_free(s_dup);
        elog("construct_ecdsa_sig: BN_dup failed");
        return ECDSA_VERIFY_FAILED;
    }

    if (ECDSA_SIG_set0(ec_sig, r_dup, s_dup) != 1) {
        BN_free(r_dup);
        BN_free(s_dup);
        elog("construct_ecdsa_sig: ECDSA_SIG_set0 failed");
        return ECDSA_VERIFY_FAILED;
    }
    return ECDSA_VERIFY_OK;
}

int32_t generate_sha256_digest(const unsigned char* data, uint32_t data_len, unsigned char* digest,
                               uint32_t& digest_len)
{
    if (data == nullptr || data_len == 0 || digest == nullptr) {
        elog("generate_sha256_digest: null input or bad len:%u", data_len);
        return ECDSA_VERIFY_FAILED;
    }
    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    if (md_ctx == nullptr) {
        elog("generate_sha256_digest: EVP_MD_CTX_new failed");
        return ECDSA_VERIFY_FAILED;
    }

    (void)EVP_DigestInit(md_ctx, EVP_sha256());
    (void)EVP_DigestUpdate(md_ctx, static_cast<const void*>(data), data_len);
    unsigned int digest_len_uint = 0;
    (void)EVP_DigestFinal(md_ctx, digest, &digest_len_uint);
    digest_len = static_cast<uint32_t>(digest_len_uint);
    EVP_MD_CTX_free(md_ctx);

    return ECDSA_VERIFY_OK;
}

int32_t construct_ec_key(EC_KEY** ec_key, const unsigned char* public_key, uint32_t public_key_len)
{
    if (public_key == nullptr || public_key_len == 0) {
        elog("construct_ec_key: null input or bad len:%u", public_key_len);
        return ECDSA_VERIFY_FAILED;
    }
    *ec_key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    if (*ec_key == nullptr) {
        elog("construct_ec_key: EC_KEY_new_by_curve_name failed");
        return ECDSA_VERIFY_FAILED;
    }

    const unsigned char* p = public_key;
    if (o2i_ECPublicKey(ec_key, &p, public_key_len) == nullptr) {
        EC_KEY_free(*ec_key);
        *ec_key = nullptr;
        elog("construct_ec_key: o2i_ECPublicKey failed");
        return ECDSA_VERIFY_FAILED;
    }
    return ECDSA_VERIFY_OK;
}

} // namespace

int32_t ecdsa_verify_sha256(std::string& data, std::string& signature, std::string& public_key)
{
    if (data.empty() || signature.empty() || public_key.empty()) {
        elog("ecdsa_verify_sha256: null input or bad len: data=%zu, sig=%zu, pub=%zu", data.size(), signature.size(),
             public_key.size());
        return ECDSA_VERIFY_FAILED;
    }

    ECDSA_SIG* ec_sig = ECDSA_SIG_new();
    if (ec_sig == nullptr) {
        elog("ecdsa_verify_sha256: ECDSA_SIG_new failed");
        return ECDSA_VERIFY_FAILED;
    }
    int32_t ret = construct_ecdsa_sig(ec_sig, reinterpret_cast<const unsigned char*>(signature.data()),
                                      static_cast<uint32_t>(signature.size()));
    if (ret != ECDSA_VERIFY_OK) {
        ECDSA_SIG_free(ec_sig);
        return ECDSA_VERIFY_FAILED;
    }

    unsigned char digest[SHA256_DIGEST_LEN] = {0};
    uint32_t      digest_len                = 0;
    ret = generate_sha256_digest(reinterpret_cast<const unsigned char*>(data.data()),
                                 static_cast<uint32_t>(data.size()), digest, digest_len);
    if (ret != ECDSA_VERIFY_OK) {
        ECDSA_SIG_free(ec_sig);
        return ECDSA_VERIFY_FAILED;
    }

    EC_KEY* ec_key = nullptr;
    ret            = construct_ec_key(&ec_key, reinterpret_cast<const unsigned char*>(public_key.data()),
                                      static_cast<uint32_t>(public_key.size()));
    if (ret != ECDSA_VERIFY_OK || ec_key == nullptr) {
        ECDSA_SIG_free(ec_sig);
        return ECDSA_VERIFY_FAILED;
    }

    ret = ECDSA_do_verify(digest, static_cast<int>(digest_len), ec_sig, ec_key);

    ECDSA_SIG_free(ec_sig);
    EC_KEY_free(ec_key);

    if (ret != 1) {
        elog("ecdsa_verify_sha256: ECDSA_do_verify failed, ret: ${err}", ("err", ret));
        return ECDSA_VERIFY_FAILED;
    }
    return ECDSA_VERIFY_OK;
}

} // namespace mc::crypto

// 恢复警告
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
