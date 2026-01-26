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
    void push_back(const JsonValue& value, bool quote_flag = true);

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

/**
 * @brief 将variant编码为JSON字符串
 *
 * @param value 要编码的variant对象
 * @param pretty_print 是否格式化输出（带缩进和换行），默认为 false
 * @return std::string 编码后的JSON字符串
 * @throw mc::parse_error_exception 当编码失败时抛出异常
 */
std::string json_encode(const mc::variant& value, bool pretty_print = false);

/**
 * @brief 将dict编码为JSON字符串
 *
 * @param obj 要编码的dict对象
 * @param pretty_print 是否格式化输出（带缩进和换行），默认为 false
 * @return std::string 编码后的JSON字符串
 * @throw mc::parse_error_exception 当编码失败时抛出异常
 */
std::string json_encode(const mc::dict& obj, bool pretty_print = false);

/**
 * @brief 将vector<variant>编码为JSON字符串
 *
 * @param arr 要编码的数组对象
 * @param pretty_print 是否格式化输出（带缩进和换行），默认为 false
 * @return std::string 编码后的JSON字符串
 * @throw mc::parse_error_exception 当编码失败时抛出异常
 */
std::string json_encode(const std::vector<mc::variant>& arr, bool pretty_print = false);

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
mc::variant json_decode(std::string_view json);

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
bool dump(const mc::variant& value, const mc::filesystem::path& file_path, bool pretty_print = false);

/**
 * @brief 将dict编码为JSON字符串并写入指定文件
 *
 * @param obj 要编码的dict对象
 * @param file_path 目标文件路径
 * @param pretty_print 是否格式化输出（带缩进和换行），默认为 false
 * @return bool 成功返回true，失败返回false
 */
bool dump(const mc::dict& obj, const mc::filesystem::path& file_path, bool pretty_print = false);

/**
 * @brief 将vector<variant>编码为JSON字符串并写入指定文件
 *
 * @param arr 要编码的数组对象
 * @param file_path 目标文件路径
 * @param pretty_print 是否格式化输出（带缩进和换行），默认为 false
 * @return bool 成功返回true，失败返回false
 */
bool dump(const std::vector<mc::variant>& arr, const mc::filesystem::path& file_path, bool pretty_print = false);

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