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

/**
 * @file ecdsa_verify_sha256.h
 * @brief ECDSA P-256 + SHA-256 signature verification helper.
 */

#ifndef MC_CRYPTO_ECDSA_VERIFY_SHA256_H
#define MC_CRYPTO_ECDSA_VERIFY_SHA256_H

#include <cstdint>
#include <string>

#include <mc/common.h>

namespace mc::crypto {

constexpr int32_t ECDSA_VERIFY_OK     = 0;
constexpr int32_t ECDSA_VERIFY_FAILED = -1;

/**
 * @brief ECDSA P-256 + SHA-256 签名验证
 * @param data 待验证的数据
 * @param signature 签名数据
 * @param public_key 公钥数据
 * @return ECDSA_VERIFY_OK 表示验证成功，ECDSA_VERIFY_FAILED 表示验证失败
 */
MC_API int32_t ecdsa_verify_sha256(std::string& data, std::string& signature, std::string& public_key);

} // namespace mc::crypto

#endif
