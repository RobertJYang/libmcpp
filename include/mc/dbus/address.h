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

#ifndef MC_DBUS_ADDRESS_H
#define MC_DBUS_ADDRESS_H

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mc {
namespace dbus {

/**
 * @brief DBus 地址条目类，表示单个传输地址
 *
 * 每个 address_entry 包含一个传输方法（如 unix, tcp）和相关的键值对参数
 */
class address_entry {
public:
    using key_value_map = std::unordered_map<std::string, std::string>;

    /**
     * @brief 构造函数
     * @param method 传输方法名称
     */
    explicit address_entry(std::string method);

    /**
     * @brief 获取传输方法
     * @return 传输方法名称
     */
    const std::string& get_method() const;

    /**
     * @brief 设置键值对
     * @param key 键名
     * @param value 键值
     */
    void set_value(std::string key, std::string value);

    /**
     * @brief 获取键值
     * @param key 键名
     * @return 键值，如果键不存在则返回空字符串
     */
    std::string_view get_value(const std::string& key) const;

    /**
     * @brief 检查是否存在指定的键
     * @param key 键名
     * @return 是否存在
     */
    bool has_key(const std::string& key) const;

    /**
     * @brief 获取所有键值对
     * @return 键值对映射
     */
    const key_value_map& get_values() const;

    /**
     * @brief 将地址条目转换为字符串表示
     * @return 地址条目的字符串表示
     */
    std::string to_string() const;

private:
    std::string   m_method; ///< 传输方法
    key_value_map m_values; ///< 键值对映射
};

using address_entry_ptr = std::shared_ptr<address_entry>;

/**
 * @brief DBus 地址解析类
 *
 * 用于解析 DBus 连接地址字符串，格式为：
 * method1:key1=value1,key2=value2;method2:key1=value1,key2=value2
 */
class address {
public:
    using entry_list = std::vector<address_entry_ptr>;

    /**
     * @brief 解析 DBus 地址字符串
     * @param address_str 地址字符串
     * @return 解析结果
     * @throws mc::bad_value_exception 如果地址字符串格式错误
     */
    static address parse(std::string_view address_str);

    /**
     * @brief 构造函数
     */
    address() = default;

    /**
     * @brief 添加地址条目
     * @param entry 地址条目
     */
    void add_entry(address_entry_ptr entry);

    /**
     * @brief 获取地址条目列表
     * @return 地址条目列表
     */
    const entry_list& get_entries() const;

    /**
     * @brief 将地址转换为字符串表示
     * @return 地址的字符串表示
     */
    std::string to_string() const;

    /**
     * @brief 转义 DBus 地址中的值
     * @param value 需要转义的值
     * @return 转义后的值
     */
    static std::string escape_value(std::string_view value);

    /**
     * @brief 反转义 DBus 地址中的值
     * @param value 需要反转义的值
     * @return 反转义后的值，如果转义失败返回
     */
    static std::optional<std::string> unescape_value(std::string_view value);

private:
    entry_list m_entries; ///< 地址条目列表
};

} // namespace dbus
} // namespace mc

#endif // MC_DBUS_ADDRESS_H