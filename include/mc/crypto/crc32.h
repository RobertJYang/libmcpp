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
 * @file crc32.h
 * @brief 定义了crc32校验方法
 */

#ifndef MC_CRYPTO_CRC32_H
#define MC_CRYPTO_CRC32_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <mc/common.h>

namespace mc::crypto {

/**
 * @brief 计算CRC32校验值
 * @param bytes 输入数据
 * @param init 初始值，默认为0xFFFFFFFF
 * @param is_last 是否为最后一段数据，默认为false
 * @return CRC32校验值
 */
MC_API uint32_t crc32_calculate(const std::string& bytes, uint32_t init = 0xFFFFFFFF, bool is_last = false);

/**
 * @brief 计算CRC-32/JAMCRC校验值
 * @param bytes 输入数据
 * @param init 初始值，默认为0xFFFFFFFF
 * @return CRC-32/JAMCRC校验值
 */
MC_API uint32_t crc32_jamcrc_calculate(const std::string& bytes, uint32_t init = 0xFFFFFFFF);

} // namespace mc::crypto

#endif
