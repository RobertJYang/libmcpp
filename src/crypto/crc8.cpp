/*
* Copyright (c) 2024 Huawei Technologies Co., Ltd.
* openUBMC is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*         http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

#include <mc/crypto/crc8.h>

mc::crypto::CRC8::CRC8() {
    initializeTable();
}

uint8_t mc::crypto::CRC8::calculate(const std::string& bytes) {
    uint8_t crc8 = 0x00;
    for (char c : bytes) {
        uint8_t byte = static_cast<uint8_t>(c);
        uint8_t index = byte ^ crc8;
        crc8 = table[index];
    }
    return crc8;
}

void mc::crypto::CRC8::initializeTable() {
    // 这里表已经静态初始化，实际不需要此函数
    // 保留此函数是为了兼容可能需要动态生成表的场景
}

