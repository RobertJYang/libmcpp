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

#pragma once

#include <functional>
#include <mc/dict.h>
#include <mc/engine/base.h>
#include <mc/expr/function/call.h>
#include <mc/signal_slot.h>
#include <mc/variant.h>
#include <memory>
#include <vector>

namespace mc::engine {

// 前向声明
class property_base;
class abstract_interface;

namespace detail {

struct empty_observer {
    void notify(const mc::variant& value, const property_base& prop)
    {
    }
    void notify_update_shm(const mc::variant& value, const property_base& prop)
    {
    }
};

// 用于存储函数调用相关的数据
struct func_data {
    mc::expr::func func_obj;
    mc::dict       params;
};

// 将可选的属性数据打包到一个结构体中，避免每个property都分配这些内存
// 使用模板参数T来存储override_value，避免variant转换开销
template <typename T>
struct property_extension_data {
    std::function<mc::variant()>            getter;
    std::function<void(const mc::variant&)> setter;
    std::function<mc::variant()>            outsider_getter; // 外部getter方法
    std::function<bool(const mc::variant&)> outsider_setter; // 外部setter方法，返回false表示不进行实际设置
    std::unique_ptr<func_data>              function_data;   // 只有需要时才分配
    std::vector<mc::connection_type>        connection_slots;
    mutable std::unique_ptr<mc::variant>    ref_object_cache; // 缓存引用对象的 variant
    mutable std::unique_ptr<T>              override_value;   // 属性的Override值
};

class MC_API interface_observer {
public:
    interface_observer()
    {
    }

    void set_interface(abstract_interface* interface)
    {
        m_interface = interface;
    }

    abstract_interface* get_interface() const
    {
        return m_interface;
    }

    void notify(const mc::variant& value, const property_base& prop);
    void notify_update_shm(const mc::variant& value, const property_base& prop);

protected:
    // 不要初始化这个值，由 interface 的基类填充
    abstract_interface* m_interface;
};

} // namespace detail

} // namespace mc::engine