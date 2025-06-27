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
class variant_extension_base : public mc::shared_base<variant_extension_base> {
public:
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