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

#ifndef MC_DBUS_PATH_H
#define MC_DBUS_PATH_H

#include <ostream>
#include <string>
#include <vector>

#include <mc/variant.h>
namespace mc {
namespace dbus {

/**
 * 表示DBus对象路径的类
 *
 * DBus对象路径必须遵循以下规则：
 * - 以斜杠（/）开头
 * - 路径元素由斜杠分隔
 * - 每个元素必须只包含ASCII字母、数字和下划线
 * - 元素不能为空（不允许连续斜杠）
 * - 可以是单个斜杠（表示根路径）
 */
class path {
public:
    /**
     * 默认构造函数，创建根路径
     */
    path();

    /**
     * 使用字符串构造路径
     *
     * @param p 路径字符串
     * @throws invalid_object_path_exception 如果路径格式无效
     */
    explicit path(std::string p);

    /**
     * 使用C风格字符串构造路径
     *
     * @param p 路径C风格字符串
     * @throws invalid_object_path_exception 如果路径格式无效
     */
    explicit path(const char* p);

    /**
     * 获取路径字符串
     *
     * @return 路径字符串
     */
    const std::string& str() const;

    /**
     * 转换为字符串
     *
     * @return 路径字符串
     */
    operator std::string() const;

    /**
     * 等于比较运算符
     *
     * @param other 要比较的路径
     * @return 如果路径相等返回true
     */
    bool operator==(const path& other) const;

    /**
     * 不等于比较运算符
     *
     * @param other 要比较的路径
     * @return 如果路径不相等返回true
     */
    bool operator!=(const path& other) const;

    /**
     * 小于比较运算符（用于容器排序）
     *
     * @param other 要比较的路径
     * @return 如果this小于other返回true
     */
    bool operator<(const path& other) const;

    /**
     * 路径连接操作符
     *
     * @param element 要添加的路径元素
     * @return 连接后的新路径
     */
    path operator/(const std::string& element) const;

    /**
     * 赋值操作符
     *
     * @param p 要赋值的路径字符串
     * @return 赋值后的路径引用
     */
    path& operator=(std::string p);

    /**
     * 等于比较运算符
     *
     * @param p 要比较的路径字符串
     * @return 如果路径相等返回true
     */
    bool operator==(std::string_view p) const {
        return m_path == p;
    }
    bool operator!=(std::string_view p) const {
        return m_path != p;
    }

    friend bool operator==(std::string_view s, const path& p) {
        return p.m_path == s;
    }
    friend bool operator!=(std::string_view s, const path& p) {
        return p.m_path != s;
    }

    /**
     * 获取父路径
     *
     * @return 父路径
     */
    path parent() const;

    /**
     * 获取路径的最后一个元素
     *
     * @return 路径的最后一个元素
     */
    std::string basename() const;

    /**
     * 验证字符串是否为有效的DBus对象路径
     *
     * @param p 要验证的路径字符串
     * @return 如果是有效的DBus对象路径返回true
     */
    static bool is_valid(const std::string& p);

    /**
     * 验证字符串是否为有效的路径元素
     *
     * @param element 要验证的路径元素
     * @return 如果是有效的路径元素返回true
     */
    static bool is_valid_element(const std::string& element);

private:
    std::string m_path;
};

/**
 * 输出流操作符重载
 */
std::ostream& operator<<(std::ostream& os, const path& p);

} // namespace dbus

inline void to_variant(const dbus::path& p, variant& v) {
    v = p.str();
}

inline void from_variant(const variant& v, dbus::path& p) {
    p = dbus::path(v.as_string());
}

} // namespace mc

#endif // MC_DBUS_PATH_H