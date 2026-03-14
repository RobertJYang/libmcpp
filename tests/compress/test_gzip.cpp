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
 * @file test_gzip.cpp
 * @brief 测试 gzip 类的解压缩方法
 */

#include <algorithm>
#include <cstring>
#include <fstream>
#include <gtest/gtest.h>
#include <iomanip>
#include <iostream>
#include <mc/compress.h>
#include <string>
#include <test_utilities/test_base.h>
#include <vector>
#include <zlib.h>

namespace mc::compress {

/**
 * @brief 辅助函数：使用zlib压缩数据为gzip格式
 */
static std::string compress_gzip(const std::string& input)
{
    z_stream stream;
    std::memset(&stream, 0, sizeof(stream));
    stream.zalloc = Z_NULL;
    stream.zfree  = Z_NULL;
    stream.opaque = Z_NULL;

    int ret = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) {
        return "";
    }

    std::vector<Bytef> buffer(input.size() * 2 + 1024);
    stream.next_in   = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    stream.avail_in  = static_cast<uInt>(input.size());
    stream.next_out  = buffer.data();
    stream.avail_out = static_cast<uInt>(buffer.size());

    ret = deflate(&stream, Z_FINISH);
    if (ret != Z_STREAM_END) {
        deflateEnd(&stream);
        return "";
    }

    std::string compressed(reinterpret_cast<char*>(buffer.data()), stream.total_out);
    deflateEnd(&stream);
    return compressed;
}

class GzipTest : public ::testing::Test {};

TEST_F(GzipTest, TestDecompress_case1)
{
    std::string original   = "Hello, World!";
    std::string compressed = compress_gzip(original);
    ASSERT_FALSE(compressed.empty());

    std::string output;
    gzip_decompress(compressed, output);

    EXPECT_EQ(output, original);
}

TEST_F(GzipTest, TestDecompress_case2)
{
    std::string original   = "123456789";
    std::string compressed = compress_gzip(original);
    ASSERT_FALSE(compressed.empty());

    std::string output;
    gzip_decompress(compressed, output);

    EXPECT_EQ(output, original);
}

TEST_F(GzipTest, TestDecompress_case3)
{
    std::string original   = "";
    std::string compressed = compress_gzip(original);
    ASSERT_FALSE(compressed.empty());

    std::string output;
    gzip_decompress(compressed, output);

    EXPECT_EQ(output, original);
}

TEST_F(GzipTest, TestDecompress_case4)
{
    std::string original   = "This is a longer string to test gzip decompression with more data. "
                             "It contains multiple sentences and various characters.";
    std::string compressed = compress_gzip(original);
    ASSERT_FALSE(compressed.empty());

    std::string output;
    gzip_decompress(compressed, output);

    EXPECT_EQ(output, original);
}

TEST_F(GzipTest, TestDecompress_case5)
{
    std::string original   = "test";
    std::string compressed = compress_gzip(original);
    ASSERT_FALSE(compressed.empty());

    std::string output;
    gzip_decompress(compressed, output);

    EXPECT_EQ(output, original);
}

TEST_F(GzipTest, TestDecompress_InvalidInput)
{
    std::string invalid_input = "This is not valid gzip data";
    std::string output;

    EXPECT_THROW(gzip_decompress(invalid_input, output), mc::runtime_exception);
}

TEST_F(GzipTest, TestDecompress_EmptyInput)
{
    std::string empty_input = "";
    std::string output;

    // Empty input might cause an error or might be handled
    // This depends on zlib behavior
    EXPECT_THROW(gzip_decompress(empty_input, output), mc::runtime_exception);
}

} // namespace mc::compress
