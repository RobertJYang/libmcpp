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
#include "mdb_validator.h"
#include <climits>
#include <cstdint>
#include <mc/dbus/message.h>
#include <sstream>
#include <stdexcept>

[[noreturn]]
void TypeError(const std::string& msg) {
    throw std::runtime_error(msg);
}

void mdb_validator::check(
    const std::string& name,
    const std::string& signature,
    const mc::variant& value) {
    try {
        check_type(signature, value);
    } catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "Invalid value for '" << name << "':" << e.what();
        throw std::runtime_error(oss.str());
    }
}

void mdb_validator::check_method_args(const std::string& method_name, const std::string& signature, const mc::variant& args) {
    if (!args.is_array()) {
        TypeError("method arguments must be array");
    }
    auto        sigs = mc::dbus::signature(signature).get_complete_types();
    const auto& arr  = args.get_array();

    if (arr.size() != sigs.size()) {
        TypeError("method argument count mismatch");
    }

    for (size_t i = 0; i < sigs.size(); ++i) {
        check_type(sigs[i].str(), arr[i]);
    }
}

void mdb_validator::check_type(const std::string& signature, const mc::variant& value) {
    auto sigs = mc::dbus::signature(signature).get_complete_types();

    if (sigs.size() == 1) {
        check_one(signature, value);
        return;
    }

    if (!value.is_array()) {
        TypeError("expect array for multi-element signature");
    }

    const auto& arr = value.get_array();
    if (arr.size() != sigs.size()) {
        TypeError("struct size mismatch");
    }

    for (size_t i = 0; i < sigs.size(); ++i) {
        check_type(sigs[i].str(), arr[i]);
    }
}

void mdb_validator::check_one(const std::string& signature, const mc::variant& value) {
    if (signature == "ay") {
        if (value.is_blob() || value.is_array()) {
            return;
        }
        TypeError("invalida ay value");
    }

    if (is_basic_type(signature)) {
        check_basic(signature, value);
    } else if (is_dict_type(signature)) {
        check_dict(signature, value);
    } else if (is_struct_type(signature)) {
        check_struct(signature, value);
    } else {
        if (signature[0] == 'a') {
            check_array(signature.substr(1), value);
        } else {
            TypeError("unsupported signature");
        }
    }
}

void mdb_validator::check_basic(const std::string& sig, const mc::variant& value) {
    if (is_integer_type(sig)) {
        check_integer_range(sig, value);
        return;
    }

    if (is_string_type(sig)) {
        if (!value.is_string()) {
            TypeError("expect string");
        }
        check_utf8(value.get_string());
        return;
    }

    if (sig == "b" && value.is_bool()) {
        return;
    }
    if (sig == "d" && value.is_double()) {
        return;
    }

    TypeError("basic type mismatch");
}

void mdb_validator::check_array(const std::string& elemsig, const mc::variant& value) {
    if (!value.is_array()) {
        TypeError("expect array");
    }

    for (const auto& v : value.get_array()) {
        check_type(elemsig, v);
    }
}

void mdb_validator::check_struct(const std::string& signature, const mc::variant& value) {
    if (!value.is_array()) {
        TypeError("expect struct(array)");
    }

    std::string inner = signature.substr(1, signature.size() - 2);
    auto        sigs  = mc::dbus::signature(inner).get_complete_types();

    const auto& arr = value.get_array();
    if (arr.size() != sigs.size()) {
        TypeError("struct element count mismatch");
    }

    for (size_t i = 0; i < sigs.size(); ++i) {
        check_type(sigs[i].str(), arr[i]);
    }
}

void mdb_validator::check_dict(const std::string& signature, const mc::variant& value) {
    if (!value.is_dict()) {
        TypeError("expect dict");
    }

    char        key_sig = signature[2];
    std::string val_sig = signature.substr(3, signature.size() - 4);

    for (const auto& entry : value.get_object()) {
        check_basic(std::string(1, key_sig), entry.key);
        check_type(val_sig, entry.value);
    }
}

bool mdb_validator::is_basic_type(const std::string& sig) {
    return sig.size() == 1;
}

bool mdb_validator::is_integer_type(const std::string& sig) {
    return sig == "y" || sig == "q" || sig == "u" || sig == "t" ||
           sig == "n" || sig == "i" || sig == "x";
}

bool mdb_validator::is_string_type(const std::string& sig) {
    return sig == "s";
}

bool mdb_validator::is_dict_type(const std::string& sig) {
    return sig.size() > 3 && sig[0] == 'a' && sig[1] == '{';
}

bool mdb_validator::is_struct_type(const std::string& sig) {
    return sig.size() >= 2 && sig.front() == '(' && sig.back() == ')';
}

void mdb_validator::check_integer_range(const std::string& sig, const mc::variant& value) {
    if (!value.is_integer()) {
        TypeError("expect integer");
    }

    // 获取整数值（支持有符号和无符号）
    int64_t int_val = 0;
    if (value.is_signed_integer()) {
        int_val = value.as_int64();
    } else if (value.is_unsigned_integer()) {
        uint64_t uint_val = value.as_uint64();
        // 检查是否超出 int64_t 范围
        if (uint_val > static_cast<uint64_t>(INT64_MAX)) {
            // 超出 int64_t 范围，只能用于 uint64
            if (sig == "t") {
                return; // uint64 允许
            }
            TypeError("integer value too large for target type");
        }
        int_val = static_cast<int64_t>(uint_val);
    }

    // 根据签名检查值范围，允许类型转换
    if (sig == "y") {
        // uint8: 0 到 255
        if (int_val < 0 || int_val > UINT8_MAX) {
            TypeError("uint8 value out of range");
        }
        return;
    }
    if (sig == "q") {
        // uint16: 0 到 65535
        if (int_val < 0 || int_val > UINT16_MAX) {
            TypeError("uint16 value out of range");
        }
        return;
    }
    if (sig == "u") {
        // uint32: 0 到 4294967295
        if (int_val < 0 || int_val > UINT32_MAX) {
            TypeError("uint32 value out of range");
        }
        return;
    }
    if (sig == "t") {
        // uint64: 0 到 18446744073709551615
        if (value.is_signed_integer() && int_val < 0) {
            TypeError("uint64 value out of range");
        }
        return;
    }
    if (sig == "n") {
        // int16: -32768 到 32767
        if (int_val < INT16_MIN || int_val > INT16_MAX) {
            TypeError("int16 value out of range");
        }
        return;
    }
    if (sig == "i") {
        // int32: -2147483648 到 2147483647
        if (int_val < INT32_MIN || int_val > INT32_MAX) {
            TypeError("int32 value out of range");
        }
        return;
    }
    if (sig == "x") {
        // int64: -9223372036854775808 到 9223372036854775807
        return;
    }

    TypeError("unsupported integer signature");
}

void mdb_validator::check_utf8(const std::string& s) {
}
