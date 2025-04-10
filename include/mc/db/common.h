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

#ifndef MC_DATABASE_COMMON_H
#define MC_DATABASE_COMMON_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace mc::db {

/**
 * 数据库对象ID类型
 */
using object_id_t = uint64_t;

/**
 * 数据库事务ID类型
 */
using txn_id_t = uint64_t;

/**
 * 空对象ID
 */
constexpr object_id_t NULL_OBJECT_ID = 0;

/**
 * 表类型枚举
 */
enum class table_type : uint8_t { unknown = 0, memory = 1, persisten = 2 };

/**
 * 数据库异常基类
 */
class database_exception : public std::exception {
public:
    explicit database_exception(const std::string& message) : m_message(message) {
    }
    const char* what() const noexcept override {
        return m_message.c_str();
    }

private:
    std::string m_message;
};

/**
 * 键不存在异常
 */
class key_not_found_exception : public database_exception {
public:
    explicit key_not_found_exception(const std::string& key)
        : database_exception("键不存在: " + key) {
    }
};

/**
 * 键重复异常
 */
class key_duplicate_exception : public database_exception {
public:
    explicit key_duplicate_exception(const std::string& key)
        : database_exception("键重复: " + key) {
    }
};

/**
 * 无效参数异常
 */
class invalid_argument_exception : public database_exception {
public:
    explicit invalid_argument_exception(const std::string& message)
        : database_exception("无效参数: " + message) {
    }
};

} // namespace mc::db

#endif // MC_DATABASE_COMMON_H