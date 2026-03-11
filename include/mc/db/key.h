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

#ifndef MC_DATABASE_KEY_H
#define MC_DATABASE_KEY_H

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <mc/db/byte_buffer.h>
#include <mc/exception.h>

namespace mc::db {

/**
 * 位转换函数 (C++17兼容版本)
 * 用于在不改变底层位表示的情况下转换类型
 */
template <typename To, typename From>
inline To bit_cast(const From& from)
{
    static_assert(sizeof(To) == sizeof(From), "类型大小必须相同");
    static_assert(std::is_trivially_copyable<From>::value, "From必须是可平凡复制的");
    static_assert(std::is_trivially_copyable<To>::value, "To必须是可平凡复制的");

    To to;
    std::memcpy(&to, &from, sizeof(To));
    return to;
}

/**
 * 浮点数与无符号整数的转换函数
 */
inline uint32_t float32_to_uint32(float f)
{
    uint32_t u = bit_cast<uint32_t>(f);
    if (f >= 0) {
        u |= 0x80000000;
    } else {
        u = ~u;
    }
    return u;
}

inline uint64_t float64_to_uint64(double f)
{
    uint64_t u = bit_cast<uint64_t>(f);
    if (f >= 0) {
        u |= 0x8000000000000000;
    } else {
        u = ~u;
    }
    return u;
}

inline float uint32_to_float32(uint32_t u)
{
    if (u & 0x80000000) {
        u &= ~uint32_t(0x80000000);
    } else {
        u = ~u;
    }
    return bit_cast<float>(u);
}

inline double uint64_to_float64(uint64_t u)
{
    if (u & 0x8000000000000000) {
        u &= ~uint64_t(0x8000000000000000);
    } else {
        u = ~u;
    }
    return bit_cast<double>(u);
}

/**
 * 数据库索引键
 * 支持多种数据类型，可用于创建和管理复合键
 */
class MC_API mdb_key {
public:
    mdb_key() = default;

    /**
     * 初始化键
     * @param key_count 键数量
     * @param is_unique 是否是唯一键
     */
    void init(int key_count, bool is_unique)
    {
        m_key_count = key_count;
        m_is_unique = is_unique;
        if (key_count <= 0) {
            MC_THROW(mc::invalid_arg_exception, "必须至少有一个key");
        }
        if (key_count > 255) {
            MC_THROW(mc::invalid_arg_exception, "组合键最多允许255个子项");
        }
    }

    /**
     * 判断是否是唯一键
     * @return 如果是唯一键返回true
     */
    bool is_unique() const
    {
        return m_is_unique;
    }

    /**
     * 判断是否是复合键
     * @return 如果是复合键返回true
     */
    bool is_compound_key() const
    {
        return m_key_count > 1;
    }

    /**
     * 获取键数量
     * @return 键数量
     */
    int key_count() const
    {
        return m_key_count;
    }

    /**
     * 获取当前键数量
     * @return 当前键数量
     */
    int key_num() const
    {
        return m_key_num;
    }

    /**
     * 重置键
     */
    void reset()
    {
        m_buf.reset();
        m_key_num  = 0;
        m_tail_nil = false;
    }

    /**
     * 获取键数据
     * @return 键数据视图
     */
    std::string_view key() const
    {
        return m_buf.bytes();
    }

    /**
     * 获取键长度
     * @return 键长度
     */
    size_t len() const
    {
        return m_buf.len();
    }

    /**
     * 获取字节缓冲区
     * @return 字节缓冲区
     */
    mc::db::byte_buffer* buffer()
    {
        return &m_buf;
    }

    /**
     * 添加字节数据
     * @param data 字节数据
     * @param size 数据大小
     */
    void append_bytes(const uint8_t* data, size_t size)
    {
        write_head(size);
        m_buf.write(data, size);
    }

    /**
     * 添加字符串
     * @param val 字符串
     */
    void append_string(std::string_view val)
    {
        write_head(val.size());
        m_buf.write_string(val);
    }

    /**
     * 添加16位整数
     * @param val 整数值
     */
    void append_int16(int16_t val)
    {
        write_head(3);
        if (val >= 0) {
            m_buf.write_byte('>');
        } else {
            m_buf.write_byte('-');
        }
        m_buf.write_uint16(static_cast<uint16_t>(val));
    }

    /**
     * 添加32位整数
     * @param val 整数值
     */
    void append_int32(int32_t val)
    {
        write_head(5);
        if (val >= 0) {
            m_buf.write_byte('>');
        } else {
            m_buf.write_byte('-');
        }
        m_buf.write_uint32(static_cast<uint32_t>(val));
    }

    /**
     * 添加64位整数
     * @param val 整数值
     */
    void append_int64(int64_t val)
    {
        write_head(9);
        if (val >= 0) {
            m_buf.write_byte('>');
        } else {
            m_buf.write_byte('-');
        }
        m_buf.write_uint64(static_cast<uint64_t>(val));
    }

    /**
     * 添加整数
     * @param val 整数值
     */
    void append_int(int val)
    {
        append_int32(static_cast<int32_t>(val));
    }

    /**
     * 添加无符号整数
     * @param val 整数值
     */
    void append_uint(unsigned int val)
    {
        append_uint32(static_cast<uint32_t>(val));
    }

    /**
     * 添加16位无符号整数
     * @param val 整数值
     */
    void append_uint16(uint16_t val)
    {
        write_head(2);
        m_buf.write_uint16(val);
    }

    /**
     * 添加32位无符号整数
     * @param val 整数值
     */
    void append_uint32(uint32_t val)
    {
        write_head(4);
        m_buf.write_uint32(val);
    }

    /**
     * 添加64位无符号整数
     * @param val 整数值
     */
    void append_uint64(uint64_t val)
    {
        write_head(8);
        m_buf.write_uint64(val);
    }

    /**
     * 添加32位浮点数
     * @param val 浮点数值
     */
    void append_float32(float val)
    {
        append_uint32(float32_to_uint32(val));
    }

    /**
     * 添加64位浮点数
     * @param val 浮点数值
     */
    void append_float64(double val)
    {
        append_uint64(float64_to_uint64(val));
    }

    /**
     * 根据类型添加值
     * @param val 要添加的值
     */
    template <typename T>
    void append_value(const T& val)
    {
        if constexpr (std::is_same<T, int16_t>::value) {
            append_int16(val);
        } else if constexpr (std::is_same<T, int32_t>::value) {
            append_int32(val);
        } else if constexpr (std::is_same<T, int>::value) {
            append_int(val);
        } else if constexpr (std::is_same<T, int64_t>::value) {
            append_int64(val);
        } else if constexpr (std::is_same<T, uint16_t>::value) {
            append_uint16(val);
        } else if constexpr (std::is_same<T, uint32_t>::value) {
            append_uint32(val);
        } else if constexpr (std::is_same<T, unsigned int>::value) {
            append_uint(val);
        } else if constexpr (std::is_same<T, uint64_t>::value) {
            append_uint64(val);
        } else if constexpr (std::is_same<T, float>::value) {
            append_float32(val);
        } else if constexpr (std::is_same<T, double>::value) {
            append_float64(val);
        } else if constexpr (std::is_same<T, std::string>::value ||
                             std::is_same<T, std::string_view>::value) {
            append_string(val);
        } else if constexpr (std::is_same<T, const char*>::value) {
            append_string(val);
        } else if constexpr (std::is_same<T, std::vector<uint8_t>>::value) {
            append_bytes(val.data(), val.size());
        } else {
            // 尝试调用val()方法
            if constexpr (has_val_method<T>::value) {
                append_value(val.val());
            } else {
                MC_THROW(mc::invalid_arg_exception, "不支持的key类型[${type}]",
                         ("type", typeid(T).name()));
            }
        }
    }

    /**
     * 直接写入值，不添加类型头信息
     * @param val 要写入的值
     */
    template <typename T>
    void write_value(const T& val)
    {
        if constexpr (std::is_same<T, int16_t>::value) {
            write_int16(val);
        } else if constexpr (std::is_same<T, int32_t>::value) {
            write_int32(val);
        } else if constexpr (std::is_same<T, int>::value) {
            write_int(val);
        } else if constexpr (std::is_same<T, int64_t>::value) {
            write_int64(val);
        } else if constexpr (std::is_same<T, uint16_t>::value) {
            write_uint16(val);
        } else if constexpr (std::is_same<T, uint32_t>::value) {
            write_uint32(val);
        } else if constexpr (std::is_same<T, unsigned int>::value) {
            write_uint(val);
        } else if constexpr (std::is_same<T, uint64_t>::value) {
            write_uint64(val);
        } else if constexpr (std::is_same<T, float>::value) {
            write_float32(val);
        } else if constexpr (std::is_same<T, double>::value) {
            write_float64(val);
        } else if constexpr (std::is_same<T, std::string>::value ||
                             std::is_same<T, std::string_view>::value) {
            write_string(val);
        } else if constexpr (std::is_same<T, const char*>::value) {
            write_string(val);
        } else if constexpr (std::is_same<T, std::vector<uint8_t>>::value) {
            write_bytes(val.data(), val.size());
        } else {
            // 尝试调用val()方法
            if constexpr (has_val_method<T>::value) {
                write_value(val.val());
            } else {
                MC_THROW(mc::invalid_arg_exception, "不支持的key类型[${type}]",
                         ("type", typeid(T).name()));
            }
        }
    }

    /**
     * 直接写入16位整数，不添加类型头信息
     * @param val 整数值
     */
    void write_int16(int16_t val)
    {
        if (val >= 0) {
            m_buf.write_byte('>');
        } else {
            m_buf.write_byte('-');
        }
        m_buf.write_uint16(static_cast<uint16_t>(val));
    }

    /**
     * 直接写入32位整数，不添加类型头信息
     * @param val 整数值
     */
    void write_int32(int32_t val)
    {
        if (val >= 0) {
            m_buf.write_byte('>');
        } else {
            m_buf.write_byte('-');
        }
        m_buf.write_uint32(static_cast<uint32_t>(val));
    }

    /**
     * 直接写入64位整数，不添加类型头信息
     * @param val 整数值
     */
    void write_int64(int64_t val)
    {
        if (val >= 0) {
            m_buf.write_byte('>');
        } else {
            m_buf.write_byte('-');
        }
        m_buf.write_uint64(static_cast<uint64_t>(val));
    }

    /**
     * 直接写入整数，不添加类型头信息
     * @param val 整数值
     */
    void write_int(int val)
    {
        write_int32(static_cast<int32_t>(val));
    }

    /**
     * 直接写入无符号整数，不添加类型头信息
     * @param val 整数值
     */
    void write_uint(unsigned int val)
    {
        write_uint32(static_cast<uint32_t>(val));
    }

    /**
     * 直接写入16位无符号整数，不添加类型头信息
     * @param val 整数值
     */
    void write_uint16(uint16_t val)
    {
        m_buf.write_uint16(val);
    }

    /**
     * 直接写入32位无符号整数，不添加类型头信息
     * @param val 整数值
     */
    void write_uint32(uint32_t val)
    {
        m_buf.write_uint32(val);
    }

    /**
     * 直接写入64位无符号整数，不添加类型头信息
     * @param val 整数值
     */
    void write_uint64(uint64_t val)
    {
        m_buf.write_uint64(val);
    }

    /**
     * 直接写入32位浮点数，不添加类型头信息
     * @param val 浮点数值
     */
    void write_float32(float val)
    {
        write_uint32(float32_to_uint32(val));
    }

    /**
     * 直接写入64位浮点数，不添加类型头信息
     * @param val 浮点数值
     */
    void write_float64(double val)
    {
        write_uint64(float64_to_uint64(val));
    }

    /**
     * 直接写入字符串，不添加类型头信息
     * @param val 字符串值
     */
    void write_string(std::string_view val)
    {
        m_buf.write_string(val);
    }

    /**
     * 直接写入字节数组，不添加类型头信息
     * @param data 字节数组指针
     * @param size 字节数组大小
     */
    void write_bytes(const uint8_t* data, size_t size)
    {
        m_buf.write_string(std::string_view(reinterpret_cast<const char*>(data), size));
    }

private:
    // SFINAE检测类是否有val()方法
    template <typename T, typename = void>
    struct has_val_method : std::false_type {};

    template <typename T>
    struct has_val_method<T, std::void_t<decltype(std::declval<T>().val())>> : std::true_type {};

    /**
     * 写入头部信息
     * @param n 数据长度
     * @return 成功返回true，失败返回false
     */
    bool write_head(size_t n)
    {
        if (m_key_num >= m_key_count) {
            MC_THROW(mc::invalid_arg_exception, "超出给定Key数量[${count}]",
                     ("count", m_key_count));
            return false;
        }
        if (m_key_count > 1) {
            if (m_key_num == 0) {
                m_buf.write_byte(static_cast<uint8_t>(m_key_count));
            }
            m_buf.write_byte(static_cast<uint8_t>(n));
        }
        m_key_num++;
        return true;
    }

    mc::db::byte_buffer m_buf;
    bool                m_is_unique{false};
    int                 m_key_count{0};
    int                 m_key_num{0};
    bool                m_tail_nil{false};
};

} // namespace mc::db

#endif // MC_DATABASE_KEY_H