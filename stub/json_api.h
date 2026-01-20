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

#ifndef JSON_API_H
#define JSON_API_H

#include <cstdint>

// 前向声明 Json 结构体
typedef struct TagJsonChildList Json;

// 错误码常量
#define JSON_OK 0
#define JSON_NUMBER_TYPE_MISMATCH 1
#define JSON_NO_FIRST_CHILD 2
#define JSON_NO_NEXT_SIBLING 3

// JSON 类型枚举
typedef enum {
    JSONTYPE_NULL = 0,
    JSONTYPE_TRUE = 1,
    JSONTYPE_FALSE = 2,
    JSONTYPE_NUMBER = 3,
    JSONTYPE_STRING = 4,
    JSONTYPE_ARRAY = 5,
    JSONTYPE_OBJECT = 6,
    JSONTYPE_QUOTE = 7,
} JsonType;

// 创建函数
inline uint32_t JsonNullCreate(Json** json) {
    (void)json;
    return JSON_OK;
}

inline uint32_t JsonBoolCreate(bool value, Json** json) {
    (void)value;
    (void)json;
    return JSON_OK;
}

inline uint32_t JsonIntegerCreate(int64_t value, Json** json) {
    (void)value;
    (void)json;
    return JSON_OK;
}

inline uint32_t JsonDoubleCreate(double value, Json** json) {
    (void)value;
    (void)json;
    return JSON_OK;
}

inline uint32_t JsonStringCreateWithLen(const char* str, uint32_t len, Json** json) {
    (void)str;
    (void)len;
    (void)json;
    return JSON_OK;
}

inline uint32_t JsonArrayCreate(Json** json) {
    (void)json;
    return JSON_OK;
}

inline uint32_t JsonObjectCreate(Json** json) {
    (void)json;
    return JSON_OK;
}

inline uint32_t JsonQuoteCreate(Json** quote, Json* target) {
    (void)quote;
    (void)target;
    return JSON_OK;
}

// 获取类型函数
inline JsonType JsonTypeGet(const Json* json) {
    (void)json;
    return JSONTYPE_NULL;
}

// 类型检查函数
inline bool JsonIsNull(const Json* json) {
    (void)json;
    return false;
}

inline bool JsonIsBool(const Json* json) {
    (void)json;
    return false;
}

inline bool JsonIsInteger(const Json* json) {
    (void)json;
    return false;
}

inline bool JsonIsDouble(const Json* json) {
    (void)json;
    return false;
}

inline bool JsonIsNumber(const Json* json) {
    (void)json;
    return false;
}

inline bool JsonIsString(const Json* json) {
    (void)json;
    return false;
}

inline bool JsonIsArray(const Json* json) {
    (void)json;
    return false;
}

inline bool JsonIsObject(const Json* json) {
    (void)json;
    return false;
}

inline bool JsonIsQuote(const Json* json) {
    (void)json;
    return false;
}

// 获取值函数
inline uint32_t JsonItemBoolValueGet(const Json* json, bool* value) {
    (void)json;
    (void)value;
    return JSON_OK;
}

inline uint32_t JsonItemIntegerValueGet(Json* json, int64_t* value) {
    (void)json;
    (void)value;
    return JSON_OK;
}

inline uint32_t JsonItemDoubleValueGet(const Json* json, double* value) {
    (void)json;
    (void)value;
    return JSON_OK;
}

inline uint32_t JsonItemStringValueGet(const Json* json, char** value) {
    (void)json;
    (void)value;
    return JSON_OK;
}

// 数组操作函数
inline uint32_t JsonArraySizeGet(const Json* json, uint32_t* size) {
    (void)json;
    (void)size;
    return JSON_OK;
}

inline uint32_t JsonArrayItemGet(const Json* json, uint32_t index, Json** item) {
    (void)json;
    (void)index;
    (void)item;
    return JSON_OK;
}

inline uint32_t JsonItemAddToArray(Json* item, Json* array) {
    (void)item;
    (void)array;
    return JSON_OK;
}

inline uint32_t JsonArrayItemReplace(Json* array, uint32_t index, Json* item) {
    (void)array;
    (void)index;
    (void)item;
    return JSON_OK;
}

// 对象操作函数
inline uint32_t JsonObjectItemGet(const Json* json, const char* key, Json** item) {
    (void)json;
    (void)key;
    (void)item;
    return JSON_OK;
}

inline uint32_t JsonObjectItemSet(Json* json, const char* key, Json* item) {
    (void)json;
    (void)key;
    (void)item;
    return JSON_OK;
}

inline uint32_t JsonObjectItemDelete(Json* json, const char* key) {
    (void)json;
    (void)key;
    return JSON_OK;
}

inline uint32_t JsonItemAddToObject(const char* key, Json* item, Json* object) {
    (void)key;
    (void)item;
    (void)object;
    return JSON_OK;
}

// 树遍历函数
inline uint32_t JsonItemFirstChild(Json* json, Json** child) {
    (void)json;
    (void)child;
    return JSON_NO_FIRST_CHILD;
}

inline uint32_t JsonItemNextSibling(Json* json, Json** next) {
    (void)json;
    (void)next;
    return JSON_NO_NEXT_SIBLING;
}

inline uint32_t JsonItemKeyGet(Json* json, char** key) {
    (void)json;
    (void)key;
    return JSON_OK;
}

// Quote 操作函数
inline uint32_t JsonObjectQuoteGet(const Json* json, Json** target) {
    (void)json;
    (void)target;
    return JSON_OK;
}

// 引用计数函数
inline uint32_t JsonObjectAddRef(Json* json) {
    (void)json;
    return JSON_OK;
}

inline uint32_t JsonObjectRelease(Json* json) {
    (void)json;
    return JSON_OK;
}

#endif // JSON_API_H