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
 * @file test_crc.cpp
 * @brief 测试 crc 类的校验方法
 */

#include <gtest/gtest.h>
#include <mc/crypto/crc32.h>
#include <mc/crypto/crc8.h>

namespace mc::crypto {

class CRCTest : public ::testing::Test {
public:
    CRC8 crc8;
};

TEST_F(CRCTest, TestCRC_case1)
{
    std::string data = "123456789";
    uint8_t     crc8 = this->crc8.calculate(data);
    EXPECT_EQ(crc8, 0xF4);

    uint32_t crc32_bzip2 = crc32_calculate(data, 0, true);
    EXPECT_EQ(crc32_bzip2, 0x765E7680);
}

TEST_F(CRCTest, TestCRC_case2)
{
    std::string data = "test";
    uint8_t     crc8 = this->crc8.calculate(data);
    EXPECT_EQ(crc8, 0xB9);

    uint32_t crc32_bzip2 = crc32_calculate(data, 0, true);
    EXPECT_EQ(crc32_bzip2, 0xF48F12D7);
}

TEST_F(CRCTest, TestCRC_case3)
{
    std::string data = "";
    uint8_t     crc8 = this->crc8.calculate(data);
    EXPECT_EQ(crc8, 0x00);

    uint32_t crc32_bzip2 = crc32_calculate(data, 0, true);
    EXPECT_EQ(crc32_bzip2, 0xFFFFFFFF);
}

// CRC32JamCRC 测试用例
TEST_F(CRCTest, TestCRC32JamCRC_case1)
{
    std::string data         = "123456789";
    uint32_t    crc32_jamcrc = crc32_jamcrc_calculate(data);
    // CRC-32/JAMCRC 对于 "123456789" 的期望值
    EXPECT_EQ(crc32_jamcrc, 0xCBF43926);
}

TEST_F(CRCTest, TestCRC32JamCRC_case2)
{
    std::string data         = "test";
    uint32_t    crc32_jamcrc = crc32_jamcrc_calculate(data);
    // CRC-32/JAMCRC 对于 "test" 的期望值
    EXPECT_EQ(crc32_jamcrc, 0xD87F7E0C);
}

TEST_F(CRCTest, TestCRC32JamCRC_case3)
{
    std::string data         = "";
    uint32_t    crc32_jamcrc = crc32_jamcrc_calculate(data);
    // 空字符串，init=0xFFFFFFFF，没有字节处理，最后异或 0xFFFFFFFF
    // 结果：0xFFFFFFFF ^ 0xFFFFFFFF = 0x00000000
    EXPECT_EQ(crc32_jamcrc, 0x00000000);
}

TEST_F(CRCTest, TestCRC32JamCRC_case4)
{
    std::string data         = "Hello, World!";
    uint32_t    crc32_jamcrc = crc32_jamcrc_calculate(data);
    // CRC-32/JAMCRC 对于 "Hello, World!" 的期望值
    EXPECT_EQ(crc32_jamcrc, 0xEC4AC3D0);
}

TEST_F(CRCTest, TestCRC32JamCRC_case5)
{
    std::string data         = "0";
    uint32_t    crc32_jamcrc = crc32_jamcrc_calculate(data);
    // CRC-32/JAMCRC 对于 "0" 的期望值
    EXPECT_EQ(crc32_jamcrc, 0xF4DBDF21);
}

} // namespace mc::crypto