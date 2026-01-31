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

#ifndef MC_COMPRESS_H
#define MC_COMPRESS_H

#include <cstddef>
#include <cstdint>
#include <string>

#include <mc/common.h>
#include <mc/exception.h>

namespace mc::compress {

/**
 * @brief 解压缩gzip数据
 *
 * @param input 输入的压缩数据
 * @param output 输出的解压数据（会自动分配缓冲区）
 */
MC_API void gzip_decompress(const std::string& input, std::string& output);

} // namespace mc::compress

#endif
