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

#include <cstring>
#include <limits>

#include <mc/compress.h>
#include <mc/exception.h>
#include <zlib.h>
#include "securec.h"

namespace mc::compress {

void gzip_decompress(const std::string& input, std::string& output)
{
#define MAX_DR_SIZE_BITE (1024 * 1024)

    size_t input_size = input.size();
    // 检查输入大小是否超过uInt的最大值
    if (input_size > std::numeric_limits<uInt>::max()) {
        MC_THROW(mc::invalid_arg_exception, "The input param data is too large, length=${length}",
                 ("length", input_size));
    }

    // 预分配输出缓冲区
    output.resize(MAX_DR_SIZE_BITE);

    // 准备输入输出缓冲区指针
    Bytef* input_buffer    = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    Bytef* output_buffer   = reinterpret_cast<Bytef*>(output.data());
    uInt   input_length    = static_cast<uInt>(input_size);
    uInt   output_capacity = static_cast<uInt>(MAX_DR_SIZE_BITE);

    // 初始化 zlib 流结构
    z_stream stream{};
    (void)memset_s(&stream, sizeof(stream), 0, sizeof(stream));
    stream.zalloc    = Z_NULL;
    stream.zfree     = Z_NULL;
    stream.opaque    = Z_NULL;
    stream.next_in   = input_buffer;
    stream.avail_in  = input_length;
    stream.next_out  = output_buffer;
    stream.avail_out = output_capacity;

    // 初始化解压缩器（使用 gzip 格式）
    const int window_bits = MAX_WBITS | 16;
    int       init_result = inflateInit2(&stream, window_bits);
    if (init_result != Z_OK) {
        MC_THROW(mc::runtime_exception, "inflateInit2 failed, ret=${ret}", ("ret", init_result));
    }

    // 执行解压缩操作
    int  decompress_result  = inflate(&stream, Z_FINISH);
    bool decompress_success = (decompress_result == Z_STREAM_END);

    // 清理资源
    inflateEnd(&stream);

    // 检查解压缩结果
    if (!decompress_success) {
        MC_THROW(mc::runtime_exception, "inflate failed, ret=${ret}", ("ret", decompress_result));
    }

    // 根据实际解压缩的数据大小调整输出
    size_t decompressed_size = static_cast<size_t>(stream.total_out);
    output.assign(reinterpret_cast<const char*>(output_buffer), decompressed_size);
}

} // namespace mc::compress
