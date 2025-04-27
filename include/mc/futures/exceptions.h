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

#ifndef MC_FUTURES_EXCEPTIONS_H
#define MC_FUTURES_EXCEPTIONS_H

#include <stdexcept>

namespace mc::futures {

class future_exception : public std::logic_error {
public:
    future_exception() : std::logic_error{""} {
    }
};

class timeout_error : public future_exception {
public:
    timeout_error() = default;
    const char* what() const noexcept override {
        return "Operation timed out";
    }
};

class future_already_retrieved : public future_exception {
public:
    future_already_retrieved() : future_exception() {
    }
    const char* what() const noexcept override {
        return "Future already retrieved";
    }
};

class promise_already_satisfied : public future_exception {
public:
    promise_already_satisfied() : future_exception() {
    }
    const char* what() const noexcept override {
        return "Promise value already set";
    }
};

} // namespace mc::futures

#endif // MC_FUTURES_EXCEPTIONS_H