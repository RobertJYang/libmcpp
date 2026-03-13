/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
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
 * @file json_wrapper.cpp
 * @brief 基于白泽 C JSON 库的 C++ 封装实现
 */

#include <mc/json_wrapper.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <cmath>

#include <json_api.h>
#include <mc/common.h>
#include <mc/dict/dict.h>
#include <mc/exception.h>

namespace mc {
namespace json_wrapper {

namespace {

// 将 C 接口返回码转换为异常
inline void check_json_ret(uint32_t ret, std::string_view msg)
{
    if (ret == JSON_OK || ret == JSON_NUMBER_TYPE_MISMATCH) {
        return;
    }
    MC_THROW(mc::parse_error_exception, "JSON operation failed: ${msg}, code=${code}",
             ("msg", std::string(msg))("code", static_cast<int>(ret)));
}

// 新建 Json* 失败时抛出异常
inline Json* ensure_created(Json* json, std::string_view msg)
{
    if (json == nullptr) {
        MC_THROW(mc::parse_error_exception, "Failed to create JSON node: ${msg}", ("msg", std::string(msg)));
    }
    return json;
}

// 比较两个 Json 节点的内容是否相等（递归比较）
bool json_equal(const Json* lhs, const Json* rhs);

bool json_array_equal(const Json* lhs, const Json* rhs)
{
    uint32_t lhs_size = 0;
    uint32_t rhs_size = 0;
    check_json_ret(JsonArraySizeGet(lhs, &lhs_size), "Failed to get array size (lhs)");
    check_json_ret(JsonArraySizeGet(rhs, &rhs_size), "Failed to get array size (rhs)");
    if (lhs_size != rhs_size) {
        return false;
    }
    for (uint32_t i = 0; i < lhs_size; ++i) {
        Json* lhs_item = nullptr;
        Json* rhs_item = nullptr;
        check_json_ret(JsonArrayItemGet(lhs, i, &lhs_item), "Failed to get array item (lhs)");
        check_json_ret(JsonArrayItemGet(rhs, i, &rhs_item), "Failed to get array item (rhs)");

        // 处理 Quote 类型
        Json* lhs_actual = lhs_item;
        if (lhs_item != nullptr && JsonIsQuote(lhs_item)) {
            Json* actual = nullptr;
            check_json_ret(JsonObjectQuoteGet(lhs_item, &actual), "Failed to get quote object (lhs)");
            lhs_actual = actual;
        }

        Json* rhs_actual = rhs_item;
        if (rhs_item != nullptr && JsonIsQuote(rhs_item)) {
            Json* actual = nullptr;
            check_json_ret(JsonObjectQuoteGet(rhs_item, &actual), "Failed to get quote object (rhs)");
            rhs_actual = actual;
        }

        if (!json_equal(lhs_actual, rhs_actual)) {
            return false;
        }
    }
    return true;
}

bool json_object_equal(const Json* lhs, const Json* rhs)
{
    // 先比较键值对数量
    uint32_t lhs_count = 0;
    uint32_t rhs_count = 0;

    auto count_members = [](const Json* obj) -> uint32_t {
        uint32_t count = 0;
        Json*    child = nullptr;
        uint32_t ret   = JsonItemFirstChild(const_cast<Json*>(obj), &child);
        if (ret == JSON_NO_FIRST_CHILD) {
            return 0;
        }
        check_json_ret(ret, "Failed to get first child");
        while (child != nullptr) {
            ++count;
            Json*    next = nullptr;
            uint32_t r    = JsonItemNextSibling(child, &next);
            if (r == JSON_NO_NEXT_SIBLING) {
                break;
            }
            check_json_ret(r, "Failed to get next sibling");
            child = next;
        }
        return count;
    };

    lhs_count = count_members(lhs);
    rhs_count = count_members(rhs);
    if (lhs_count != rhs_count) {
        return false;
    }

    // 遍历 lhs 的所有 key，在 rhs 中查找并比较
    Json*    child = nullptr;
    uint32_t ret   = JsonItemFirstChild(const_cast<Json*>(lhs), &child);
    if (ret == JSON_NO_FIRST_CHILD) {
        return true;
    }
    check_json_ret(ret, "Failed to get first child of object");

    while (child != nullptr) {
        char* key_c = nullptr;
        check_json_ret(JsonItemKeyGet(child, &key_c), "Failed to get object key");

        Json* rhs_item = nullptr;
        ret            = JsonObjectItemGet(rhs, key_c, &rhs_item);
        if (ret != JSON_OK || rhs_item == nullptr) {
            return false;
        }

        // 处理 Quote 类型
        Json* lhs_actual = child;
        if (JsonIsQuote(child)) {
            Json* actual = nullptr;
            check_json_ret(JsonObjectQuoteGet(child, &actual), "Failed to get quote object (lhs)");
            lhs_actual = actual;
        }

        Json* rhs_actual = rhs_item;
        if (JsonIsQuote(rhs_item)) {
            Json* actual = nullptr;
            check_json_ret(JsonObjectQuoteGet(rhs_item, &actual), "Failed to get quote object (rhs)");
            rhs_actual = actual;
        }

        if (!json_equal(lhs_actual, rhs_actual)) {
            return false;
        }

        Json*    next = nullptr;
        uint32_t r    = JsonItemNextSibling(child, &next);
        if (r == JSON_NO_NEXT_SIBLING) {
            break;
        }
        check_json_ret(r, "Failed to get next child of object");
        child = next;
    }
    return true;
}

bool json_equal(const Json* lhs, const Json* rhs)
{
    if (lhs == rhs) {
        return true;
    }
    if (lhs == nullptr || rhs == nullptr) {
        return false;
    }

    JsonType lhs_type = JsonTypeGet(lhs);
    JsonType rhs_type = JsonTypeGet(rhs);
    if (lhs_type != rhs_type) {
        return false;
    }

    switch (lhs_type) {
        case JSONTYPE_NULL:
            return true;
        case JSONTYPE_TRUE:
        case JSONTYPE_FALSE: {
            bool lhs_val = false;
            bool rhs_val = false;
            check_json_ret(JsonItemBoolValueGet(lhs, &lhs_val), "Failed to get bool value (lhs)");
            check_json_ret(JsonItemBoolValueGet(rhs, &rhs_val), "Failed to get bool value (rhs)");
            return lhs_val == rhs_val;
        }
        case JSONTYPE_NUMBER: {
            // 先尽量按整数比较，如果存在类型不匹配则统一用 double 比较
            int64_t  lhs_int = 0;
            int64_t  rhs_int = 0;
            uint32_t rl      = JsonItemIntegerValueGet(const_cast<Json*>(lhs), &lhs_int);
            uint32_t rr      = JsonItemIntegerValueGet(const_cast<Json*>(rhs), &rhs_int);
            if (rl == JSON_OK && rr == JSON_OK) {
                return lhs_int == rhs_int;
            }

            double lhs_d = 0.0;
            double rhs_d = 0.0;
            check_json_ret(JsonItemDoubleValueGet(lhs, &lhs_d), "Failed to get double value (lhs)");
            check_json_ret(JsonItemDoubleValueGet(rhs, &rhs_d), "Failed to get double value (rhs)");
            return lhs_d == rhs_d;
        }
        case JSONTYPE_STRING: {
            char* lhs_str = nullptr;
            char* rhs_str = nullptr;
            check_json_ret(JsonItemStringValueGet(lhs, &lhs_str), "Failed to get string value (lhs)");
            check_json_ret(JsonItemStringValueGet(rhs, &rhs_str), "Failed to get string value (rhs)");
            if (lhs_str == nullptr || rhs_str == nullptr) {
                return lhs_str == rhs_str;
            }
            return std::strcmp(lhs_str, rhs_str) == 0;
        }
        case JSONTYPE_ARRAY:
            return json_array_equal(lhs, rhs);
        case JSONTYPE_OBJECT:
            return json_object_equal(lhs, rhs);
        case JSONTYPE_QUOTE: {
            Json* lhs_obj = nullptr;
            Json* rhs_obj = nullptr;
            check_json_ret(JsonObjectQuoteGet(lhs, &lhs_obj), "Failed to get quote object (lhs)");
            check_json_ret(JsonObjectQuoteGet(rhs, &rhs_obj), "Failed to get quote object (rhs)");
            return json_equal(lhs_obj, rhs_obj);
        }
        default:
            return false;
    }
}

// 将 mc::variant 递归转换为 Json*
Json* build_json_from_variant(const mc::variant& value)
{
    Json* json = nullptr;
    value.visit_with([&](auto&& v) {
        using T      = std::decay_t<decltype(v)>;
        uint32_t ret = JSON_OK;

        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            ret = JsonNullCreate(&json);
        } else if constexpr (std::is_same_v<T, bool>) {
            ret = JsonBoolCreate(v, &json);
        } else if constexpr (std::is_integral_v<T>) {
            ret = JsonIntegerCreate(static_cast<int64_t>(v), &json);
        } else if constexpr (std::is_floating_point_v<T>) {
            ret = JsonDoubleCreate(static_cast<double>(v), &json);
        } else if constexpr (std::is_same_v<T, mc::variant::string_type>) {
            ret = JsonStringCreateWithLen(v.data(), static_cast<uint32_t>(v.size()), &json);
        } else if constexpr (std::is_same_v<T, mc::variant::blob_type>) {
            auto sv = v.as_string_view();
            ret     = JsonStringCreateWithLen(sv.data(), static_cast<uint32_t>(sv.size()), &json);
        } else if constexpr (std::is_same_v<T, mc::variant::array_type>) {
            ret = JsonArrayCreate(&json);
            check_json_ret(ret, "Failed to create array node");
            for (const auto& item : v) {
                Json* child = build_json_from_variant(item);
                ensure_created(child, "Failed to build array element");
                check_json_ret(JsonItemAddToArray(child, json), "Failed to add array element");
            }
        } else if constexpr (std::is_same_v<T, mc::variant::object_type>) {
            ret = JsonObjectCreate(&json);
            check_json_ret(ret, "Failed to create object node");
            for (const auto& entry : v) {
                const auto& key_var = entry.key;
                const auto& val_var = entry.value;
                std::string key_str;
                if (!entry.key.is_string()) {
                    key_str = key_var.as_string();
                } else {
                    key_str = key_var.get_string();
                }
                Json*       child   = build_json_from_variant(val_var);
                ensure_created(child, "Failed to build object element");
                check_json_ret(JsonItemAddToObject(key_str.c_str(), child, json), "Failed to add object element");
            }
        } else if constexpr (std::is_same_v<T, mc::variant::extension_type>) {
            auto s = v.as_string();
            ret    = JsonStringCreateWithLen(s.data(), static_cast<uint32_t>(s.size()), &json);
        } else {
            MC_THROW(mc::parse_error_exception, "Unsupported variant type for JSON conversion: ${type}",
                     ("type", mc::pretty_name<T>()));
        }

        check_json_ret(ret, "Failed to build JSON node");
    });

    return ensure_created(json, "Failed to build JSON node");
}

// 将 Json* 递归转换为 mc::variant
mc::variant build_variant_from_json(const Json* json)
{
    if (json == nullptr) {
        return mc::variant();
    }

    JsonType type = JsonTypeGet(json);
    switch (type) {
        case JSONTYPE_NULL:
            return mc::variant();
        case JSONTYPE_TRUE:
        case JSONTYPE_FALSE: {
            bool v = false;
            check_json_ret(JsonItemBoolValueGet(json, &v), "Failed to get bool value");
            return mc::variant(v);
        }
        case JSONTYPE_NUMBER: {
            int64_t  iv  = 0;
            uint32_t ret = JsonItemIntegerValueGet(const_cast<Json*>(json), &iv);
            if (ret == JSON_OK) {
                return mc::variant(iv);
            }
            double dv = 0.0;
            check_json_ret(JsonItemDoubleValueGet(json, &dv), "Failed to get double value");
            return mc::variant(dv);
        }
        case JSONTYPE_STRING: {
            char* str = nullptr;
            check_json_ret(JsonItemStringValueGet(json, &str), "Failed to get string value");
            if (str == nullptr) {
                return mc::variant();
            }
            // 直接使用 const char* 构造 variant，避免先构造 std::string
            // variant 可以从 const char* 直接构造（通过 string_view）
            return mc::variant(str);
        }
        case JSONTYPE_ARRAY: {
            uint32_t size = 0;
            check_json_ret(JsonArraySizeGet(json, &size), "Failed to get array size");
            if (size == 0) {
                return mc::variant(mc::variants());
            }
            mc::variants arr;
            arr.reserve(size);
            for (uint32_t i = 0; i < size; ++i) {
                Json* item = nullptr;
                check_json_ret(JsonArrayItemGet(json, i, &item), "Failed to get array item");

                // 如果 item 是 Quote 类型，JsonTypeGet(item) 会返回 JSONTYPE_QUOTE
                // 递归调用 build_variant_from_json(item) 会进入 JSONTYPE_QUOTE 分支处理
                // 因此这里不需要检查 JsonIsQuote
                arr.push_back(build_variant_from_json(item));
            }
            return mc::variant(arr);
        }
        case JSONTYPE_OBJECT: {
            mc::dict obj;
            Json*    child = nullptr;
            uint32_t ret   = JsonItemFirstChild(const_cast<Json*>(json), &child);
            if (ret == JSON_NO_FIRST_CHILD) {
                return mc::variant(obj);
            }
            check_json_ret(ret, "Failed to get first child of object");

            while (child != nullptr) {
                char* key_c = nullptr;
                check_json_ret(JsonItemKeyGet(child, &key_c), "Failed to get object key");
                // 直接使用 const char* 构造 variant，避免先构造 std::string
                // variant 可以从 const char* 直接构造（通过 string_view）
                // 一般键值不会是空，所以优先使用 key_c，只有在为空时才使用空字符串
                mc::variant key_var(key_c ? key_c : "");

                // 如果 child 是 Quote 类型，JsonTypeGet(child) 会返回 JSONTYPE_QUOTE
                // 递归调用 build_variant_from_json(child) 会进入 JSONTYPE_QUOTE 分支处理
                // 因此这里不需要检查 JsonIsQuote
                obj(key_var, build_variant_from_json(child));

                Json*    next = nullptr;
                uint32_t r    = JsonItemNextSibling(child, &next);
                if (r == JSON_NO_NEXT_SIBLING) {
                    break;
                }
                check_json_ret(r, "Failed to get next child");
                child = next;
            }
            return mc::variant(obj);
        }
        case JSONTYPE_QUOTE: {
            Json* target = nullptr;
            check_json_ret(JsonObjectQuoteGet(const_cast<Json*>(json), &target), "Failed to get quote object");
            return build_variant_from_json(target);
        }
        default:
            MC_THROW(mc::parse_error_exception, "Unsupported JSON node type");
    }
}

// 引用计数封装
inline void add_ref(Json* json)
{
    if (json != nullptr) {
        (void)JsonObjectAddRef(json);
    }
}

inline void release_ref(Json* json)
{
    if (json != nullptr) {
        (void)JsonObjectRelease(json);
    }
}

} // namespace

// ======================== JsonValue ========================

JsonValue::JsonValue() noexcept
    : m_json(nullptr)
{
}

JsonValue::JsonValue(Json* json) noexcept
    : m_json(json)
{
}

JsonValue::JsonValue(const JsonValue& other)
    : m_json(other.m_json)
{
    add_ref(m_json);
}

JsonValue& JsonValue::operator=(const JsonValue& other)
{
    if (this == &other) {
        return *this;
    }
    add_ref(other.m_json);
    release_ref(m_json);
    m_json = other.m_json;
    return *this;
}

JsonValue::JsonValue(JsonValue&& other) noexcept
    : m_json(other.m_json)
{
    other.m_json = nullptr;
}

JsonValue& JsonValue::operator=(JsonValue&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    release_ref(m_json);
    m_json       = other.m_json;
    other.m_json = nullptr;
    return *this;
}

JsonValue::~JsonValue()
{
    release_ref(m_json);
    m_json = nullptr;
}

JsonValue& JsonValue::operator=(int64_t value)
{
    if (m_json != nullptr && is_int()) {
        if (JsonItemIntegerValueSet(m_json, value) != JSON_OK) {
            MC_THROW(mc::parse_error_exception, "Failed to set integer value");
        }
        return *this;
    }
    Json* new_json = nullptr;
    if (JsonIntegerCreate(value, &new_json) != JSON_OK || new_json == nullptr) {
        MC_THROW(mc::parse_error_exception, "create json integer object failed");
    }
    release_ref(m_json);
    m_json = new_json;
    return *this;
}

JsonValue& JsonValue::operator=(bool value)
{
    if (m_json != nullptr && is_bool()) {
        if (JsonItemBoolValueSet(m_json, value) != JSON_OK) {
            MC_THROW(mc::parse_error_exception, "Failed to set bool value");
        }
        return *this;
    }
    Json* new_json = nullptr;
    if (JsonBoolCreate(value, &new_json) != JSON_OK || new_json == nullptr) {
        MC_THROW(mc::parse_error_exception, "create json bool object failed");
    }
    release_ref(m_json);
    m_json = new_json;
    return *this;
}

JsonValue& JsonValue::operator=(double value)
{
    if (m_json != nullptr && is_double()) {
        if (JsonItemDoubleValueSet(m_json, value) != JSON_OK) {
            MC_THROW(mc::parse_error_exception, "Failed to set double value");
        }
        return *this;
    }
    Json* new_json = nullptr;
    if (JsonDoubleCreate(value, &new_json) != JSON_OK || new_json == nullptr) {
        MC_THROW(mc::parse_error_exception, "create json double object failed");
    }
    release_ref(m_json);
    m_json = new_json;
    return *this;
}

JsonValue& JsonValue::operator=(std::string_view value)
{
    if (m_json != nullptr && is_string()) {
        if (JsonItemStringValueSet(m_json, value.data(),
                                   static_cast<uint32_t>(value.size())) != JSON_OK) {
            MC_THROW(mc::parse_error_exception, "Failed to set string value");
        }
        return *this;
    }
    Json* new_json = nullptr;
    if (JsonStringCreateWithLen(value.data(),
                                static_cast<uint32_t>(value.size()),
                                &new_json) != JSON_OK ||
        new_json == nullptr) {
        MC_THROW(mc::parse_error_exception, "create json string object failed");
    }
    release_ref(m_json);
    m_json = new_json;
    return *this;
}

JsonValue& JsonValue::operator=(const std::string value)
{
    return *this = std::string_view(value);
}

JsonValue& JsonValue::operator=(const JsonArray& value)
{
    return *this = JsonValue(value);
}

JsonValue& JsonValue::operator=(const JsonObject& value)
{
    return *this = JsonValue(value);
}

JsonValue& JsonValue::operator=(const std::vector<JsonValue>& value)
{
    JsonValue arr = JsonValue::make_array();
    JsonArray a   = arr.as_array();
    for (const auto& e : value) {
        a.push_back(e);
    }
    return *this = std::move(arr);
}

JsonValue& JsonValue::operator=(const std::vector<int64_t>& value)
{
    JsonValue arr = JsonValue::make_array();
    JsonArray a   = arr.as_array();
    for (int64_t e : value) {
        a.push_back(JsonValue(e));
    }
    return *this = std::move(arr);
}

JsonValue& JsonValue::operator=(const std::vector<double>& value)
{
    JsonValue arr = JsonValue::make_array();
    JsonArray a   = arr.as_array();
    for (double e : value) {
        a.push_back(JsonValue(e));
    }
    return *this = std::move(arr);
}

JsonValue& JsonValue::operator=(const std::vector<bool>& value)
{
    JsonValue arr = JsonValue::make_array();
    JsonArray a   = arr.as_array();
    for (bool e : value) {
        a.push_back(JsonValue(e));
    }
    return *this = std::move(arr);
}

JsonValue& JsonValue::operator=(const std::vector<std::string>& value)
{
    JsonValue arr = JsonValue::make_array();
    JsonArray a   = arr.as_array();
    for (const auto& e : value) {
        a.push_back(JsonValue(std::string_view(e)));
    }
    return *this = std::move(arr);
}

JsonValue& JsonValue::operator=(const std::vector<std::string_view>& value)
{
    JsonValue arr = JsonValue::make_array();
    JsonArray a   = arr.as_array();
    for (std::string_view e : value) {
        a.push_back(JsonValue(e));
    }
    return *this = std::move(arr);
}

JsonValue::JsonValue(bool value)
{
    Json* object = nullptr;
    if (JsonBoolCreate(value, &object) != JSON_OK || object == nullptr) {
        MC_THROW(mc::parse_error_exception, "create json bool object failed");
    }
    m_json = object;
}

JsonValue::JsonValue(int64_t value)
{
    Json* object = nullptr;
    if (JsonIntegerCreate(value, &object) != JSON_OK || object == nullptr) {
        MC_THROW(mc::parse_error_exception, "create json integer object failed");
    }
    m_json = object;
}

JsonValue::JsonValue(double value)
{
    Json* object = nullptr;
    if (JsonDoubleCreate(value, &object) != JSON_OK || object == nullptr) {
        MC_THROW(mc::parse_error_exception, "create json double object failed");
    }
    m_json = object;
}

JsonValue::JsonValue(std::string_view value)
{
    Json* object = nullptr;
    if (JsonStringCreateWithLen(value.data(),
                                static_cast<uint32_t>(value.size()),
                                &object) != JSON_OK ||
        object == nullptr) {
        MC_THROW(mc::parse_error_exception, "create json string object failed");
    }
    m_json = object;
}

JsonValue::JsonValue(const char* value)
    : JsonValue(value != nullptr ? std::string_view(value) : std::string_view(""))
{
}

JsonValue::JsonValue(const JsonArray& value)
    : m_json(value.get_raw())
{
    add_ref(m_json);
}

JsonValue::JsonValue(const JsonObject& value)
    : m_json(value.get_raw())
{
    add_ref(m_json);
}

JsonValue::JsonValue(const std::vector<JsonValue>& value)
    : JsonValue(JsonValue::make_array())
{
    JsonArray a = as_array();
    for (const auto& e : value) {
        a.push_back(e);
    }
}

JsonValue::JsonValue(const std::vector<int64_t>& value)
    : JsonValue(JsonValue::make_array())
{
    JsonArray a = as_array();
    for (int64_t e : value) {
        a.push_back(JsonValue(e));
    }
}

JsonValue::JsonValue(const std::vector<double>& value)
    : JsonValue(JsonValue::make_array())
{
    JsonArray a = as_array();
    for (double e : value) {
        a.push_back(JsonValue(e));
    }
}

JsonValue::JsonValue(const std::vector<bool>& value)
    : JsonValue(JsonValue::make_array())
{
    JsonArray a = as_array();
    for (bool e : value) {
        a.push_back(JsonValue(e));
    }
}

JsonValue::JsonValue(const std::vector<std::string>& value)
    : JsonValue(JsonValue::make_array())
{
    JsonArray a = as_array();
    for (const auto& e : value) {
        a.push_back(JsonValue(std::string_view(e)));
    }
}

JsonValue::JsonValue(const std::vector<std::string_view>& value)
    : JsonValue(JsonValue::make_array())
{
    JsonArray a = as_array();
    for (std::string_view e : value) {
        a.push_back(JsonValue(e));
    }
}

template <typename T,
          typename std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int>>
JsonValue& JsonValue::operator=(const std::vector<T>& value)
{
    JsonValue arr = JsonValue::make_array();
    JsonArray a   = arr.as_array();
    for (const auto& e : value) {
        a.push_back(JsonValue(static_cast<int64_t>(e)));
    }
    return *this = std::move(arr);
}

template <typename T,
          typename std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int>>
JsonValue::JsonValue(const std::vector<T>& value)
    : JsonValue(JsonValue::make_array())
{
    JsonArray a = as_array();
    for (const auto& e : value) {
        a.push_back(JsonValue(static_cast<int64_t>(e)));
    }
}

// 显式实例化整型 vector，避免头文件中 JsonArray 不完整类型
template JsonValue& JsonValue::operator=(const std::vector<int>&);
template JsonValue& JsonValue::operator=(const std::vector<unsigned int>&);
template JsonValue& JsonValue::operator=(const std::vector<long>&);
template JsonValue& JsonValue::operator=(const std::vector<unsigned long>&);
template JsonValue& JsonValue::operator=(const std::vector<long long>&);
template JsonValue& JsonValue::operator=(const std::vector<unsigned long long>&);
template JsonValue::JsonValue(const std::vector<int>&);
template JsonValue::JsonValue(const std::vector<unsigned int>&);
template JsonValue::JsonValue(const std::vector<long>&);
template JsonValue::JsonValue(const std::vector<unsigned long>&);
template JsonValue::JsonValue(const std::vector<long long>&);
template JsonValue::JsonValue(const std::vector<unsigned long long>&);

JsonValue JsonValue::make_null()
{
    Json* object = nullptr;
    if (JsonNullCreate(&object) != JSON_OK || object == nullptr) {
        MC_THROW(mc::parse_error_exception, "create json null object failed");
    }
    return JsonValue(object);
}

JsonValue JsonValue::make_bool(bool value)
{
    Json* object = nullptr;
    if (JsonBoolCreate(value, &object) != JSON_OK || object == nullptr) {
        MC_THROW(mc::parse_error_exception, "create json bool object failed");
    }
    return JsonValue(object);
}

JsonValue JsonValue::make_int(int64_t value)
{
    Json* object = nullptr;
    if (JsonIntegerCreate(value, &object) != JSON_OK || object == nullptr) {
        MC_THROW(mc::parse_error_exception, "create json integer object failed");
    }
    return JsonValue(object);
}

JsonValue JsonValue::make_double(double value)
{
    Json* object = nullptr;
    if (JsonDoubleCreate(value, &object) != JSON_OK || object == nullptr) {
        MC_THROW(mc::parse_error_exception, "create json double object failed");
    }
    return JsonValue(object);
}

JsonValue JsonValue::make_string(std::string_view value)
{
    Json* object = nullptr;
    if (JsonStringCreateWithLen(value.data(),
                                static_cast<uint32_t>(value.size()),
                                &object) != JSON_OK ||
        object == nullptr) {
        MC_THROW(mc::parse_error_exception, "create json string object failed");
    }
    return JsonValue(object);
}

JsonValue JsonValue::make_array()
{
    Json* object = nullptr;
    if (JsonArrayCreate(&object) != JSON_OK || object == nullptr) {
        MC_THROW(mc::parse_error_exception, "create json array object failed");
    }
    return JsonValue(object);
}

JsonValue JsonValue::make_object()
{
    Json* object = nullptr;
    if (JsonObjectCreate(&object) != JSON_OK || object == nullptr) {
        MC_THROW(mc::parse_error_exception, "create json object object failed");
    }
    return JsonValue(object);
}

JsonValueType JsonValue::type() const
{
    if (m_json == nullptr) {
        return JsonValueType::Undefined;
    }
    if (JsonIsNull(m_json)) {
        return JsonValueType::Null;
    }
    if (JsonIsBool(m_json)) {
        return JsonValueType::Bool;
    }
    if (JsonIsInteger(m_json)) {
        return JsonValueType::Integer;
    }
    if (JsonIsDouble(m_json)) {
        return JsonValueType::Double;
    }
    if (JsonIsString(m_json)) {
        return JsonValueType::String;
    }
    if (JsonIsArray(m_json)) {
        return JsonValueType::Array;
    }
    if (JsonIsObject(m_json)) {
        return JsonValueType::Object;
    }
    return JsonValueType::Undefined;
}

bool JsonValue::is_undefined() const
{
    return m_json == nullptr;
}

bool JsonValue::is_null() const
{
    return m_json && JsonIsNull(m_json);
}

bool JsonValue::is_bool() const
{
    return m_json && JsonIsBool(m_json);
}

bool JsonValue::is_int() const
{
    return m_json && JsonIsInteger(m_json);
}

bool JsonValue::is_double() const
{
    return m_json && JsonIsDouble(m_json);
}

bool JsonValue::is_number() const
{
    return m_json && JsonIsNumber(m_json);
}

bool JsonValue::is_string() const
{
    return m_json && JsonIsString(m_json);
}

bool JsonValue::is_array() const
{
    return m_json && JsonIsArray(m_json);
}

bool JsonValue::is_object() const
{
    return m_json && JsonIsObject(m_json);
}

bool JsonValue::as_bool() const
{
    if (!is_bool()) {
        MC_THROW(mc::bad_cast_exception, "JSON type is not bool");
    }
    bool value = false;
    check_json_ret(JsonItemBoolValueGet(m_json, &value), "Failed to get bool value");
    return value;
}

int64_t JsonValue::as_int() const
{
    if (!is_number()) {
        MC_THROW(mc::bad_cast_exception, "JSON type is not number");
    }
    int64_t  value = 0;
    uint32_t ret   = JsonItemIntegerValueGet(m_json, &value);
    if (ret == JSON_NUMBER_TYPE_MISMATCH) {
        double d = 0.0;
        check_json_ret(JsonItemDoubleValueGet(m_json, &d), "Failed to get double value");
        return static_cast<int64_t>(d);
    }
    check_json_ret(ret, "Failed to get integer value");
    return value;
}

double JsonValue::as_double() const
{
    if (!is_number()) {
        MC_THROW(mc::bad_cast_exception, "JSON type is not number");
    }
    double value = 0.0;
    check_json_ret(JsonItemDoubleValueGet(m_json, &value), "Failed to get double value");
    return value;
}

std::string JsonValue::as_string() const
{
    if (!is_string()) {
        MC_THROW(mc::bad_cast_exception, "JSON type is not string");
    }
    char* value = nullptr;
    check_json_ret(JsonItemStringValueGet(m_json, &value), "Failed to get string value");
    if (value == nullptr) {
        return {};
    }
    return std::string(value);
}

JsonArray JsonValue::as_array() const
{
    if (!is_array()) {
        MC_THROW(mc::bad_cast_exception, "JSON type is not array");
    }
    return JsonArray(m_json);
}

JsonObject JsonValue::as_object() const
{
    if (!is_object()) {
        MC_THROW(mc::bad_cast_exception, "JSON type is not object");
    }
    return JsonObject(m_json);
}

mc::variant JsonValue::to_variant() const
{
    return build_variant_from_json(m_json);
}

JsonValue JsonValue::from_variant(const mc::variant& value)
{
    Json* json = build_json_from_variant(value);
    return JsonValue(json);
}

bool JsonValue::operator==(const JsonValue& other) const
{
    return json_equal(m_json, other.m_json);
}

bool JsonValue::operator==(int64_t value) const
{
    if (!is_number()) {
        return false;
    }
    return as_int() == value;
}

bool JsonValue::operator==(bool value) const
{
    if (!is_bool()) {
        return false;
    }
    return as_bool() == value;
}

bool JsonValue::operator==(double value) const
{
    if (!is_number()) {
        return false;
    }
    double self_val = is_int() ? static_cast<double>(as_int()) : as_double();
    return MC_FLOAT_EQUAL(self_val, value, MC_FLOAT_EPSILON);
}

bool JsonValue::operator==(std::string_view value) const
{
    if (!is_string()) {
        return false;
    }
    return std::string_view(as_string()) == value;
}

bool operator==(int64_t lhs, const JsonValue& rhs)
{
    return rhs == lhs;
}

bool operator!=(int64_t lhs, const JsonValue& rhs)
{
    return !(rhs == lhs);
}

bool operator==(bool lhs, const JsonValue& rhs)
{
    return rhs == lhs;
}

bool operator!=(bool lhs, const JsonValue& rhs)
{
    return !(rhs == lhs);
}

bool operator==(double lhs, const JsonValue& rhs)
{
    return rhs == lhs;
}

bool operator!=(double lhs, const JsonValue& rhs)
{
    return !(rhs == lhs);
}

bool operator==(std::string_view lhs, const JsonValue& rhs)
{
    return rhs == lhs;
}

bool operator!=(std::string_view lhs, const JsonValue& rhs)
{
    return !(rhs == lhs);
}

bool operator==(const std::string lhs, const JsonValue& rhs)
{
    return rhs == lhs;
}

bool operator!=(const std::string lhs, const JsonValue& rhs)
{
    return !(rhs == lhs);
}

bool operator==(const char* lhs, const JsonValue& rhs)
{
    return rhs == std::string_view(lhs != nullptr ? lhs : "");
}

bool operator!=(const char* lhs, const JsonValue& rhs)
{
    return !(rhs == std::string_view(lhs != nullptr ? lhs : ""));
}

JsonValue JsonValue::operator[](uint32_t index) const
{
    if (!is_array()) {
        MC_THROW(mc::bad_cast_exception, "JSON type is not array");
    }
    return as_array().at(index);
}

JsonValue JsonValue::operator[](std::string_view key) const
{
    if (!is_object()) {
        MC_THROW(mc::bad_cast_exception, "JSON type is not object");
    }
    return as_object().get(key);
}

JsonArrayValue JsonValue::operator[](uint32_t index)
{
    if (is_undefined() || is_null()) {
        return JsonArrayValue(this, index);
    }
    if (!is_array()) {
        MC_THROW(mc::bad_cast_exception, "JSON type is not array");
    }
    if (index > as_array().size()) {
        MC_THROW(mc::out_of_range_exception, "Index out of range");
    }
    if (index == as_array().size()) {
        return JsonArrayValue(this, index);
    }
    return JsonArrayValue(this, index, as_array().at(index));
}

JsonObjectValue JsonValue::operator[](std::string_view key)
{
    if (is_undefined() || is_null()) {
        return JsonObjectValue(this, key);
    }
    if (!is_object()) {
        MC_THROW(mc::bad_cast_exception, "JSON type is not object");
    }
    return as_object().has(key) ? JsonObjectValue(this, key, as_object().get(key))
                                : JsonObjectValue(this, key);
}

JsonValue JsonValue::new_from_raw(Json* raw_ptr)
{
    if (raw_ptr == nullptr) {
        MC_THROW(mc::bad_cast_exception, "Cannot create JsonValue from null pointer");
    }
    // 增加引用计数
    add_ref(raw_ptr);
    return JsonValue(raw_ptr);
}

// ======================== JsonArrayValue ========================

void JsonArrayValue::set_value_impl(const JsonValue& value)
{
    // 如果父对象为空，则创建一个空数组
    if (m_parent->is_undefined() || m_parent->is_null()) {
        JsonObjectValue* obj_ref = dynamic_cast<JsonObjectValue*>(m_parent);
        JsonArrayValue*  arr_ref = dynamic_cast<JsonArrayValue*>(m_parent);
        if (obj_ref != nullptr) {
            *obj_ref = JsonValue::make_array();
        } else if (arr_ref != nullptr) {
            *arr_ref = JsonValue::make_array();
        } else {
            *m_parent = JsonValue::make_array();
        }
    }
    JsonArray arr = m_parent->as_array();
    // 如果当前元素为空，则允许索引递增（append），即 index == size() 时 push_back；否则为已有空位则 set
    if (is_undefined() || is_null()) {
        // 空槽：仅允许索引递增（append），即 index == size() 时 push_back；否则为已有空位则 set
        if (m_index < arr.size()) {
            arr.set(m_index, value);
        } else if (m_index == arr.size()) {
            arr.push_back(value);
        } else {
            MC_THROW(mc::out_of_range_exception,
                     "JsonArrayValue: index must be <= size() when element is null");
        }
    } else {
        // 如果当前元素不为空，则设置为新的值
        arr.set(m_index, value);
    }
    // 设置当前元素为新的值
    JsonValue::operator=(value);
}

JsonArrayValue& JsonArrayValue::operator=(const JsonValue& value)
{
    set_value_impl(value);
    return *this;
}

JsonArrayValue& JsonArrayValue::operator=(int64_t value)
{
    set_value_impl(JsonValue(value));
    return *this;
}

JsonArrayValue& JsonArrayValue::operator=(bool value)
{
    set_value_impl(JsonValue(value));
    return *this;
}

JsonArrayValue& JsonArrayValue::operator=(double value)
{
    set_value_impl(JsonValue(value));
    return *this;
}

JsonArrayValue& JsonArrayValue::operator=(std::string_view value)
{
    set_value_impl(JsonValue(value));
    return *this;
}

JsonArrayValue& JsonArrayValue::operator=(const char* value)
{
    set_value_impl(JsonValue(value));
    return *this;
}

JsonArrayValue& JsonArrayValue::operator=(const std::string& value)
{
    set_value_impl(JsonValue(value));
    return *this;
}

JsonObjectValue JsonArrayValue::operator[](std::string_view key)
{
    if (is_undefined() || is_null()) {
        return JsonObjectValue(this, key);
    }
    if (!is_object()) {
        MC_THROW(mc::bad_cast_exception, "JSON type is not object");
    }
    return as_object().has(key) ? JsonObjectValue(this, key, as_object().get(key))
                                : JsonObjectValue(this, key);
}

// ======================== JsonObjectValue ========================

JsonObjectValue& JsonObjectValue::operator=(const JsonValue& value)
{
    // 如果父对象为空，则创建一个空对象；若父为 JsonArrayValue 则调用其 operator= 以写回数组元素
    if (m_parent->is_undefined() || m_parent->is_null()) {
        JsonArrayValue*  arr_ref = dynamic_cast<JsonArrayValue*>(m_parent);
        JsonObjectValue* obj_ref = dynamic_cast<JsonObjectValue*>(m_parent);
        if (arr_ref != nullptr) {
            *arr_ref = JsonValue::make_object();
        } else if (obj_ref != nullptr) {
            *obj_ref = JsonValue::make_object();
        } else {
            *m_parent = JsonValue::make_object();
        }
    }
    m_parent->as_object().set(m_key, value);
    JsonValue::operator=(value);
    return *this;
}

JsonObjectValue& JsonObjectValue::operator=(int64_t value)
{
    return *this = JsonValue(value);
}

JsonObjectValue& JsonObjectValue::operator=(bool value)
{
    return *this = JsonValue(value);
}

JsonObjectValue& JsonObjectValue::operator=(double value)
{
    return *this = JsonValue(value);
}

JsonObjectValue& JsonObjectValue::operator=(std::string_view value)
{
    return *this = JsonValue(value);
}

JsonObjectValue& JsonObjectValue::operator=(const char* value)
{
    return *this = JsonValue(value);
}

JsonObjectValue& JsonObjectValue::operator=(const std::string& value)
{
    return *this = JsonValue(value);
}

JsonObjectValue JsonObjectValue::operator[](std::string_view key)
{
    if (is_undefined() || is_null()) {
        return JsonObjectValue(this, key);
    }
    return as_object().has(key) ? JsonObjectValue(this, key, as_object().get(key))
                                : JsonObjectValue(this, key);
}

JsonArrayValue JsonObjectValue::operator[](uint32_t index)
{
    if (is_undefined() || is_null()) {
        return JsonArrayValue(this, index);
    }
    if (!is_array()) {
        MC_THROW(mc::bad_cast_exception, "JSON type is not array");
    }
    if (index > as_array().size()) {
        MC_THROW(mc::out_of_range_exception, "Index out of range");
    }
    if (index == as_array().size()) {
        return JsonArrayValue(this, index);
    }
    return JsonArrayValue(this, index, as_array().at(index));
}

// ======================== JsonArray ========================

JsonArray::JsonArray() noexcept
    : m_json(nullptr)
{
}

JsonArray::JsonArray(Json* json) noexcept
    : m_json(json)
{
    add_ref(m_json);
}

JsonArray::JsonArray(const JsonArray& other)
    : m_json(other.m_json)
{
    add_ref(m_json);
}

JsonArray& JsonArray::operator=(const JsonArray& other)
{
    if (this == &other) {
        return *this;
    }
    add_ref(other.m_json);
    release_ref(m_json);
    m_json = other.m_json;
    return *this;
}

JsonArray::JsonArray(JsonArray&& other) noexcept
    : m_json(other.m_json)
{
    other.m_json = nullptr;
}

JsonArray& JsonArray::operator=(JsonArray&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    release_ref(m_json);
    m_json       = other.m_json;
    other.m_json = nullptr;
    return *this;
}

JsonArray::~JsonArray()
{
    release_ref(m_json);
    m_json = nullptr;
}

uint32_t JsonArray::size() const
{
    if (m_json == nullptr) {
        return 0;
    }
    uint32_t sz  = 0;
    uint32_t ret = JsonArraySizeGet(m_json, &sz);
    check_json_ret(ret, "Failed to get array size");
    return sz;
}

JsonValue JsonArray::at(uint32_t index) const
{
    if (m_json == nullptr) {
        MC_THROW(mc::out_of_range_exception, "Access empty array");
    }
    Json*    item = nullptr;
    uint32_t ret  = JsonArrayItemGet(m_json, index, &item);
    check_json_ret(ret, "Failed to get array item");

    // 如果是 Quote 类型，需要获取实际对象
    if (item != nullptr && JsonIsQuote(item)) {
        Json* actual = nullptr;
        check_json_ret(JsonObjectQuoteGet(item, &actual), "Failed to get quote object");
        item = actual;
    }

    // 增加引用计数，确保返回的 JsonValue 持有有效引用
    add_ref(item);
    return JsonValue(item);
}

void JsonArray::set(uint32_t index, const JsonValue& value)
{
    if (m_json == nullptr) {
        MC_THROW(mc::out_of_range_exception, "Access empty array");
    }

    // 创建 Quote 对象用于替换
    Json*    quote = nullptr;
    uint32_t ret   = JsonQuoteCreate(&quote, value.get_raw());
    check_json_ret(ret, "Failed to create quote for array item");

    ret = JsonArrayItemReplace(m_json, index, quote);
    check_json_ret(ret, "Failed to set array item");
}

void JsonArray::push_back(const JsonValue& value, bool quote_flag)
{
    if (m_json == nullptr) {
        MC_THROW(mc::bad_cast_exception, "Array handle is empty");
    }
    if (quote_flag) {
        // 创建 Quote 对象用于插入
        Json*    quote = nullptr;
        uint32_t ret   = JsonQuoteCreate(&quote, value.get_raw());
        check_json_ret(ret, "Failed to create quote for array item");
        ret = JsonItemAddToArray(quote, m_json);
        check_json_ret(ret, "Failed to append array item");
    } else {
        JsonObjectAddRef(value.get_raw());
        uint32_t ret = JsonItemAddToArray(value.get_raw(), m_json);
        check_json_ret(ret, "Failed to append array item");
    }
}

// ======================== JsonObject ========================

JsonObject::JsonObject() noexcept
    : m_json(nullptr)
{
}

JsonObject::JsonObject(Json* json) noexcept
    : m_json(json)
{
    add_ref(m_json);
}

JsonObject::JsonObject(const JsonObject& other)
    : m_json(other.m_json)
{
    add_ref(m_json);
}

JsonObject& JsonObject::operator=(const JsonObject& other)
{
    if (this == &other) {
        return *this;
    }
    add_ref(other.m_json);
    release_ref(m_json);
    m_json = other.m_json;
    return *this;
}

JsonObject::JsonObject(JsonObject&& other) noexcept
    : m_json(other.m_json)
{
    other.m_json = nullptr;
}

JsonObject& JsonObject::operator=(JsonObject&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    release_ref(m_json);
    m_json       = other.m_json;
    other.m_json = nullptr;
    return *this;
}

JsonObject::~JsonObject()
{
    release_ref(m_json);
    m_json = nullptr;
}

bool JsonObject::has(std::string_view key) const
{
    if (m_json == nullptr) {
        return false;
    }
    Json*    item = nullptr;
    uint32_t ret  = JsonObjectItemGet(m_json, std::string(key).c_str(), &item);
    return (ret == JSON_OK) && (item != nullptr);
}

JsonValue JsonObject::get(std::string_view key) const
{
    if (m_json == nullptr) {
        MC_THROW(mc::bad_cast_exception, "Object handle is empty");
    }
    Json*    item = nullptr;
    uint32_t ret  = JsonObjectItemGet(m_json, std::string(key).c_str(), &item);
    check_json_ret(ret, "Failed to get object field");

    // 如果是 Quote 类型，需要获取实际对象
    if (item != nullptr && JsonIsQuote(item)) {
        Json* actual = nullptr;
        check_json_ret(JsonObjectQuoteGet(item, &actual), "Failed to get quote object");
        item = actual;
    }

    // 增加引用计数，确保返回的 JsonValue 持有有效引用
    add_ref(item);
    return JsonValue(item);
}

void JsonObject::set(std::string_view key, const JsonValue& value, bool quote_flag)
{
    if (m_json == nullptr) {
        MC_THROW(mc::bad_cast_exception, "Object handle is empty");
    }

    if (quote_flag) {
        // 创建 Quote 对象用于插入
        Json*    quote = nullptr;
        uint32_t ret   = JsonQuoteCreate(&quote, value.get_raw());
        check_json_ret(ret, "Failed to create quote for object field");

        std::string key_str(key);
        ret = JsonObjectItemSet(m_json, key_str.c_str(), quote);
        check_json_ret(ret, "Failed to set object field");
    } else {
        JsonObjectAddRef(value.get_raw());
        uint32_t ret = JsonObjectItemSet(m_json, key.data(), value.get_raw());
        check_json_ret(ret, "Failed to set object field");
    }
}

void JsonObject::erase(std::string_view key)
{
    if (m_json == nullptr) {
        return;
    }
    std::string key_str(key);
    (void)JsonObjectItemDelete(m_json, key_str.c_str());
}

uint32_t JsonObject::size() const
{
    if (m_json == nullptr) {
        return 0;
    }
    uint32_t count = 0;
    Json*    child = nullptr;
    uint32_t ret   = JsonItemFirstChild(const_cast<Json*>(m_json), &child);
    if (ret == JSON_NO_FIRST_CHILD) {
        return 0;
    }
    check_json_ret(ret, "Failed to get first child");
    while (child != nullptr) {
        ++count;
        Json*    next = nullptr;
        uint32_t r    = JsonItemNextSibling(child, &next);
        if (r == JSON_NO_NEXT_SIBLING) {
            break;
        }
        check_json_ret(r, "Failed to get next child");
        child = next;
    }
    return count;
}

// ======================== JsonObject::iterator ========================

JsonObject::iterator::iterator(Json* json, Json* child)
    : m_json(json), m_child(child)
{
}

JsonObject::key_value_pair JsonObject::iterator::operator*() const
{
    if (m_child == nullptr) {
        MC_THROW(mc::out_of_range_exception, "Dereference invalid iterator");
    }
    char* key_c = nullptr;
    check_json_ret(JsonItemKeyGet(m_child, &key_c), "Failed to get object key");
    std::string key = key_c ? std::string(key_c) : std::string();

    // 检查是否为 Quote 类型
    Json* value_ptr = m_child;
    if (JsonIsQuote(m_child)) {
        Json* actual = nullptr;
        check_json_ret(JsonObjectQuoteGet(m_child, &actual), "Failed to get quote object from iterator");
        value_ptr = actual;
    }

    // 增加引用计数，确保返回的 JsonValue 持有有效引用
    add_ref(value_ptr);
    return {key, JsonValue(value_ptr)};
}

JsonObject::iterator& JsonObject::iterator::operator++()
{
    if (m_child != nullptr) {
        Json*    next = nullptr;
        uint32_t ret  = JsonItemNextSibling(m_child, &next);
        if (ret == JSON_NO_NEXT_SIBLING) {
            m_child = nullptr;
        } else {
            check_json_ret(ret, "Failed to get next sibling");
            m_child = next;
        }
    }
    return *this;
}

JsonObject::iterator JsonObject::iterator::operator++(int)
{
    iterator tmp = *this;
    ++(*this);
    return tmp;
}

bool JsonObject::iterator::operator==(const iterator& other) const
{
    return m_json == other.m_json && m_child == other.m_child;
}

JsonObject::iterator JsonObject::begin() const
{
    if (m_json == nullptr) {
        return iterator(nullptr, nullptr);
    }
    Json*    child = nullptr;
    uint32_t ret   = JsonItemFirstChild(const_cast<Json*>(m_json), &child);
    if (ret == JSON_NO_FIRST_CHILD) {
        return iterator(m_json, nullptr);
    }
    check_json_ret(ret, "Failed to get first child");
    return iterator(m_json, child);
}

JsonObject::iterator JsonObject::end() const
{
    return iterator(m_json, nullptr);
}

// ======================== json_encode ========================
std::string json_encode(const mc::variant& value, bool pretty_print)
{
    Json* json_ptr = nullptr;
    char* result   = nullptr;
    try {
        // 将 variant 转换为底层 JSON 对象
        json_ptr = build_json_from_variant(value);

        if (json_ptr == nullptr) {
            MC_THROW(mc::parse_error_exception, "Failed to convert variant to JSON");
        }

        if (pretty_print) {
            result = JsonPrint(json_ptr);
        } else {
            result = JsonPrintWithoutFormat(json_ptr);
        }

        if (result == nullptr) {
            JsonObjectRelease(json_ptr);
            json_ptr = nullptr;
            MC_THROW(mc::parse_error_exception, "Failed to serialize JSON to string");
        }

        // 拷贝结果并释放内存
        std::string output(result);
        (void)JsonStringValueFree(result);
        JsonObjectRelease(json_ptr);
        json_ptr = nullptr;

        return output;
    } catch (const mc::parse_error_exception&) {
        if (json_ptr != nullptr) {
            JsonObjectRelease(json_ptr);
            json_ptr = nullptr;
        }
        if (result != nullptr) {
            (void)JsonStringValueFree(result);
            result = nullptr;
        }
        throw;
    } catch (const std::exception& e) {
        if (json_ptr != nullptr) {
            JsonObjectRelease(json_ptr);
            json_ptr = nullptr;
        }
        if (result != nullptr) {
            (void)JsonStringValueFree(result);
            result = nullptr;
        }
        MC_THROW(mc::parse_error_exception, "JSON encoding failed: ${error}", ("error", e.what()));
    }
}

std::string json_encode(const mc::dict& obj, bool pretty_print)
{
    Json* json_obj = nullptr;
    char* result   = nullptr;
    try {
        uint32_t ret = JsonObjectCreate(&json_obj);
        check_json_ret(ret, "Failed to create JSON object");

        // 遍历 dict，添加键值对
        for (const auto& entry : obj) {
            std::string key_str;
            if (!entry.key.is_string()) {
                key_str = entry.key.as_string();
            } else {
                key_str = entry.key.get_string();
            }
            Json* child = build_json_from_variant(entry.value);
            ensure_created(child, "Failed to build object value");
            check_json_ret(JsonItemAddToObject(key_str.c_str(), child, json_obj),
                           "Failed to add object element");
        }

        if (pretty_print) {
            result = JsonPrint(json_obj);
        } else {
            result = JsonPrintWithoutFormat(json_obj);
        }

        if (result == nullptr) {
            JsonObjectRelease(json_obj);
            json_obj = nullptr;
            MC_THROW(mc::parse_error_exception, "Failed to serialize JSON");
        }

        std::string output(result);
        (void)JsonStringValueFree(result);
        JsonObjectRelease(json_obj);
        json_obj = nullptr;

        return output;
    } catch (const mc::parse_error_exception&) {
        if (json_obj != nullptr) {
            JsonObjectRelease(json_obj);
            json_obj = nullptr;
        }
        if (result != nullptr) {
            (void)JsonStringValueFree(result);
            result = nullptr;
        }
        throw;
    } catch (const std::exception& e) {
        if (json_obj != nullptr) {
            JsonObjectRelease(json_obj);
            json_obj = nullptr;
        }
        if (result != nullptr) {
            (void)JsonStringValueFree(result);
            result = nullptr;
        }
        MC_THROW(mc::parse_error_exception, "JSON encoding failed: ${error}", ("error", e.what()));
    }
}

std::string json_encode(const std::vector<mc::variant>& arr, bool pretty_print)
{
    Json* json_arr = nullptr;
    char* result   = nullptr;
    try {
        uint32_t ret = JsonArrayCreate(&json_arr);
        check_json_ret(ret, "Failed to create JSON array");

        // 遍历 vector，添加元素
        for (const auto& item : arr) {
            Json* child = build_json_from_variant(item);
            ensure_created(child, "Failed to build array element");
            check_json_ret(JsonItemAddToArray(child, json_arr),
                           "Failed to add array element");
        }

        if (pretty_print) {
            result = JsonPrint(json_arr);
        } else {
            result = JsonPrintWithoutFormat(json_arr);
        }

        if (result == nullptr) {
            JsonObjectRelease(json_arr);
            json_arr = nullptr;
            MC_THROW(mc::parse_error_exception, "Failed to serialize JSON");
        }

        std::string output(result);
        (void)JsonStringValueFree(result);
        JsonObjectRelease(json_arr);
        json_arr = nullptr;

        return output;
    } catch (const mc::parse_error_exception&) {
        if (json_arr != nullptr) {
            JsonObjectRelease(json_arr);
            json_arr = nullptr;
        }
        if (result != nullptr) {
            (void)JsonStringValueFree(result);
            json_arr = nullptr;
        }
        throw;
    } catch (const std::exception& e) {
        if (json_arr != nullptr) {
            JsonObjectRelease(json_arr);
            json_arr = nullptr;
        }
        if (result != nullptr) {
            (void)JsonStringValueFree(result);
            json_arr = nullptr;
        }
        MC_THROW(mc::parse_error_exception, "JSON encoding failed: ${error}", ("error", e.what()));
    }
}

std::string json_encode(const JsonValue& json_val, bool pretty_print)
{
    Json* json_ptr = nullptr;
    char* result   = nullptr;
    try {
        json_ptr = json_val.get_raw();

        if (json_ptr == nullptr) {
            MC_THROW(mc::parse_error_exception, "JsonValue contains null Json* pointer");
        }

        if (pretty_print) {
            result = JsonPrint(json_ptr);
        } else {
            result = JsonPrintWithoutFormat(json_ptr);
        }

        if (result == nullptr) {
            JsonObjectRelease(json_ptr);
            json_ptr = nullptr;
            MC_THROW(mc::parse_error_exception, "Failed to serialize JSON");
        }

        // 拷贝结果并释放内存
        std::string output(result);
        (void)JsonStringValueFree(result);

        return output;
    } catch (const mc::parse_error_exception&) {
        if (json_ptr != nullptr) {
            JsonObjectRelease(json_ptr);
            json_ptr = nullptr;
        }
        if (result != nullptr) {
            (void)JsonStringValueFree(result);
            result = nullptr;
        }
        throw;
    } catch (const std::exception& e) {
        if (json_ptr != nullptr) {
            JsonObjectRelease(json_ptr);
            json_ptr = nullptr;
        }
        if (result != nullptr) {
            (void)JsonStringValueFree(result);
            result = nullptr;
        }
        MC_THROW(mc::parse_error_exception, "JSON encoding failed: ${error}", ("error", e.what()));
    }
}

// ======================== json_decode ========================
mc::variant json_decode(std::string_view json)
{
    Json* json_obj = nullptr;
    try {
        // 创建错误信息对象
        JsonErrorInfo* error_info = JsonErrorInfoCreate();
        if (error_info == nullptr) {
            MC_THROW(mc::parse_error_exception, "Failed to create JSON error info");
        }

        uint32_t ret = JsonParseMulti(json.data(), &json_obj, error_info);
        if (ret != JSON_OK || json_obj == nullptr) {
            uint32_t error_pos = JsonErrorPositionGetMulti(error_info);
            JsonErrorInfoDelete(error_info);
            MC_THROW(mc::parse_error_exception, "JSON parsing failed at position ${pos}", ("pos", error_pos));
        }

        // 释放错误信息对象
        JsonErrorInfoDelete(error_info);

        // 将解析结果转换为 variant
        mc::variant result = build_variant_from_json(json_obj);
        JsonObjectRelease(json_obj);
        json_obj = nullptr;

        return result;
    } catch (const mc::parse_error_exception&) {
        if (json_obj != nullptr) {
            JsonObjectRelease(json_obj);
            json_obj = nullptr;
        }
        throw;
    } catch (const std::exception& e) {
        if (json_obj != nullptr) {
            JsonObjectRelease(json_obj);
            json_obj = nullptr;
        }
        MC_THROW(mc::parse_error_exception, "JSON decoding failed: ${error}", ("error", e.what()));
    }
}

JsonValue json_decode_raw(const char* json)
{
    Json* json_obj = nullptr;
    try {
        if (json == nullptr) {
            MC_THROW(mc::parse_error_exception, "JSON string is null");
        }

        // 创建错误信息对象
        JsonErrorInfo* error_info = JsonErrorInfoCreate();
        if (error_info == nullptr) {
            MC_THROW(mc::parse_error_exception, "Failed to create JSON error info");
        }

        uint32_t ret = JsonParseMulti(json, &json_obj, error_info);
        if (ret != JSON_OK || json_obj == nullptr) {
            uint32_t error_pos = JsonErrorPositionGetMulti(error_info);
            JsonErrorInfoDelete(error_info);
            MC_THROW(mc::parse_error_exception, "JSON parsing failed at position ${pos}", ("pos", error_pos));
        }

        // 释放错误信息对象
        JsonErrorInfoDelete(error_info);

        return JsonValue(json_obj);
    } catch (const mc::parse_error_exception&) {
        if (json_obj != nullptr) {
            JsonObjectRelease(json_obj);
            json_obj = nullptr;
        }
        throw;
    } catch (const std::exception& e) {
        if (json_obj != nullptr) {
            JsonObjectRelease(json_obj);
            json_obj = nullptr;
        }
        MC_THROW(mc::parse_error_exception, "JSON decoding failed: ${error}", ("error", e.what()));
    }
}

// ======================== dump ========================
bool dump(const mc::variant& value, const mc::filesystem::path& file_path, bool pretty_print)
{
    try {
        std::string json_str = json_encode(value, pretty_print);
        return mc::filesystem::write_file(file_path, json_str);
    } catch (const std::exception&) {
        return false;
    }
}

bool dump(const mc::dict& obj, const mc::filesystem::path& file_path, bool pretty_print)
{
    try {
        std::string json_str = json_encode(obj, pretty_print);
        return mc::filesystem::write_file(file_path, json_str);
    } catch (const std::exception&) {
        return false;
    }
}

bool dump(const std::vector<mc::variant>& arr, const mc::filesystem::path& file_path, bool pretty_print)
{
    try {
        std::string json_str = json_encode(arr, pretty_print);
        return mc::filesystem::write_file(file_path, json_str);
    } catch (const std::exception&) {
        return false;
    }
}

bool dump(const JsonValue& json_val, const mc::filesystem::path& file_path, bool pretty_print)
{
    try {
        std::string json_str = json_encode(json_val, pretty_print);
        return mc::filesystem::write_file(file_path, json_str);
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace json_wrapper
} // namespace mc
