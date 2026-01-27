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
 * @file json_wrapper.h
 * @brief 基于白泽 C JSON 库的 C++ 封装
 */
#ifndef MC_JSON_WRAPPER_H
#define MC_JSON_WRAPPER_H

#include <string>
#include <string_view>

#include <mc/json.h>
#include <mc/variant.h>

// 使用正确的前向声明（匹配 json_api.h 中的定义）
struct TagJsonChildList;
typedef struct TagJsonChildList Json;
typedef struct TagJsonErrorInfo JsonErrorInfo;

namespace mc {
namespace json_wrapper {

class JsonArray;
class JsonObject;

/**
 * @brief JSON 值包装类，负责管理底层 Json* 生命周期
 */
class JsonValue {
public:
    // 构造与赋值（RAII 管理 Json* 引用计数）
    JsonValue() noexcept;
    explicit JsonValue(Json* json) noexcept;
    JsonValue(const JsonValue& other);
    JsonValue& operator=(const JsonValue& other);
    JsonValue(JsonValue&& other) noexcept;
    JsonValue& operator=(JsonValue&& other) noexcept;
    ~JsonValue();

    // 工厂方法：创建不同类型的 JSON 节点
    static JsonValue make_null();
    static JsonValue make_bool(bool value);
    static JsonValue make_int(int64_t value);
    static JsonValue make_double(double value);
    static JsonValue make_string(std::string_view value);
    static JsonValue make_array();
    static JsonValue make_object();

    // 类型判断
    bool is_null() const;
    bool is_bool() const;
    bool is_int() const;
    bool is_double() const;
    bool is_number() const;
    bool is_string() const;
    bool is_array() const;
    bool is_object() const;

    // 值读取（类型不匹配时抛出异常）
    bool        as_bool() const;
    int64_t     as_int() const;
    double      as_double() const;
    std::string as_string() const;
    JsonArray   as_array() const;
    JsonObject  as_object() const;

    // 与 mc::variant 相互转换
    mc::variant      to_variant() const;
    static JsonValue from_variant(const mc::variant& value);

    // 相等比较（按 JSON 语义比较内容）
    bool operator==(const JsonValue& other) const;
    bool operator!=(const JsonValue& other) const {
        return !(*this == other);
    }

    // 底层句柄访问，仅供内部/测试使用
    Json* get_raw() const noexcept {
        return m_json;
    }

    // 从底层 Json* 指针创建 JsonValue（会增加引用计数）
    static JsonValue new_from_raw(Json* raw_ptr);

private:
    Json* m_json;
};

/**
 * @brief JSON 数组视图，内部通过引用计数共享底层 Json*
 */
class JsonArray {
public:
    JsonArray() noexcept;
    explicit JsonArray(Json* json) noexcept;
    JsonArray(const JsonArray& other);
    JsonArray& operator=(const JsonArray& other);
    JsonArray(JsonArray&& other) noexcept;
    JsonArray& operator=(JsonArray&& other) noexcept;
    ~JsonArray();

    uint32_t size() const;
    bool     empty() const {
        return size() == 0;
    }
    JsonValue at(uint32_t index) const;
    JsonValue operator[](uint32_t index) const {
        return at(index);
    }

    void set(uint32_t index, const JsonValue& value);
    void push_back(const JsonValue& value);

    // 迭代器支持
    class iterator {
    public:
        iterator(const JsonArray* array, uint32_t index)
            : m_array(array), m_index(index) {
        }

        JsonValue operator*() const {
            return m_array->at(m_index);
        }
        iterator& operator++() {
            ++m_index;
            return *this;
        }
        iterator operator++(int) {
            iterator tmp = *this;
            ++m_index;
            return tmp;
        }
        bool operator==(const iterator& other) const {
            return m_array == other.m_array && m_index == other.m_index;
        }
        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

    private:
        const JsonArray* m_array;
        uint32_t         m_index;
    };

    iterator begin() const {
        return iterator(this, 0);
    }
    iterator end() const {
        return iterator(this, size());
    }

    Json* get_raw() const noexcept {
        return m_json;
    }

private:
    Json* m_json;
};

/**
 * @brief JSON 对象视图，内部通过引用计数共享底层 Json*
 */
class JsonObject {
public:
    JsonObject() noexcept;
    explicit JsonObject(Json* json) noexcept;
    JsonObject(const JsonObject& other);
    JsonObject& operator=(const JsonObject& other);
    JsonObject(JsonObject&& other) noexcept;
    JsonObject& operator=(JsonObject&& other) noexcept;
    ~JsonObject();

    bool      has(std::string_view key) const;
    JsonValue get(std::string_view key) const;
    JsonValue operator[](std::string_view key) const {
        return get(key);
    }

    void set(std::string_view key, const JsonValue& value);
    void erase(std::string_view key);

    // 获取对象中键值对数量（通过遍历计算）
    uint32_t size() const;
    bool     empty() const {
        return size() == 0;
    }

    // 键值对结构
    struct key_value_pair {
        std::string key;
        JsonValue   value;
    };

    // 迭代器支持
    class iterator {
    public:
        iterator(Json* json, Json* child);

        key_value_pair operator*() const;
        iterator&      operator++();
        iterator       operator++(int);
        bool           operator==(const iterator& other) const;
        bool           operator!=(const iterator& other) const {
            return !(*this == other);
        }

    private:
        Json* m_json;
        Json* m_child;
    };

    iterator begin() const;
    iterator end() const;

    Json* get_raw() const noexcept {
        return m_json;
    }

private:
    Json* m_json;
};

} // namespace json_wrapper
} // namespace mc

#endif // MC_JSON_WRAPPER_H