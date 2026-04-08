/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/**
 * @file test_ecdsa_verify_sha256.cpp
 * @brief Tests for EcdsaVerifySha256.
 */

#include <gtest/gtest.h>

#include <mc/crypto/ecdsa_verify_sha256.h>

#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>

namespace mc::crypto {

namespace {

using evp_pkey_ctx_ptr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using evp_pkey_ptr     = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using evp_md_ctx_ptr   = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

evp_pkey_ptr generate_ec_pkey()
{
    evp_pkey_ctx_ptr pkey_ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr), &EVP_PKEY_CTX_free);
    if (!pkey_ctx) {
        return evp_pkey_ptr(nullptr, &EVP_PKEY_free);
    }
    if (EVP_PKEY_keygen_init(pkey_ctx.get()) <= 0) {
        return evp_pkey_ptr(nullptr, &EVP_PKEY_free);
    }
    if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pkey_ctx.get(), NID_X9_62_prime256v1) <= 0) {
        return evp_pkey_ptr(nullptr, &EVP_PKEY_free);
    }

    EVP_PKEY* raw_pkey = nullptr;
    if (EVP_PKEY_keygen(pkey_ctx.get(), &raw_pkey) <= 0) {
        return evp_pkey_ptr(nullptr, &EVP_PKEY_free);
    }
    return evp_pkey_ptr(raw_pkey, &EVP_PKEY_free);
}

std::string to_der_signature(EVP_PKEY* pkey, const std::string& data)
{
    evp_md_ctx_ptr md_ctx(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    if (!md_ctx) {
        return {};
    }

    if (EVP_DigestSignInit(md_ctx.get(), nullptr, EVP_sha256(), nullptr, pkey) <= 0) {
        return {};
    }
    if (EVP_DigestSignUpdate(md_ctx.get(), data.data(), data.size()) <= 0) {
        return {};
    }

    size_t sig_len = 0;
    if (EVP_DigestSignFinal(md_ctx.get(), nullptr, &sig_len) <= 0 || sig_len == 0) {
        return {};
    }

    std::string signature(sig_len, '\0');
    auto*       out = reinterpret_cast<unsigned char*>(&signature[0]);
    if (EVP_DigestSignFinal(md_ctx.get(), out, &sig_len) <= 0) {
        return {};
    }
    signature.resize(sig_len);
    return signature;
}

std::string to_der_public_key(EVP_PKEY* pkey)
{
    unsigned char* pub_der_raw = nullptr;
    size_t         pub_len     = EVP_PKEY_get1_encoded_public_key(pkey, &pub_der_raw);
    if (pub_der_raw == nullptr || pub_len == 0) {
        return {};
    }
    std::string pub_der(reinterpret_cast<const char*>(pub_der_raw), pub_len);
    OPENSSL_free(pub_der_raw);
    return pub_der;
}

} // namespace

class EcdsaVerifySha256Test : public ::testing::Test {
protected:
    void SetUp() override {
        key_ = generate_ec_pkey();
        ASSERT_NE(key_, nullptr);
        pub_der_ = to_der_public_key(key_.get());
        ASSERT_FALSE(pub_der_.empty());
    }

    evp_pkey_ptr key_{nullptr, &EVP_PKEY_free};
    std::string  pub_der_;
};

TEST_F(EcdsaVerifySha256Test, VerifySuccess)
{
    const std::string data          = "hello ecdsa";
    auto              signature_der = to_der_signature(key_.get(), data);
    ASSERT_FALSE(signature_der.empty());

    std::string data_mutable = data;
    int32_t     ret          = ecdsa_verify_sha256(data_mutable, signature_der, pub_der_);
    EXPECT_EQ(ret, ECDSA_VERIFY_OK);
}

TEST_F(EcdsaVerifySha256Test, VerifyFail_TamperedData)
{
    const std::string data          = "hello ecdsa";
    auto              signature_der = to_der_signature(key_.get(), data);
    ASSERT_FALSE(signature_der.empty());

    std::string bad_data = data;
    bad_data[0] ^= 0x01;

    int32_t ret = ecdsa_verify_sha256(bad_data, signature_der, pub_der_);
    EXPECT_EQ(ret, ECDSA_VERIFY_FAILED);
}

TEST_F(EcdsaVerifySha256Test, VerifyFail_TamperedSignature)
{
    const std::string data          = "hello ecdsa";
    auto              signature_der = to_der_signature(key_.get(), data);
    ASSERT_FALSE(signature_der.empty());

    std::string bad_sig = signature_der;
    // 篡改末尾字节，确保影响到 r/s 值
    bad_sig.back() ^= 0x01;

    std::string data_mutable = data;
    int32_t     ret          = ecdsa_verify_sha256(data_mutable, bad_sig, pub_der_);
    EXPECT_EQ(ret, ECDSA_VERIFY_FAILED);
}

TEST_F(EcdsaVerifySha256Test, VerifyFail_EmptyInput)
{
    std::string empty;
    int32_t     ret = ecdsa_verify_sha256(empty, empty, empty);
    EXPECT_EQ(ret, ECDSA_VERIFY_FAILED);
}

} // namespace mc::crypto
