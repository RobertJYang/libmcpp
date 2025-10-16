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

class CRC8Test : public ::testing::Test {
public:
    CRC8  crc8;
    CRC32 crc32;
};

TEST_F(CRC8Test, TestCRC_case1) {
    std::string data = "123456789";
    uint8_t     crc8 = this->crc8.calculate(data);
    EXPECT_EQ(crc8, 0xF4);

    uint32_t crc32 = this->crc32.calculate(data, 0, true);
    EXPECT_EQ(crc32, 0x765E7680);
}

TEST_F(CRC8Test, TestCRC_case2) {
    std::string data = "test";
    uint8_t     crc8 = this->crc8.calculate(data);
    EXPECT_EQ(crc8, 0xB9);

    uint32_t crc32 = this->crc32.calculate(data, 0, true);
    EXPECT_EQ(crc32, 0xF48F12D7);
}

TEST_F(CRC8Test, TestCRC_case3) {
    std::string data = "";
    uint8_t     crc8 = this->crc8.calculate(data);
    EXPECT_EQ(crc8, 0x00);

    uint32_t crc32 = this->crc32.calculate(data, 0, true);
    EXPECT_EQ(crc32, 0xFFFFFFFF);
}

} // namespace mc::crypto