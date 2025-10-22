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
 * @file variant_extension.h
 * @brief 定义了 variant 扩展类型的基类，用于管理复杂类型
 */
#ifndef MC_VARIANT_EXTENSION_H
#define MC_VARIANT_EXTENSION_H

#include <memory>
#include <string>
#include <typeinfo>

#include <mc/memory.h>
#include <mc/traits.h>

namespace mc {

/**
 * @brief variant 扩展类型基类
 *
 * 所有需要在 variant 中管理的复杂类型都应该继承此类，
 * 提供统一的接口用于复制、克隆、类型信息等操作
 */
class MC_API variant_extension_base : public mc::enable_shared_from_this<variant_extension_base> {
public:
    /**
     * @brief 虚析构函数
     */
    virtual ~variant_extension_base();

    /**
     * @brief 创建当前对象的深拷贝
     * @return 返回当前对象的深拷贝
     */
    virtual mc::shared_ptr<variant_extension_base> clone() const = 0;

    /**
     * @brief 获取类型名称
     * @return 返回类型名称字符串
     */
    virtual std::string_view get_type_name() const = 0;

    /**
     * @brief 比较两个扩展对象是否相等
     * @param other 要比较的对象
     * @return 如果相等返回 true，否则返回 false
     */
    virtual bool equals(const variant_extension_base& other) const = 0;

    /**
     * @brief 计算对象的哈希值
     * @return 返回哈希值
     */
    virtual std::size_t hash() const {
        return reinterpret_cast<std::size_t>(this);
    }

    virtual int64_t as_int64() const {
        return 0;
    }
    virtual uint64_t as_uint64() const {
        return 0;
    }
    virtual double as_double() const {
        return 0;
    }
    virtual bool as_bool() const {
        return false;
    }
    virtual std::string as_string() const {
        return std::string(get_type_name());
    }
    virtual void visit(std::function<void(const mc::variant&)>&& visitor) const {
    }

    /**
     * @brief 查询是否支持零开销的引用访问
     * @return true 表示 extension 内部存储就是 variant，支持返回引用；false 需要值拷贝
     *
     * 如果返回 true，operator[] 将调用 get_ptr/get_ref 获取引用，零开销
     * 如果返回 false，operator[] 将调用 get 获取值，有拷贝开销
     */
    virtual bool supports_reference_access() const {
        return false; // 默认不支持，保持向后兼容
    }

    /**
     * @brief 通过索引获取元素指针（零开销访问）
     * @param index 索引位置
     * @return 返回指向内部 variant 的指针，如果不支持则返回 nullptr
     * @note 只有 supports_reference_access() 返回 true 时才会被调用
     */
    virtual mc::variant* get_ptr(std::size_t index) {
        return nullptr; // 默认不支持
    }

    /**
     * @brief 通过索引获取元素指针（const 版本）
     */
    virtual const mc::variant* get_ptr(std::size_t index) const {
        return nullptr; // 默认不支持
    }

    /**
     * @brief 通过键获取元素指针（零开销访问）
     * @param key 键名
     * @return 返回指向内部 variant 的指针，如果不支持则返回 nullptr
     * @note 只有 supports_reference_access() 返回 true 时才会被调用
     */
    virtual mc::variant* get_ptr(std::string_view key) {
        return nullptr; // 默认不支持
    }

    /**
     * @brief 通过键获取元素指针（const 版本）
     */
    virtual const mc::variant* get_ptr(std::string_view key) const {
        return nullptr; // 默认不支持
    }

    /**
     * @brief 通过索引获取元素（用于支持 operator[](size_t)）
     * @param index 索引位置
     * @return 返回指定位置的元素（值类型）
     * @throw std::runtime_error 如果不支持索引访问
     */
    virtual mc::variant get(std::size_t index) const;

    /**
     * @brief 通过索引设置元素（用于支持 operator[](size_t) 赋值）
     * @param index 索引位置
     * @param value 要设置的值
     * @throw std::runtime_error 如果不支持索引访问
     */
    virtual void set(std::size_t index, const mc::variant& value);

    /**
     * @brief 通过键获取元素（用于支持 operator[](string_view)）
     * @param key 键名
     * @return 返回指定键对应的元素（值类型）
     * @throw std::runtime_error 如果不支持键访问
     */
    virtual mc::variant get(std::string_view key) const;

    /**
     * @brief 通过键设置元素（用于支持 operator[](string_view) 赋值）
     * @param key 键名
     * @param value 要设置的值
     * @throw std::runtime_error 如果不支持键访问
     */
    virtual void set(std::string_view key, const mc::variant& value);
};

template <typename T>
class variant_extension : public variant_extension_base {
public:
    using value_type = T;

    virtual mc::shared_ptr<variant_extension_base> clone() const override {
        return mc::make_shared<T>(static_cast<const T&>(*this));
    }

    virtual std::string_view get_type_name() const override {
        return mc::pretty_name<T>();
    }

    virtual bool equals(const variant_extension_base& other) const override {
        if (auto other_extension = dynamic_cast<const variant_extension<T>*>(&other)) {
            if constexpr (mc::traits::has_operator_equal_v<T, T>) {
                return static_cast<const T&>(*this) == static_cast<const T&>(*other_extension);
            }
        }
        return false;
    }

    mc::shared_ptr<T> shared_from_this() const {
        return mc::shared_ptr<T>(static_cast<const T*>(this));
    }
    mc::shared_ptr<T> shared_from_this() {
        return mc::shared_ptr<T>(static_cast<T*>(this));
    }
    mc::weak_ptr<T> weak_from_this() const {
        return mc::weak_ptr<T>(static_cast<const T*>(this));
    }
    mc::weak_ptr<T> weak_from_this() {
        return mc::weak_ptr<T>(static_cast<T*>(this));
    }
};

} // namespace mc

#endif // MC_VARIANT_EXTENSION_H