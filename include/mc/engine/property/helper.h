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
#include <mc/engine/property/types.h>
#include <mc/exception.h>
#include <mc/expr/function/call.h>
#include <mc/log.h>
#include <mc/variant.h>
#include <string>
#include <vector>

namespace mc::expr {
struct relate_property;
}

namespace mc::engine {

// 前向声明
class abstract_object;

namespace detail {
struct func_data;
}

/**
 * @brief property的辅助基类，包含与模板参数无关的工具方法
 *
 * 这个类将property中与模板参数T无关的逻辑抽取出来，
 * 减少模板展开时的代码重复，优化编译时间和内存使用
 */
class property_helper : public property_base {
public:
    property_helper()          = default;
    virtual ~property_helper() = default;

    // 查找相关对象的辅助方法
    abstract_object* find_related_object(const std::string& object_name);

    // 执行函数调用并返回结果
    mc::variant call_function_with_result(const detail::func_data* function_data);

    // 获取相关属性值
    MC_API mc::variant get_relate_property(const mc::expr::relate_property& relate_property);

    // 设置相关属性值
    MC_API void set_relate_property(const mc::expr::relate_property& relate_property, const mc::variant& value);

    // 连接属性变化监听器
    void connect_property_listener(abstract_object& target_object, const std::string& property_name,
                                   std::function<void()> callback);

    // 断开所有连接
    void disconnect_all_connections(std::vector<mc::connection_type>& connection_slots);

    // 按对象分组属性
    mc::dict group_properties_by_object(const mc::dict& relate_properties);

    // 为处理器提供的访问器接口
    virtual void        set_property_type(p_type type)              = 0;
    virtual p_type      get_property_type_enum() const              = 0;
    virtual void        set_variant_value(const mc::variant& value) = 0;
    virtual mc::variant get_variant_value() const                   = 0;
    // 添加内部设置值的方法，绕过from_variant，供processor使用
    virtual void               set_internal_value(const mc::variant& value)                        = 0;
    virtual bool               has_extension_data() const                                          = 0;
    virtual void               ensure_extension_data()                                             = 0;
    virtual void               set_ref_object_cache(std::unique_ptr<mc::variant> cache)            = 0;
    virtual mc::variant*       get_ref_object_cache() const                                        = 0;
    virtual void               set_getter_function(std::function<mc::variant()> getter)            = 0;
    virtual void               set_setter_function(std::function<void(const mc::variant&)> setter) = 0;
    virtual void               add_connection_slot(mc::connection_type slot)                       = 0;
    virtual void               clear_connection_slots()                                            = 0;
    virtual void               set_function_data(std::unique_ptr<detail::func_data> data)          = 0;
    virtual detail::func_data* get_function_data() const                                           = 0;
};

} // namespace mc::engine