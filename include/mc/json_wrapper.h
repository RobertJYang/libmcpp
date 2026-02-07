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

#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <mc/common.h>
#include <mc/filesystem.h>
#include <mc/variant.h>

// 使用正确的前向声明（匹配 json_api.h 中的定义）
struct TagJsonChildList;
typedef struct TagJsonChildList Json;
typedef struct TagJsonErrorInfo JsonErrorInfo;

namespace mc {
namespace json_wrapper {

class JsonArray;
class JsonObject;
class JsonArrayValue;
class JsonObjectValue;

/**
 * @brief JsonValue 类型枚举
 */
enum class JsonValueType {
    Undefined,
    Null,
    Bool,
    Integer,
    Double,
    String,
    Array,
    Object,
};

/**
 * @brief JSON 值包装类，负责管理底层 Json* 生命周期
 */
class MC_API JsonValue {
public:
    // 构造与赋值（RAII 管理 Json* 引用计数）
    JsonValue() noexcept;
    explicit JsonValue(Json* json) noexcept;
    JsonValue(const JsonValue& other);
    JsonValue& operator=(const JsonValue& other);
    JsonValue(JsonValue&& other) noexcept;
    JsonValue& operator=(JsonValue&& other) noexcept;
    virtual ~JsonValue();

    // 从整型、布尔、浮点、字符串、Array、Object 重新赋值（jv = 2; jv = "x"; 等）
    JsonValue& operator=(int64_t value);
    JsonValue& operator=(bool value);
    JsonValue& operator=(double value);
    JsonValue& operator=(std::string_view value);
    JsonValue& operator=(const std::string value);
    JsonValue& operator=(const JsonArray& value);
    JsonValue& operator=(const JsonObject& value);
    /** 将 std::vector<T> 转为 JsonArray 并赋值，T 支持：JsonValue、整型、浮点、布尔、字符串 */
    JsonValue& operator=(const std::vector<JsonValue>& value);
    JsonValue& operator=(const std::vector<int64_t>& value);
    JsonValue& operator=(const std::vector<double>& value);
    JsonValue& operator=(const std::vector<bool>& value);
    JsonValue& operator=(const std::vector<std::string>& value);
    JsonValue& operator=(const std::vector<std::string_view>& value);
    template <typename T,
              typename std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    JsonValue& operator=(const std::vector<T>& value);
    template <typename T,
              typename std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    JsonValue& operator=(T value) {
        return *this = static_cast<int64_t>(value);
    }

    JsonValue(bool value);
    JsonValue(int64_t value);
    JsonValue(double value);
    // 支持其他整型（unsigned、long、long long 等）通过委托到 int64_t 构造
    template <typename T,
              typename std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    JsonValue(T value)
        : JsonValue(static_cast<int64_t>(value)) {
    }
    JsonValue(std::string_view value);
    /** @brief 支持从 C 字符串直接构造，便于书写 JsonValue v = "test"; 或 {1, 2, "test"} */
    JsonValue(const char* value);
    JsonValue(const JsonArray& value);
    JsonValue(const JsonObject& value);
    /** 从 std::vector<T> 构造 JsonArray，T 支持：JsonValue、整型、浮点、布尔、字符串 */
    explicit JsonValue(const std::vector<JsonValue>& value);
    explicit JsonValue(const std::vector<int64_t>& value);
    explicit JsonValue(const std::vector<double>& value);
    explicit JsonValue(const std::vector<bool>& value);
    explicit JsonValue(const std::vector<std::string>& value);
    explicit JsonValue(const std::vector<std::string_view>& value);
    template <typename T,
              typename std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    explicit JsonValue(const std::vector<T>& value);

    // 工厂方法：创建不同类型的 JSON 节点
    static JsonValue make_null();
    static JsonValue make_bool(bool value);
    static JsonValue make_int(int64_t value);
    static JsonValue make_double(double value);
    static JsonValue make_string(std::string_view value);
    static JsonValue make_array();
    static JsonValue make_object();

    // 类型判断
    JsonValueType type() const;
    bool          is_undefined() const;
    bool          is_null() const;
    bool          is_bool() const;
    bool          is_int() const;
    bool          is_double() const;
    bool          is_number() const;
    bool          is_string() const;
    bool          is_array() const;
    bool          is_object() const;

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

    // 与整型、布尔、浮点、字符串的类型和值比较（类型须一致且值相等）
    bool operator==(int64_t value) const;
    bool operator!=(int64_t value) const {
        return !(*this == value);
    }
    bool operator==(bool value) const;
    bool operator!=(bool value) const {
        return !(*this == value);
    }
    bool operator==(double value) const;
    bool operator!=(double value) const {
        return !(*this == value);
    }
    bool operator==(std::string_view value) const;
    bool operator!=(std::string_view value) const {
        return !(*this == value);
    }
    bool operator==(const std::string value) const {
        return *this == std::string_view(value);
    }
    bool operator!=(const std::string value) const {
        return !(*this == value);
    }
    bool operator==(const char* value) const {
        return *this == std::string_view(value);
    }
    bool operator!=(const char* value) const {
        return !(*this == value);
    }
    // 其他整型委托到 int64_t 比较
    template <typename T,
              typename std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    bool operator==(T value) const {
        return *this == static_cast<int64_t>(value);
    }
    template <typename T,
              typename std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    bool operator!=(T value) const {
        return !(*this == value);
    }

    // 下标访问：Array 类型用 [index]，Object 类型用 [key]；其他类型抛出 bad_cast_exception
    // 仅提供 uint32_t 与 string_view，避免字面量 0 与 const char* 重载歧义
    JsonValue operator[](uint32_t index) const;
    JsonValue operator[](std::string_view key) const;
    // 下标赋值：Array 类型用 [index] = value，Object 类型用 [key] = value
    JsonArrayValue  operator[](uint32_t index);
    JsonObjectValue operator[](std::string_view key);
    // 底层句柄访问，仅供内部/测试使用
    Json* get_raw() const noexcept {
        return m_json;
    }

    // 从底层 Json* 指针创建 JsonValue（会增加引用计数）
    static JsonValue new_from_raw(Json* raw_ptr);

private:
    Json* m_json;
};

// 字面量在左侧时的相等比较（委托给 JsonValue 的成员 operator==）
MC_API bool operator==(int64_t lhs, const JsonValue& rhs);
MC_API bool operator!=(int64_t lhs, const JsonValue& rhs);
MC_API bool operator==(bool lhs, const JsonValue& rhs);
MC_API bool operator!=(bool lhs, const JsonValue& rhs);
MC_API bool operator==(double lhs, const JsonValue& rhs);
MC_API bool operator!=(double lhs, const JsonValue& rhs);
MC_API bool operator==(std::string_view lhs, const JsonValue& rhs);
MC_API bool operator!=(std::string_view lhs, const JsonValue& rhs);
MC_API bool operator==(const std::string lhs, const JsonValue& rhs);
MC_API bool operator!=(const std::string lhs, const JsonValue& rhs);
MC_API bool operator==(const char* lhs, const JsonValue& rhs);
MC_API bool operator!=(const char* lhs, const JsonValue& rhs);
template <typename T,
          typename std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
bool operator==(T lhs, const JsonValue& rhs) {
    return rhs == static_cast<int64_t>(lhs);
}
template <typename T,
          typename std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
bool operator!=(T lhs, const JsonValue& rhs) {
    return !(rhs == lhs);
}

/**
 * @brief JSON 数组视图，内部通过引用计数共享底层 Json*
 */
class MC_API JsonArray {
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
    void push_back(const JsonValue& value, bool quote_flag = true);

    // 迭代器支持（满足 std::iterator_traits，可与 std::find 等算法配合）
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = JsonValue;
        using difference_type   = std::ptrdiff_t;
        using pointer           = void;
        using reference         = JsonValue;

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
class MC_API JsonObject {
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

    void set(std::string_view key, const JsonValue& value, bool quote_flag = true);
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

    // 迭代器支持（满足 std::iterator_traits，可与 std::find_if 等算法配合）
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = key_value_pair;
        using difference_type   = std::ptrdiff_t;
        using pointer           = void;
        using reference         = key_value_pair;

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

/**
 * @brief 继承自 JsonValue，表示通过 JsonValue 数组下标得到的可写引用，支持 arr[i] = value 赋值。
 *        赋值类型仅允许 int64_t、bool、double、string_view、JsonValue。
 *        为空时仅允许索引递增（append）：仅当 index == size() 时可赋值，否则抛异常。
 */
class MC_API JsonArrayValue : public JsonValue {
public:
    /** 构造：父 Array（JsonValue）、索引、可选的当前元素值（默认空） */
    JsonArrayValue(JsonValue* parent_array, uint32_t index, JsonValue value = JsonValue())
        : JsonValue(value), m_parent(parent_array), m_index(index) {
    }

    JsonArrayValue& operator=(const JsonValue& value);
    JsonArrayValue& operator=(int64_t value);
    JsonArrayValue& operator=(bool value);
    JsonArrayValue& operator=(double value);
    JsonArrayValue& operator=(std::string_view value);
    JsonArrayValue& operator=(const char* value);
    JsonArrayValue& operator=(const std::string& value);
    template <typename T,
              typename std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    JsonArrayValue& operator=(T value) {
        return *this = static_cast<int64_t>(value);
    }

    /** 链式创建：若当前元素为空或非 Object，则先设为空对象，再返回其键引用，支持 jv[i][\"key\"] = value */
    JsonObjectValue operator[](std::string_view key);

private:
    void set_value_impl(const JsonValue& value);

    JsonValue* m_parent;
    uint32_t   m_index;
};

/**
 * @brief 继承自 JsonValue，表示通过 JsonValue 对象下标得到的可写引用，支持 obj["key"] = value 赋值。
 *        赋值类型仅允许 int64_t、bool、double、string_view、JsonValue。
 */
class MC_API JsonObjectValue : public JsonValue {
public:
    /** 构造：父 Object（JsonValue）、键、可选的当前元素值（默认空） */
    JsonObjectValue(JsonValue* parent_object, std::string_view key, JsonValue value = JsonValue())
        : JsonValue(value), m_parent(parent_object), m_key(key) {
    }

    JsonObjectValue& operator=(const JsonValue& value);
    JsonObjectValue& operator=(int64_t value);
    JsonObjectValue& operator=(bool value);
    JsonObjectValue& operator=(double value);
    JsonObjectValue& operator=(std::string_view value);
    JsonObjectValue& operator=(const char* value);
    JsonObjectValue& operator=(const std::string& value);
    template <typename T,
              typename std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    JsonObjectValue& operator=(T value) {
        return *this = static_cast<int64_t>(value);
    }

    /** 链式创建：若当前元素为空或非 Object，则先设为空对象，再返回其键引用 */
    JsonObjectValue operator[](std::string_view key);
    /** 链式创建：若当前元素为空或非 Array，则先设为空数组，再返回其下标引用 */
    JsonArrayValue operator[](uint32_t index);

private:
    JsonValue*  m_parent;
    std::string m_key;
};

/**
 * @brief 将variant编码为JSON字符串
 *
 * @param value 要编码的variant对象
 * @param pretty_print 是否格式化输出（带缩进和换行），默认为 false
 * @return std::string 编码后的JSON字符串
 * @throw mc::parse_error_exception 当编码失败时抛出异常
 */
MC_API std::string json_encode(const mc::variant& value, bool pretty_print = false);

/**
 * @brief 将dict编码为JSON字符串
 *
 * @param obj 要编码的dict对象
 * @param pretty_print 是否格式化输出（带缩进和换行），默认为 false
 * @return std::string 编码后的JSON字符串
 * @throw mc::parse_error_exception 当编码失败时抛出异常
 */
MC_API std::string json_encode(const mc::dict& obj, bool pretty_print = false);

/**
 * @brief 将vector<variant>编码为JSON字符串
 *
 * @param arr 要编码的数组对象
 * @param pretty_print 是否格式化输出（带缩进和换行），默认为 false
 * @return std::string 编码后的JSON字符串
 * @throw mc::parse_error_exception 当编码失败时抛出异常
 */
MC_API std::string json_encode(const std::vector<mc::variant>& arr, bool pretty_print = false);

/**
 * @brief 将JsonValue对象编码为JSON字符串
 *
 * 此函数直接对底层的Json*对象进行序列化，不进行类型转换。
 * 适用于已经持有JsonValue对象的场景。
 *
 * @param json_val 要编码的JsonValue对象
 * @param pretty_print 是否格式化输出（带缩进和换行），默认为 false
 * @return std::string 编码后的JSON字符串
 * @throw mc::parse_error_exception 当编码失败时抛出异常
 */
MC_API std::string json_encode(const JsonValue& json_val, bool pretty_print = false);

/**
 * @brief 从JSON字符串解码为variant对象
 *
 * @param json JSON字符串视图
 * @return mc::variant 解码后的variant对象
 * @throw mc::parse_error_exception 当解码失败时抛出异常
 */
MC_API mc::variant json_decode(std::string_view json);

/**
 * @brief 从JSON字符串解码为JsonValue对象（const char* 重载）
 *
 * 此函数直接返回JsonValue对象，不进行variant转换。
 * 适用于需要直接操作JSON对象的场景。
 *
 * @param json JSON字符串（C 风格字符串指针）
 * @return JsonValue 解码后的JsonValue对象（RAII 自动管理引用计数）
 * @throw mc::parse_error_exception 当解码失败时抛出异常
 *
 * @note 返回的JsonValue对象自动管理内部Json*的引用计数，无需手动释放
 */
MC_API JsonValue json_decode_raw(const char* json);

/**
 * @brief 将variant编码为JSON字符串并写入指定文件
 *
 * @param value 要编码的variant对象
 * @param file_path 目标文件路径
 * @param pretty_print 是否格式化输出（带缩进和换行），默认为 false
 * @return bool 成功返回true，失败返回false
 */
MC_API bool dump(const mc::variant& value, const mc::filesystem::path& file_path, bool pretty_print = false);

/**
 * @brief 将dict编码为JSON字符串并写入指定文件
 *
 * @param obj 要编码的dict对象
 * @param file_path 目标文件路径
 * @param pretty_print 是否格式化输出（带缩进和换行），默认为 false
 * @return bool 成功返回true，失败返回false
 */
MC_API bool dump(const mc::dict& obj, const mc::filesystem::path& file_path, bool pretty_print = false);

/**
 * @brief 将vector<variant>编码为JSON字符串并写入指定文件
 *
 * @param arr 要编码的数组对象
 * @param file_path 目标文件路径
 * @param pretty_print 是否格式化输出（带缩进和换行），默认为 false
 * @return bool 成功返回true，失败返回false
 */
MC_API bool dump(const std::vector<mc::variant>& arr, const mc::filesystem::path& file_path, bool pretty_print = false);

/**
 * @brief 将JsonValue对象编码为JSON字符串并写入指定文件
 *
 * @param json_val 要编码的JsonValue对象
 * @param file_path 目标文件路径
 * @param pretty_print 是否格式化输出（带缩进和换行），默认为 false
 * @return bool 成功返回true，失败返回false
 *
 * @note 此函数直接对JsonValue对象进行序列化，不进行类型转换
 */
MC_API bool dump(const JsonValue& json_val, const mc::filesystem::path& file_path, bool pretty_print = false);

} // namespace json_wrapper
} // namespace mc

#endif // MC_JSON_WRAPPER_H