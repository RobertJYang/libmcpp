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

#ifndef MC_DBUS_MESSAGE_HEADER_H
#define MC_DBUS_MESSAGE_HEADER_H

#include <mc/dbus/enums.h>
#include <mc/dbus/error.h>
#include <mc/engine/path.h>
#include <mc/exception.h>
#include <mc/future.h>
#include <mc/reflect.h>
#include <mc/reflect/signature.h>
#include <mc/reflect/signature_helper.h>
#include <mc/reflect/type_code.h>
#include <mc/variant.h>

#include <dbus/dbus.h>

namespace mc::dbus {
using signature_iterator = mc::reflect::signature_iterator;
using signature          = mc::reflect::signature;
using type_code          = mc::reflect::type_code;
using path               = mc::engine::path;
namespace container      = mc::reflect::container;

/**
 * @brief 方法调用参数
 */
struct MC_API method_call_params {
    std::string_view service_name;
    std::string_view path;
    std::string_view interface;
    std::string_view method;
    std::string_view signature;
    const variants&  args;
};

/**
 * @brief 确保容器长度不超过最大限制
 * @param type_name [in] 容器类型名称
 * @param size [in] 容器大小
 * @exception 超出限制时抛出异常
 */
MC_API void ensure_container_max_length(const char* type_name, std::size_t size);

/**
 * @brief 确保消息深度不超过最大限制
 * @param depth [in] 消息深度
 * @exception 超出限制时抛出异常
 */
MC_API void ensure_message_depth(std::size_t depth);

template <typename T>
void ensure_container_max_length(T& container)
{
    ensure_container_max_length(mc::pretty_name<T>(), container.size());
}

template <typename T>
inline const std::string& get_signature()
{
    return mc::reflect::get_signature<T>();
}

template <typename T>
struct auto_dbus_free {
    void operator()(T* v) const
    {
        dbus_free(v);
    }
};

template <typename T>
using dbus_ptr = std::unique_ptr<T, auto_dbus_free<T>>;

/**
 * @brief DBus消息对象
 */
class MC_API message {
public:
    /**
     * @brief 创建方法调用消息
     * @param destination [in] 目标服务名称
     * @param path [in] 对象路径
     * @param interface [in] 接口名称
     * @param member [in] 方法名称
     * @return 返回方法调用消息对象
     */
    static message new_method_call(std::string_view destination, std::string_view path,
                                   std::string_view interface, std::string_view member);

    /**
     * @brief 创建方法返回消息
     * @param msg [in] 原始方法调用消息
     * @return 返回方法返回消息对象
     */
    static message new_method_return(const message& msg);

    /**
     * @brief 创建错误消息
     * @param msg [in] 原始消息
     * @param error_name [in] 错误名称
     * @param error_message [in] 错误消息内容
     * @return 返回错误消息对象
     */
    static message new_error(const message& msg, std::string_view error_name,
                             std::string_view error_message = {});

    /**
     * @brief 创建信号消息
     * @param path [in] 对象路径
     * @param interface [in] 接口名称
     * @param member [in] 信号名称
     * @return 返回信号消息对象
     */
    static message new_signal(std::string_view path, std::string_view interface,
                              std::string_view member);

    /**
     * @brief 创建指定类型的消息
     * @param msg_type [in] 消息类型
     * @return 返回消息对象
     */
    static message new_message(message_type msg_type = message_type::method_call);

    /**
     * @brief 创建错误消息
     * @param error_name [in] 错误名称
     * @param error_message [in] 错误消息内容
     * @return 返回错误消息对象
     */
    static message new_error_message(std::string_view error_name,
                                     std::string_view error_message = {});

    /**
     * @brief 获取底层DBus消息指针
     * @return 返回DBusMessage指针
     */
    DBusMessage* get_dbus_message() const;

    /**
     * @brief 检查消息是否有效
     * @return 有效返回true，否则返回false
     */
    bool is_valid() const;

    /**
     * @brief 默认构造函数
     */
    message();

    /**
     * @brief 从DBusMessage构造
     * @param msg [in] DBusMessage指针
     * @param add_ref [in] 是否增加引用计数
     */
    message(DBusMessage* msg, bool add_ref = false);

    /**
     * @brief 析构函数
     */
    ~message();

    /**
     * @brief 拷贝构造函数
     * @param other [in] 源消息对象
     */
    message(const message&);

    /**
     * @brief 拷贝赋值运算符
     * @param other [in] 源消息对象
     * @return 返回当前对象的引用
     */
    message& operator=(const message&);

    /**
     * @brief 移动构造函数
     * @param other [in/out] 源消息对象
     */
    message(message&& other) noexcept;

    /**
     * @brief 移动赋值运算符
     * @param other [in/out] 源消息对象
     * @return 返回当前对象的引用
     */
    message& operator=(message&& other) noexcept;

    /**
     * @brief 释放消息对象
     */
    void release();

    /**
     * @brief 获取消息类型
     * @return 返回消息类型枚举值
     */
    message_type get_type() const;

    /**
     * @brief 获取对象路径
     * @return 返回对象路径字符串
     */
    std::string_view get_path() const;

    /**
     * @brief 获取接口名称
     * @return 返回接口名称字符串
     */
    std::string_view get_interface() const;

    /**
     * @brief 获取成员名称
     * @return 返回成员名称字符串
     */
    std::string_view get_member() const;

    /**
     * @brief 获取错误名称
     * @return 返回错误名称字符串
     */
    std::string_view get_error_name() const;

    /**
     * @brief 获取目标服务名称
     * @return 返回目标服务名称字符串
     */
    std::string_view get_destination() const;

    /**
     * @brief 获取发送者名称
     * @return 返回发送者名称字符串
     */
    std::string_view get_sender() const;

    /**
     * @brief 获取消息签名
     * @return 返回消息签名字符串
     */
    std::string_view get_signature() const;

    /**
     * @brief 获取错误消息内容
     * @return 返回错误消息内容字符串
     */
    std::string get_error_message() const;

    /**
     * @brief 获取消息序列号
     * @return 返回消息序列号
     */
    uint32_t get_serial() const;

    /**
     * @brief 获取回复消息的序列号
     * @return 返回回复消息的序列号
     */
    uint32_t get_reply_serial() const;

    /**
     * @brief 设置对象路径
     * @param path [in] 对象路径
     */
    void set_path(std::string_view path);

    /**
     * @brief 设置接口名称
     * @param interface [in] 接口名称
     */
    void set_interface(std::string_view interface);

    /**
     * @brief 设置成员名称
     * @param member [in] 成员名称
     */
    void set_member(std::string_view member);

    /**
     * @brief 设置错误名称
     * @param error_name [in] 错误名称
     */
    void set_error_name(std::string_view error_name);

    /**
     * @brief 设置目标服务名称
     * @param destination [in] 目标服务名称
     */
    void set_destination(std::string_view destination);

    /**
     * @brief 设置发送者名称
     * @param sender [in] 发送者名称
     */
    void set_sender(std::string_view sender);

    /**
     * @brief 设置消息序列号
     * @param serial [in] 消息序列号
     */
    void set_serial(uint32_t serial);

    /**
     * @brief 检查是否为错误消息
     * @return 是错误消息返回true，否则返回false
     */
    bool is_error() const;

    /**
     * @brief 检查是否为方法调用消息
     * @return 是方法调用消息返回true，否则返回false
     */
    bool is_method_call() const;

    /**
     * @brief 检查是否为方法返回消息
     * @return 是方法返回消息返回true，否则返回false
     */
    bool is_method_return() const;

    /**
     * @brief 检查是否为信号消息
     * @return 是信号消息返回true，否则返回false
     */
    bool is_signal() const;

    /**
     * @brief 锁定消息，使其不可修改
     */
    void lock();

    /**
     * @brief 检查消息签名是否匹配
     * @param signature [in] 待检查的签名
     * @return 匹配返回true，否则返回false
     */
    bool has_signature(std::string_view signature);

    /**
     * @brief 获取消息读取器
     * @return 返回消息读取器对象
     */
    struct message_reader reader() const;

    /**
     * @brief 获取消息写入器
     * @return 返回消息写入器对象
     */
    struct message_writer writer();

    /**
     * @brief 将消息内容转换为指定类型
     * @tparam T 目标类型
     * @return 返回转换后的对象
     * @exception 类型转换失败时抛出异常
     */
    template <typename T>
    T as() const;

    /**
     * @brief 读取消息参数
     * @return 返回消息参数的variant数组
     */
    mc::variants read_args() const;

    /**
     * @brief 从指定对象写入消息内容
     * @tparam T 源对象类型
     * @param v [in] 源对象
     */
    template <typename T>
    void from(const T& v);

    /**
     * @brief 序列化消息
     * @return 返回序列化后的数据和长度
     */
    std::pair<dbus_ptr<char>, std::size_t> marshal();

    /**
     * @brief 反序列化消息
     * @param in [in] 输入数据
     * @param err [out] 错误信息
     * @return 成功返回true，失败返回false
     */
    bool demarshal(const std::vector<uint8_t>& in, error& err);

    /**
     * @brief 反序列化消息
     * @param in [in] 输入数据指针
     * @param len [in] 数据长度
     * @param err [out] 错误信息
     * @return 成功返回true，失败返回false
     */
    bool demarshal(const char* in, std::size_t len, error& err);

protected:
    DBusMessage* m_dbus_message{nullptr};
};

/**
 * @brief DBus消息读取器
 */
struct MC_API message_reader {
    /**
     * @brief 默认构造函数
     */
    message_reader();

    /**
     * @brief 从消息构造读取器
     * @param msg [in] 消息对象
     */
    message_reader(const message& msg);

    /**
     * @brief 读取variant值
     * @param v [out] 输出的variant对象
     * @param depth [in] 嵌套深度
     */
    void read_variant(mc::variant& v, std::size_t depth) const;

    /**
     * @brief 读取指定类型的variant值
     * @param type [in] 类型码
     * @param v [out] 输出的variant对象
     * @param depth [in] 嵌套深度
     */
    void read_variant_value(type_code type, mc::variant& v, std::size_t depth) const;

    /**
     * @brief 读取variant数组或dict
     * @param v [out] 输出的variant对象
     * @param depth [in] 嵌套深度
     */
    void read_variant_array_or_dict(mc::variant& v, std::size_t depth) const;

    /**
     * @brief 读取variant数组
     * @param arr [out] 输出的variants数组
     * @param depth [in] 嵌套深度
     */
    void read_variant_array(mc::variants& arr, std::size_t depth) const;

    /**
     * @brief 读取variant结构体
     * @param v [out] 输出的variant对象
     * @param depth [in] 嵌套深度
     */
    void read_variant_struct(mc::variant& v, std::size_t depth) const;

    /**
     * @brief 读取variant dict
     * @param dict [out] 输出的dict对象
     * @param depth [in] 嵌套深度
     */
    void read_variant_dict(mc::dict& dict, std::size_t depth) const;

    /**
     * @brief 读取原始variant结构体
     * @param v [out] 输出的variant对象
     * @param depth [in] 嵌套深度
     */
    void read_variant_raw_struct(mc::variant& v, std::size_t depth) const;

    /**
     * @brief 递归进入容器
     * @param parent [in] 父读取器
     */
    void recurse(const message_reader& parent) const;

    /**
     * @brief 移动到下一个元素
     * @return 返回当前读取器的引用
     */
    const message_reader& next() const;

    /**
     * @brief 检查是否到达末尾
     * @return 到达末尾返回true，否则返回false
     */
    bool at_end() const;

    /**
     * @brief 确保当前类型匹配期望类型
     * @param expected [in] 期望的类型
     * @exception 类型不匹配时抛出异常
     */
    void ensure_type(int expected) const;

    /**
     * @brief 确保类型匹配（静态方法）
     * @param expected [in] 期望的类型
     * @param actual [in] 实际的类型
     * @exception 类型不匹配时抛出异常
     */
    static void ensure_type(int expected, int actual);

    /**
     * @brief 获取当前类型码
     * @return 返回当前类型码
     */
    type_code current_type() const;

    template <typename T>
    T as() const
    {
        T v;
        *this >> v;
        return v;
    }

    mutable DBusMessageIter m_iter;
};

/**
 * @brief DBus消息写入器
 */
struct MC_API message_writer {
    /**
     * @brief 默认构造函数
     */
    message_writer() = default;

    /**
     * @brief 从消息构造写入器
     * @param msg [in/out] 消息对象
     */
    message_writer(message& msg);

    /**
     * @brief 从父迭代器构造写入器
     * @param parent_iter [in/out] 父迭代器
     * @param type [in] 容器类型
     * @param signature [in] 类型签名
     */
    message_writer(DBusMessageIter& parent_iter, int type,
                   std::string_view signature = std::string_view());

    /**
     * @brief 关闭容器
     */
    void close_container();

    /**
     * @brief 写入variant值
     * @param v [in] variant对象
     * @param depth [in] 嵌套深度
     */
    void write_variant(const mc::variant& v, std::size_t depth) const;

    /**
     * @brief 写入variant值（不带签名）
     * @param v [in] variant对象
     */
    void write_variant_value(const mc::variant& v) const;

    /**
     * @brief 按签名写入variant值
     * @param it [in] 签名迭代器
     * @param v [in] variant对象
     * @param depth [in] 嵌套深度
     */
    void write_variant(signature_iterator it, const mc::variant& v, std::size_t depth) const;

    /**
     * @brief 写入variant数组或dict
     * @param it [in] 签名迭代器
     * @param v [in] variant对象
     * @param depth [in] 嵌套深度
     */
    void write_variant_array_or_dict(signature_iterator it, const mc::variant& v,
                                     std::size_t depth) const;

    /**
     * @brief 写入variant数组
     * @param it [in] 签名迭代器
     * @param arr [in] variants数组
     * @param depth [in] 嵌套深度
     */
    void write_variant_array(signature_iterator it, const mc::variants& arr,
                             std::size_t depth) const;

    /**
     * @brief 写入variant结构体
     * @param it [in] 签名迭代器
     * @param v [in] variant对象
     * @param depth [in] 嵌套深度
     */
    void write_variant_struct(signature_iterator it, const mc::variant& v, std::size_t depth) const;

    /**
     * @brief 写入variant dict
     * @param it [in] 签名迭代器
     * @param dict [in] dict对象
     * @param depth [in] 嵌套深度
     */
    void write_variant_dict(signature_iterator it, const mc::dict& dict, std::size_t depth) const;

    /**
     * @brief 写入签名
     * @param sig [in] 签名对象
     */
    void write_signature(const signature& sig) const;

    /**
     * @brief 写入签名字符串
     * @param sig [in] 签名字符串
     * @param need_add_tail_zero [in] 是否需要添加尾零
     */
    void write_signature(std::string_view sig, bool need_add_tail_zero = true) const;

    /**
     * @brief 写入路径对象
     * @param p [in] 路径对象
     */
    void write_path(const path& p) const;

    /**
     * @brief 写入路径字符串
     * @param p [in] 路径字符串
     * @param need_add_tail_zero [in] 是否需要添加尾零
     */
    void write_path(std::string_view p, bool need_add_tail_zero = true) const;

    template <typename T>
    const message_writer& append(const T& v) const
    {
        *this << v;
        return *this;
    }

    template <typename F>
    const message_writer& write_container(signature_iterator it, F&& v)
    {
        MC_ASSERT(it.is_container(), "not a container type: ${v}", ("v", it.str()));

        auto container_type = it.current_type_char();
        auto sub_iter       = it.get_content_iterator();

        message_writer sub_writer(m_iter, container_type, sub_iter.str());
        if (sub_iter.current_type_code() == mc::reflect::type_code::dict_entry_start) {
            // 字典的 {key、value} 还是一个容器，需要单独处理
            auto           elem_iter = sub_iter.get_content_iterator();
            message_writer entry_writer(sub_writer.m_iter, DBUS_TYPE_DICT_ENTRY);
            v(entry_writer, elem_iter);
            entry_writer.close_container();
        } else {
            v(sub_writer, sub_iter);
        }

        sub_writer.close_container();
        return *this;
    }

    mutable DBusMessageIter* m_parent_iter{nullptr};
    mutable DBusMessageIter  m_iter;
};
/* -------------------- 重载 operator>> -------------------- */

template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
const message_reader& operator>>(const message_reader& reader, T& v)
{
    reader.ensure_type(mc::reflect::first_type(get_signature<T>()));

    if constexpr (std::is_same_v<T, float>) {
        double d;
        dbus_message_iter_get_basic(&reader.m_iter, &d);
        v = static_cast<T>(d);
    } else if constexpr (std::is_same_v<T, bool>) {
        uint32_t b;
        dbus_message_iter_get_basic(&reader.m_iter, &b);
        v = b != 0;
    } else {
        dbus_message_iter_get_basic(&reader.m_iter, &v);
    }

    return reader.next();
}

MC_API const message_reader& operator>>(const message_reader& reader, std::string& v);
MC_API const message_reader& operator>>(const message_reader& reader, std::string_view& v);
MC_API const message_reader& operator>>(const message_reader& reader, mc::dbus::path& v);
MC_API const message_reader& operator>>(const message_reader& reader, mc::dbus::signature& v);
MC_API const message_reader& operator>>(const message_reader& reader, mc::blob& v);
MC_API const message_reader& operator>>(const message_reader& reader, mc::variant& v);
MC_API const message_reader& operator>>(const message_reader& reader, mc::variants& v);
MC_API const message_reader& operator>>(const message_reader& reader, mc::dict& v);

// 读取标准库类型

template <typename T, bool UseEmplaceBack, bool IsContiguous, typename Container>
const message_reader& read_array(const message_reader& reader, Container& v)
{
    reader.ensure_type(DBUS_TYPE_ARRAY);

    message_reader arr_reader;
    arr_reader.recurse(reader);
    arr_reader.ensure_type(mc::reflect::first_type(get_signature<T>()));

    if constexpr (std::is_trivially_copyable_v<T> && IsContiguous) {
        // 对可平凡复制的类型，直接使用 memcpy 优化（必须确保容器内存是连续的）
        auto count = dbus_message_iter_get_element_count(&reader.m_iter);
        v.resize(count);
        int   len  = 0;
        void* data = nullptr;
        dbus_message_iter_get_fixed_array(&arr_reader.m_iter, &data, &len);
        std::memcpy(v.data(), data, len * sizeof(T));
    } else {
        while (!arr_reader.at_end()) {
            T item;
            arr_reader >> item;
            if constexpr (UseEmplaceBack) {
                v.emplace_back(std::move(item));
            } else {
                v.emplace(std::move(item));
            }
        }
    }

    return reader.next();
}

template <typename T, typename Allocator>
const message_reader& operator>>(const message_reader& reader, std::vector<T, Allocator>& v)
{
    return read_array<T, true, true>(reader, v);
}

template <typename T, typename Allocator>
const message_reader& operator>>(const message_reader& reader, std::list<T, Allocator>& v)
{
    return read_array<T, true, false>(reader, v);
}

template <typename T, typename Allocator>
const message_reader& operator>>(const message_reader& reader, std::deque<T, Allocator>& v)
{
    return read_array<T, true, false>(reader, v);
}

template <typename T, std::size_t N>
const message_reader& operator>>(const message_reader& reader, std::array<T, N>& v)
{
    reader.ensure_type(DBUS_TYPE_ARRAY);

    message_reader arr_reader;
    arr_reader.recurse(reader);
    arr_reader.ensure_type(mc::reflect::first_type(get_signature<T>()));

    auto count = dbus_message_iter_get_element_count(&reader.m_iter);
    if (count != N) {
        MC_THROW(mc::exception, "array size mismatch, expected ${expected}, got ${actual}",
                 ("expected", N)("actual", count));
    }

    if constexpr (std::is_trivially_copyable_v<T>) {
        // 对可平凡复制的类型，直接使用 memcpy 优化
        int   len  = 0;
        void* data = nullptr;
        dbus_message_iter_get_fixed_array(&arr_reader.m_iter, &data, &len);
        std::memcpy(v.data(), data, len * sizeof(T));
    } else {
        for (std::size_t i = 0; i < N; ++i) {
            arr_reader >> v[i];
        }
    }

    return reader.next();
}

template <typename T, typename Comp, typename Alloc>
const message_reader& operator>>(const message_reader& reader, std::set<T, Comp, Alloc>& v)
{
    return read_array<T, false, false>(reader, v);
}

template <typename T, typename Hash, typename KeyEqual, typename Alloc>
const message_reader& operator>>(const message_reader&                         reader,
                                 std::unordered_set<T, Hash, KeyEqual, Alloc>& v)
{
    return read_array<T, false, false>(reader, v);
}

template <typename T, typename Comp, typename Alloc>
const message_reader& operator>>(const message_reader& reader, std::multiset<T, Comp, Alloc>& v)
{
    return read_array<T, false, false>(reader, v);
}

template <typename T, typename U>
const message_reader& operator>>(const message_reader& reader, std::pair<T, U>& v)
{
    message_reader sub_reader;
    sub_reader.recurse(reader);
    sub_reader >> v.first >> v.second;

    return reader.next();
}

template <typename T>
const message_reader& operator>>(const message_reader& reader, std::optional<T>& v)
{
    // 用数组来表示可选值，数组空表示 nullopt，否则数组的第一个值是 optional 的值
    reader.ensure_type(DBUS_TYPE_ARRAY);

    message_reader arr_reader;
    arr_reader.recurse(reader);
    arr_reader.ensure_type(mc::reflect::first_type(get_signature<T>()));

    if (arr_reader.at_end()) {
        v = std::nullopt;
        return reader.next();
    }

    T value;
    arr_reader >> value;
    v = value;

    return reader.next();
}

template <typename... T>
const message_reader& operator>>(const message_reader& reader, std::tuple<T...>& v)
{
    reader.ensure_type(DBUS_TYPE_STRUCT);

    message_reader sub_reader;
    sub_reader.recurse(reader);

    mc::traits::tuple_for_each(v, [&](auto&& item) {
        // 如果 sub_reader 提前遍历完，说明输入的数据只匹配了结构的前半部分，
        // 按理应该失败才对，但我想支持只提供部分元素初始化元组，所以这里不检查
        if (sub_reader.at_end()) {
            return;
        }

        sub_reader >> item;
    });

    return reader.next();
}

template <typename K, typename V, typename Container>
const message_reader& read_dict(const message_reader& reader, Container& v)
{
    reader.ensure_type(DBUS_TYPE_ARRAY);

    message_reader sub_reader;
    sub_reader.recurse(reader);
    sub_reader.ensure_type(DBUS_TYPE_DICT_ENTRY);

    while (!sub_reader.at_end()) {
        message_reader entry_reader;
        entry_reader.recurse(sub_reader);

        K key;
        entry_reader >> key;

        V value;
        entry_reader >> value;

        v.emplace(std::move(key), std::move(value));
        sub_reader.next();
    }

    return reader.next();
}

template <typename K, typename V, typename Comp, typename Alloc>
const message_reader& operator>>(const message_reader& reader, std::map<K, V, Comp, Alloc>& v)
{
    return read_dict<K, V>(reader, v);
}

template <typename K, typename V, typename Comp, typename Alloc>
const message_reader& operator>>(const message_reader&             reader,
                                 std::multimap<K, V, Comp, Alloc>& v)
{
    return read_dict<K, V>(reader, v);
}

template <typename K, typename V, typename Hash, typename KeyEqual, typename Alloc>
const message_reader& operator>>(const message_reader&                            reader,
                                 std::unordered_map<K, V, Hash, KeyEqual, Alloc>& v)
{
    return read_dict<K, V>(reader, v);
}

// 读取反射类型，自动按照反射类型签名读取
template <typename T, std::enable_if_t<mc::reflect::is_reflectable<T>(), int> = 0>
const message_reader& operator>>(const message_reader& reader, T& v)
{
    reader.ensure_type(DBUS_TYPE_STRUCT);

    message_reader sub_reader;
    sub_reader.recurse(reader);
    mc::traits::tuple_for_each(mc::reflect::get_static_properties<T>(), [&](auto* item) {
        // 如果 sub_reader 已经遍历完则直接返回，我们允许只提供前面的部分属性初始化反射类型
        if (sub_reader.at_end()) {
            return;
        }

        sub_reader >> v.*item->member_ptr;
    });

    return reader.next();
}

/* -------------------- 重载 operator<< -------------------- */

template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
const message_writer& operator<<(const message_writer& writer, T v)
{
    if constexpr (std::is_same_v<T, bool>) {
        uint32_t b = v ? 1 : 0;
        dbus_message_iter_append_basic(&writer.m_iter, DBUS_TYPE_BOOLEAN, &b);
    } else if constexpr (std::is_same_v<T, int8_t>) {
        dbus_message_iter_append_basic(&writer.m_iter, DBUS_TYPE_BYTE, &v);
    } else if constexpr (std::is_same_v<T, uint8_t>) {
        dbus_message_iter_append_basic(&writer.m_iter, DBUS_TYPE_BYTE, &v);
    } else if constexpr (std::is_same_v<T, int16_t>) {
        dbus_message_iter_append_basic(&writer.m_iter, DBUS_TYPE_INT16, &v);
    } else if constexpr (std::is_same_v<T, uint16_t>) {
        dbus_message_iter_append_basic(&writer.m_iter, DBUS_TYPE_UINT16, &v);
    } else if constexpr (std::is_same_v<T, int32_t>) {
        dbus_message_iter_append_basic(&writer.m_iter, DBUS_TYPE_INT32, &v);
    } else if constexpr (std::is_same_v<T, uint32_t>) {
        dbus_message_iter_append_basic(&writer.m_iter, DBUS_TYPE_UINT32, &v);
    } else if constexpr (std::is_same_v<T, int64_t>) {
        dbus_message_iter_append_basic(&writer.m_iter, DBUS_TYPE_INT64, &v);
    } else if constexpr (std::is_same_v<T, uint64_t>) {
        dbus_message_iter_append_basic(&writer.m_iter, DBUS_TYPE_UINT64, &v);
    } else if constexpr (std::is_same_v<T, float>) {
        double d = static_cast<double>(v);
        dbus_message_iter_append_basic(&writer.m_iter, DBUS_TYPE_DOUBLE, &d);
    } else if constexpr (std::is_same_v<T, double>) {
        dbus_message_iter_append_basic(&writer.m_iter, DBUS_TYPE_DOUBLE, &v);
    } else {
        static_assert(!std::is_arithmetic_v<T>, "unsupported type");
    }
    return writer;
}

MC_API const message_writer& operator<<(const message_writer& writer, const mc::dbus::path& v);
MC_API const message_writer& operator<<(const message_writer& writer, const mc::dbus::signature& v);
MC_API const message_writer& operator<<(const message_writer& writer, const mc::blob& v);
MC_API const message_writer& operator<<(const message_writer& writer, const mc::variant& v);
MC_API const message_writer& operator<<(const message_writer& writer, const mc::variants& v);
MC_API const message_writer& operator<<(const message_writer& writer, const mc::dict& v);
MC_API const message_writer& operator<<(const message_writer& writer, const std::string& v);
MC_API const message_writer& operator<<(const message_writer& writer, const char* str);
MC_API const message_writer& operator<<(const message_writer& writer, const std::string_view& v);

// 写入标准库类型

template <typename T, bool IsContiguous, typename Container>
const message_writer& write_array(const message_writer& writer, const Container& v)
{
    const std::string& sig = get_signature<T>();

    message_writer sub_writer(writer.m_iter, DBUS_TYPE_ARRAY, sig);
    if constexpr (std::is_trivially_copyable_v<T> && IsContiguous && !std::is_pointer_v<T>) {
        // 对可平凡复制的类型，直接使用 fixed array 写入方式优化
        const T* data = v.data();
        dbus_message_iter_append_fixed_array(&sub_writer.m_iter, mc::reflect::first_type(sig),
                                             &data, v.size());
    } else {
        for (const auto& item : v) {
            sub_writer << item;
        }
    }
    sub_writer.close_container();
    return writer;
}

template <typename T, typename Allocator>
const message_writer& operator<<(const message_writer& writer, const std::vector<T, Allocator>& v)
{
    return write_array<T, true>(writer, v);
}

template <typename T, typename Allocator>
const message_writer& operator<<(const message_writer& writer, const std::list<T, Allocator>& v)
{
    return write_array<T, false>(writer, v);
}

template <typename T, typename Allocator>
const message_writer& operator<<(const message_writer& writer, const std::deque<T, Allocator>& v)
{
    return write_array<T, false>(writer, v);
}

template <typename T, typename Comp, typename Alloc>
const message_writer& operator<<(const message_writer& writer, const std::set<T, Comp, Alloc>& v)
{
    return write_array<T, false>(writer, v);
}

template <typename T, typename Hash, typename KeyEqual, typename Alloc>
const message_writer& operator<<(const message_writer&                               writer,
                                 const std::unordered_set<T, Hash, KeyEqual, Alloc>& v)
{
    return write_array<T, false>(writer, v);
}

template <typename T, typename Comp, typename Alloc>
const message_writer& operator<<(const message_writer&                writer,
                                 const std::multiset<T, Comp, Alloc>& v)
{
    return write_array<T, false>(writer, v);
}

template <typename T, std::size_t N>
const message_writer& operator<<(const message_writer& writer, const std::array<T, N>& v)
{
    return write_array<T, true>(writer, v);
}

template <typename T, typename U>
const message_writer& operator<<(const message_writer& writer, const std::pair<T, U>& v)
{
    message_writer sub_writer(writer.m_iter, DBUS_TYPE_STRUCT);
    sub_writer << v.first << v.second;
    sub_writer.close_container();
    return writer;
}

template <typename T>
const message_writer& operator<<(const message_writer& writer, const std::optional<T>& v)
{
    // 用数组来表示可选值，如果可选值有值，则写入一个值，否则写入一个空数组
    message_writer sub_writer(writer.m_iter, DBUS_TYPE_ARRAY, get_signature<T>());
    if (v.has_value()) {
        sub_writer << v.value();
    }
    sub_writer.close_container();
    return writer;
}

template <typename... T>
const message_writer& operator<<(const message_writer& writer, const std::tuple<T...>& v)
{
    message_writer sub_writer(writer.m_iter, DBUS_TYPE_STRUCT);
    mc::traits::tuple_for_each(v, [&](auto&& item) {
        sub_writer << item;
    });
    sub_writer.close_container();
    return writer;
}

template <typename K, typename V, typename Container>
const message_writer& write_dict(const message_writer& writer, const Container& v)
{
    ensure_container_max_length(v);

    auto           sig = get_signature<Container>();
    message_writer sub_writer(writer.m_iter, DBUS_TYPE_ARRAY, sig.substr(1));
    for (const auto& item : v) {
        message_writer entry_writer(sub_writer.m_iter, DBUS_TYPE_DICT_ENTRY);
        entry_writer << item.first << item.second;
        entry_writer.close_container();
    }
    sub_writer.close_container();
    return writer;
}

template <typename K, typename V, typename Comp, typename Alloc>
const message_writer& operator<<(const message_writer&              writer,
                                 const std::map<K, V, Comp, Alloc>& v)
{
    return write_dict<K, V>(writer, v);
}

template <typename K, typename V, typename Comp, typename Alloc>
const message_writer& operator<<(const message_writer&                   writer,
                                 const std::multimap<K, V, Comp, Alloc>& v)
{
    return write_dict<K, V>(writer, v);
}

template <typename K, typename V, typename Hash, typename KeyEqual, typename Alloc>
const message_writer& operator<<(const message_writer&                                  writer,
                                 const std::unordered_map<K, V, Hash, KeyEqual, Alloc>& v)
{
    return write_dict<K, V>(writer, v);
}
namespace detail {
// 辅助标签类型
struct const_ref_tag {};
struct value_tag {};

// 检测函数指针转换的主模板
template <typename T, typename Tag = void, typename = void>
struct has_operator : std::false_type {};

// 检测 const T& 重载版本
template <typename T>
struct has_operator<
    T, const_ref_tag,
    std::void_t<decltype(static_cast<const message_writer& (*)(const message_writer&, const T&)>(
        &operator<<))>> : std::true_type {};

// 检测 T 值重载版本
template <typename T>
struct has_operator<
    T, value_tag,
    std::void_t<decltype(static_cast<const message_writer& (*)(const message_writer&, T)>(
        &operator<<))>> : std::true_type {};

// 侦测 T 类型是否支持通过 operator<< 直接写入到 dbus::stream 中，防止出现类型隐士转换导致
// 写入的类型签名与 get_signature<> 获取的类型签名不一致
template <typename T>
inline constexpr bool has_operator_v =
    has_operator<T, const_ref_tag>::value || has_operator<T, value_tag>::value;

// 将 variant 解析为 dbus 签名
void variant_to_dbus_signature(signature& sig, const mc::variant& v);
} // namespace detail

// 写入反射类型，自动按照反射类型签名写入
template <typename T, std::enable_if_t<mc::reflect::is_reflectable<T>(), int> = 0>
const message_writer& operator<<(const message_writer& writer, const T& v)
{
    message_writer sub_writer(writer.m_iter, DBUS_TYPE_STRUCT);

    mc::traits::tuple_for_each(mc::reflect::get_static_properties<T>(), [&](auto* item) {
        using item_type     = std::decay_t<decltype(*item)>;
        using property_type = typename item_type::member_type;

        static_assert(detail::has_operator_v<property_type>,
                      "属性类型T不支持通过 operator<< 写入到 dbus::message 中");

        sub_writer << v.*item->member_ptr;
    });

    sub_writer.close_container();
    return writer;
}

template <typename T>
T message::as() const
{
    return reader().as<T>();
}

template <typename T>
void message::from(const T& v)
{
    writer() << v;
}

} // namespace mc::dbus

#endif // MC_DBUS_MESSAGE_HEADER_H