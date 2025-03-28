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

#ifndef MC_IM_BYTE_BUFFER_H
#define MC_IM_BYTE_BUFFER_H

#include <cstdint>
#include <vector>
#include <string>
#include <string_view>
#include <cstring>

namespace mc::im {

/**
 * 字节缓冲区，用于高效处理字节数据
 */
class byte_buffer {
public:
    byte_buffer();

    /**
     * 重置缓冲区
     */
    void reset();

    /**
     * 增加缓冲区大小
     * @param n 增加的字节数
     */
    void grow(size_t n);

    /**
     * 尝试通过重新切片增加缓冲区大小
     * @param n 增加的字节数
     * @return 旧长度和是否成功
     */
    std::pair<size_t, bool> try_grow_by_reslice(size_t n);

    /**
     * 获取缓冲区长度
     * @return 缓冲区长度
     */
    size_t len() const;

    /**
     * 获取缓冲区容量
     * @return 缓冲区容量
     */
    size_t cap() const;

    /**
     * 获取缓冲区数据
     * @return 缓冲区数据
     */
    const std::vector<uint8_t>& bytes() const;

    /**
     * 增加缓冲区大小（内部实现）
     * @param n 增加的字节数
     * @return 旧长度
     */
    size_t grow_internal(size_t n);

    /**
     * 截断缓冲区
     * @param n 新的长度
     */
    void truncate(size_t n);

    /**
     * 写入字节数据
     * @param p 要写入的数据
     * @param size 数据大小
     * @return 是否成功
     */
    void write(const uint8_t* p, size_t size);

    /**
     * 写入字节数据
     * @param p 要写入的数据
     * @return 是否成功
     */
    void write(const std::vector<uint8_t>& p);

    /**
     * 写入单个字节
     * @param c 要写入的字节
     * @return 是否成功
     */
    void write_byte(uint8_t c);

    /**
     * 写入字符串
     * @param v 要写入的字符串
     * @return 是否成功
     */
    void write_string(std::string_view v);

    /**
     * 写入16位无符号整数
     * @param v 要写入的整数
     * @return 是否成功
     */
    void write_uint16(uint16_t v);

    /**
     * 写入32位无符号整数
     * @param v 要写入的整数
     * @return 是否成功
     */
    void write_uint32(uint32_t v);

    /**
     * 写入64位无符号整数
     * @param v 要写入的整数
     * @return 是否成功
     */
    void write_uint64(uint64_t v);

private:
    std::vector<uint8_t> m_buf;
    uint8_t m_bootstrap[64];
};

} // namespace mc::im

#endif // MC_IM_BYTE_BUFFER_H 