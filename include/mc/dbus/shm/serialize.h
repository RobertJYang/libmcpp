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
#ifndef MC_DBUS_SERIALIZE_H
#define MC_DBUS_SERIALIZE_H

#include <mc/dbus/message.h>
#include <mc/dict.h>
#include <mc/variant.h>

namespace mc::dbus::serialize {
constexpr size_t      MAX_COOKIE           = 32;               /**< 最大cookie值 */
constexpr size_t      BLOCK_SIZE           = 128;              /**< 块大小 */
constexpr size_t      MAX_DEPTH            = 32;               /**< 最大深度 */
constexpr uint8_t     COOKIE_NUMBER_ZERO   = 0;                /**< 零值cookie */
constexpr uint8_t     COOKIE_NUMBER_BYTE   = 1;                /**< 字节cookie */
constexpr uint8_t     COOKIE_NUMBER_WORD   = 2;                /**< 字cookie */
constexpr uint8_t     COOKIE_NUMBER_DWORD  = 4;                /**< 双字cookie */
constexpr uint8_t     COOKIE_NUMBER_QWORD  = 6;                /**< 四字cookie */
constexpr uint8_t     COOKIE_NUMBER_REAL   = 8;                /**< 实数cookie */
constexpr const char* ERROR_INVALID_FORMAT = "invalid format"; /**< 无效格式错误 */
constexpr uint8_t     TYPE_MASK            = 7;                /**< 类型掩码 */
constexpr uint8_t     VALUE_SHIFT          = 3;                /**< 值位移 */

/**
 * @brief 序列化参数
 * @param args [in] 参数列表
 * @return 返回序列化后的字符串
 */
MC_API std::string pack(const variants& args);

/**
 * @brief 反序列化消息
 * @param msg [in] 消息字符串
 * @return 返回反序列化后的variants数组
 */
MC_API variants unpack(std::string_view msg);

/**
 * @brief 序列化参数（serialize为deserialize的反操作，在pack数据前添加四字节长度头）
 * @param args [in] 参数列表
 * @return 返回序列化后的字符串（含四字节长度头）
 */
MC_API std::string serialize(const variants& args);

/**
 * @brief 反序列化消息（与unpack功能相同）
 * @param msg [in] 消息字符串
 * @return 返回反序列化后的variants数组
 */
MC_API variants deserialize(std::string_view msg);

/**
 * @brief 数据类型枚举
 */
enum class data_type : uint8_t {
    nil          = 0, /**< 空值 */
    boolean      = 1, /**< 布尔值 */
    number       = 2, /**< 数值 */
    userdata     = 3, /**< 用户数据 */
    short_string = 4, /**< 短字符串 */
    long_string  = 5, /**< 长字符串 */
    table        = 6, /**< 表 */
    gvariant     = 7, /**< GVariant */
    tail         = 8  /**< 尾标记 */
};

/**
 * @brief 数据块
 */
struct data_block {
    /**
     * @brief 构造函数
     */
    data_block() : next(nullptr), buf(BLOCK_SIZE)
    {}

    data_block*          next; /**< 下一个数据块指针 */
    std::vector<uint8_t> buf;  /**< 数据缓冲区 */
};

/**
 * @brief 写入缓冲区
 */
class MC_API write_buffer {
public:
    /**
     * @brief 构造函数
     */
    write_buffer();

    /**
     * @brief 析构函数
     */
    ~write_buffer();

    /**
     * @brief 写入参数
     * @param arg [in] variant参数
     * @param depth [in] 嵌套深度
     */
    void write_arg(const variant& arg, int depth = 0);

    /**
     * @brief 按签名写入参数
     * @param it [in] 签名迭代器
     * @param arg [in] variant参数
     * @param depth [in] 嵌套深度
     */
    void write_arg_with_signature(signature_iterator it, const variant& arg, int depth = 0);

    /**
     * @brief 写入数组
     * @param it [in] 签名迭代器
     * @param args [in] variants数组
     * @param depth [in] 嵌套深度
     */
    void write_array(signature_iterator it, const variants& args, int depth = 0);

    /**
     * @brief 写入variant元素
     * @param it [in] 签名迭代器
     * @param args [in] variants数组
     * @param depth [in] 嵌套深度
     */
    void write_variant_elements(signature_iterator it, const variants& args, int depth = 0);

    /**
     * @brief 转换为字符串
     * @return 返回序列化后的字符串
     */
    std::string to_string() const;

private:
    void write_array_or_dict(signature_iterator it, const variant& arg, int depth);
    void write_inner(const uint8_t* input, size_t size);
    void write_nil();
    void write_boolean(bool value);
    void write_number(const variant& arg);
    void write_double(double value);
    void write_integer(int64_t value);
    void write_string(std::string_view value);
    void write_array(const variants& args, int depth);
    void write_dict(const dict& args, int depth);
    void write_dict(signature_iterator it, const dict& args, int depth);
    void write_gvariant(const variant& arg);

    data_block* m_head;
    data_block* m_current;
    size_t      m_len;
    size_t      m_offset;
};

/**
 * @brief 读取缓冲区
 */
class MC_API read_buffer {
public:
    /**
     * @brief 构造函数
     * @param msg [in] 消息字符串
     */
    read_buffer(std::string_view msg);

    /**
     * @brief 读取指定大小的数据
     * @param size [in] 数据大小
     * @return 返回数据指针
     */
    const char* read(size_t size);

    /**
     * @brief 读取值
     * @param type [in] 类型
     * @param cookie [in] cookie值
     * @return 返回读取的variant值
     */
    variant read_value(uint8_t type, uint8_t cookie);

private:
    variant     read_inner();
    int64_t     read_integer(uint8_t cookie);
    double      read_double();
    void*       read_pointer();
    std::string read_string(size_t len);
    variant     read_table(int64_t array_size);
    variant     read_gvariant(size_t len);

    std::string_view m_buf;
    size_t           m_offset;
};

} // namespace mc::dbus::serialize

#endif // MC_DBUS_SERIALIZE_H