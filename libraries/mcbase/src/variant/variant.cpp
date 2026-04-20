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
 * @file variant.cpp
 * @brief 实现 variant.h 中声明的方法
 */
#include <cstring>
#include <limits>
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/string.h>
#include <mc/variant.h>
#include <mc/variant/variant_base.h>
#include <mc/variant/variant_extension.h>
#include <stdexcept>

namespace mc {

// variant_extension_base 的实现
variant_extension_base::~variant_extension_base() = default;

mc::shared_ptr<variant_extension_base> variant_extension_base::deep_copy(detail::copy_context* ctx) const
{
    MC_UNUSED(ctx);
    return copy();
}

mc::shared_ptr<variant_extension_base> variant_extension_base::clone() const
{
    return copy();
}

extension_type_info variant_extension_base::get_type_info() const
{
    return extension_type_info();
}

extension_type_id_t variant_extension_base::get_type_id() const
{
    return get_type_info().id;
}

mc::string_view variant_extension_base::get_type_name() const
{
    return get_type_info().name;
}

std::size_t variant_extension_base::hash() const
{
    return reinterpret_cast<std::size_t>(this);
}

const variant_payload_type* variant_extension_base::payload_type() const
{
    return nullptr;
}

void* variant_extension_base::payload_address()
{
    return nullptr;
}

const void* variant_extension_base::payload_address() const
{
    return nullptr;
}

void* variant_extension_base::upcast_payload_to(const variant_payload_type& target)
{
    MC_UNUSED(target);
    return nullptr;
}

const void* variant_extension_base::upcast_payload_to(const variant_payload_type& target) const
{
    MC_UNUSED(target);
    return nullptr;
}

bool variant_extension_base::is_payload_type(const variant_payload_type& target) const
{
    return detail::payload_type_is_a(payload_type(), &target);
}

int64_t variant_extension_base::as_int64() const
{
    return 0;
}

uint64_t variant_extension_base::as_uint64() const
{
    return 0;
}

double variant_extension_base::as_double() const
{
    return 0;
}

bool variant_extension_base::as_bool() const
{
    return false;
}

mc::string variant_extension_base::as_string() const
{
    return mc::string(get_type_name());
}

void variant_extension_base::visit(std::function<void(const mc::variant&)>&& visitor) const
{
    MC_UNUSED(visitor);
}

bool variant_extension_base::supports_reference_access() const
{
    return false;
}

mc::variant* variant_extension_base::get_ptr(std::size_t index)
{
    MC_UNUSED(index);
    return nullptr;
}

const mc::variant* variant_extension_base::get_ptr(std::size_t index) const
{
    MC_UNUSED(index);
    return nullptr;
}

mc::variant* variant_extension_base::get_ptr(mc::string_view key)
{
    MC_UNUSED(key);
    return nullptr;
}

const mc::variant* variant_extension_base::get_ptr(mc::string_view key) const
{
    MC_UNUSED(key);
    return nullptr;
}

mc::variant variant_extension_base::get(std::size_t index) const
{
    throw_not_supported_error("扩展类型索引访问");
}

void variant_extension_base::set(std::size_t index, const mc::variant& value)
{
    throw_not_supported_error("扩展类型索引访问");
}

mc::variant variant_extension_base::get(mc::string_view key) const
{
    throw_not_supported_error("扩展类型键访问");
}

void variant_extension_base::set(mc::string_view key, const mc::variant& value)
{
    throw_not_supported_error("扩展类型键访问");
}

namespace detail {

/**
 * @brief 抛出方法参数类型不匹配异常
 *
 * @param method_name 方法名称
 * @param expect_type 期望的参数类型
 * @param actual_type 实际的参数类型
 */
void throw_method_arg_not_match(mc::string_view method_name, mc::string_view expect_type,
                                mc::string_view actual_type)
{
    MC_THROW(mc::invalid_arg_exception,
             "调用方法 ${method_name} 参数不匹配，需要 ${expect_type} 类型，实际提供 "
             "${actual_type} 类型",
             ("method_name", method_name)("expect_type", expect_type)("actual_type", actual_type));
}

} // namespace detail

// 获取类型名称
mc::string_view get_type_name_internal(type_id type)
{
    static const char* type_names[] = {
        "null",     // null_type
        "int8",     // int8_type
        "uint8",    // uint8_type
        "int16",    // int16_type
        "uint16",   // uint16_type
        "int32",    // int32_type
        "uint32",   // uint32_type
        "int64",    // int64_type
        "uint64",   // uint64_type
        "double",   // double_type
        "bool",     // bool_type
        "string",   // string_type
        "array",    // array_type
        "object",   // object_type
        "blob",     // blob_type
        "extension" // extension_type
    };

    int index = static_cast<int>(type);
    if (index >= 0 && index < static_cast<int>(type_id::max_type)) {
        return type_names[index];
    }

    return "unknown";
}

void throw_type_error(mc::string_view expected_type, type_id actual_type)
{
    if (actual_type < type_id::max_type) {
        MC_THROW(mc::invalid_arg_exception, "类型错误：期望${type}，实际为${actual_type}",
                 ("type", expected_type)("actual_type", get_type_name_internal(actual_type)));
    } else {
        MC_THROW(mc::invalid_arg_exception, "类型错误：期望${type}，实际为${actual_type}",
                 ("type", expected_type)("actual_type", static_cast<int>(actual_type)));
    }
}

void throw_unknow_type_error(type_id actual_type)
{
    MC_THROW(mc::invalid_arg_exception, "未知类型：${type}", ("type", static_cast<int>(actual_type)));
}

void throw_invalid_type_comparison_error(mc::string_view type1, mc::string_view type2, mc::string_view op)
{
    MC_THROW(mc::invalid_op_exception, "不支持的类型比较操作: ${type1} ${op} ${type2}",
             ("type1", type1)("op", op)("type2", type2));
}

void throw_invalid_type_operation_error(mc::string_view type1, mc::string_view type2, mc::string_view op)
{
    MC_THROW(mc::invalid_op_exception, "无效的类型操作: ${type1} ${op} ${type2}",
             ("type1", type1)("op", op)("type2", type2));
}

void throw_divide_by_zero_exception(mc::string_view msg)
{
    MC_THROW(mc::divide_by_zero_exception, "${msg}", ("msg", msg));
}

// 通用异常封装函数实现
void throw_out_of_range_error(mc::string_view msg)
{
    MC_THROW(mc::out_of_range_exception, "${msg}", ("msg", msg));
}

void throw_out_of_range_error(size_t index, size_t size)
{
    MC_THROW(mc::out_of_range_exception, "索引越界: 索引${index}超出范围[0, ${size})", ("index", index)("size", size));
}

void throw_bad_cast_error(mc::string_view msg)
{
    MC_THROW(mc::bad_cast_exception, "${msg}", ("msg", msg));
}

void throw_runtime_error(mc::string_view msg)
{
    MC_THROW(mc::runtime_exception, "${msg}", ("msg", msg));
}

void throw_not_supported_error(mc::string_view operation)
{
    MC_THROW(mc::invalid_op_exception, "不支持的操作: ${operation}", ("operation", operation));
}

void throw_extension_null_error()
{
    MC_THROW(mc::runtime_exception, "扩展对象为空");
}

void throw_container_overflow_error(mc::string_view container_type)
{
    MC_THROW(mc::overflow_exception, "容器${type}元素过多", ("type", container_type));
}

// 走 mc::string_hash（FNV-1a32）；不再为空串特判，保持 dict 桶与 quark 缓存一致
size_t calculate_str_hash(mc::string_view data)
{
    return mc::string_hash(data.data(), data.size());
}

} // namespace mc