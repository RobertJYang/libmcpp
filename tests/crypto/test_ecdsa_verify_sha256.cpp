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

#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/sha.h>

namespace mc::crypto {

namespace {

std::string to_der_signature(EC_KEY* key, const std::string& data)
{
    unsigned char digest[SHA256_DIGEST_LENGTH] = {0};
    SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), digest);

    ECDSA_SIG* sig = ECDSA_do_sign(digest, SHA256_DIGEST_LENGTH, key);
    if (sig == nullptr) {
        return {};
    }

    int der_len = i2d_ECDSA_SIG(sig, nullptr);
    if (der_len <= 0) {
        ECDSA_SIG_free(sig);
        return {};
    }
    std::string    der(der_len, '\0');
    unsigned char* p = reinterpret_cast<unsigned char*>(&der[0]);
    if (i2d_ECDSA_SIG(sig, &p) != der_len) {
        ECDSA_SIG_free(sig);
        return {};
    }
    ECDSA_SIG_free(sig);
    return der;
}

std::string to_der_public_key(EC_KEY* key)
{
    int pub_len = i2o_ECPublicKey(key, nullptr);
    if (pub_len <= 0) {
        return {};
    }
    std::string    pub(pub_len, '\0');
    unsigned char* p = reinterpret_cast<unsigned char*>(&pub[0]);
    if (i2o_ECPublicKey(key, &p) != pub_len) {
        return {};
    }
    return pub;
}

} // namespace

class EcdsaVerifySha256Test : public ::testing::Test {
protected:
    void SetUp() override
    {
        key_ = EC_KEY_new_by_curve_name(NID_secp256k1);
        ASSERT_NE(key_, nullptr);
        ASSERT_EQ(EC_KEY_generate_key(key_), 1);
        pub_der_ = to_der_public_key(key_);
        ASSERT_FALSE(pub_der_.empty());
    }

    void TearDown() override
    {
        if (key_ != nullptr) {
            EC_KEY_free(key_);
        }
    }

    EC_KEY*     key_{nullptr};
    std::string pub_der_;
};

TEST_F(EcdsaVerifySha256Test, VerifySuccess)
{
    const std::string data          = "hello ecdsa";
    auto              signature_der = to_der_signature(key_, data);
    ASSERT_FALSE(signature_der.empty());

    std::string data_mutable = data;
    int32_t     ret          = ecdsa_verify_sha256(data_mutable,
                                                   signature_der,
                                                   pub_der_);
    EXPECT_EQ(ret, ECDSA_VERIFY_OK);
}

TEST_F(EcdsaVerifySha256Test, VerifyFail_TamperedData)
{
    const std::string data          = "hello ecdsa";
    auto              signature_der = to_der_signature(key_, data);
    ASSERT_FALSE(signature_der.empty());

    std::string bad_data = data;
    bad_data[0] ^= 0x01;

    int32_t ret = ecdsa_verify_sha256(bad_data, signature_der, pub_der_);
    EXPECT_EQ(ret, ECDSA_VERIFY_FAILED);
}

TEST_F(EcdsaVerifySha256Test, VerifyFail_TamperedSignature)
{
    const std::string data          = "hello ecdsa";
    auto              signature_der = to_der_signature(key_, data);
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
