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

#ifndef MC_EXPR_FUNCTION_PARSER_H
#define MC_EXPR_FUNCTION_PARSER_H

#include <mc/dict.h>
#include <mc/reflect.h>
#include <mc/expr/function/call.h>
#include <memory>
#include <string>

namespace mc::expr {

struct relate_property {
    std::string type;
    std::string object_name;
    std::string property_name;
    std::string full_name;
};

// 属性类型配置结构
struct property_type_config {
    std::string_view prefix;
    std::string_view type_name;
    std::string_view error_message_prefix;
};

class func_parser {
public:
    static func_parser& get_instance() {
        static func_parser instance;
        return instance;
    }

    func_call parse_function_call(const std::string& input);
    relate_property parse_property(const std::string& input);
    relate_property parse_sync_property(const std::string& input);
    relate_property parse_ref_property(const std::string& input);

private:
    func_parser()                         = default;
    ~func_parser()                        = default;
    func_parser(const func_parser&)            = delete;
    func_parser& operator=(const func_parser&) = delete;

    std::pair<std::string, std::string> parse_dotted_property(const std::string& input);
    std::string parse_function_name(const std::string& input);
    std::string parse_params_string(const std::string& input);
    
    // 通用属性解析函数
    relate_property parse_property_with_type(const std::string& input, 
                                             const property_type_config& config);
};

class param_value_comparator {
public:
    bool operator()(const mc::variant& a, const mc::variant& b) const {
        if (a.get_type() != b.get_type()) {
            return false;
        }

        switch (a.get_type()) {
        case mc::type_id::string_type:
            return a.as_string() == b.as_string();
        case mc::type_id::int8_type:
            return a.as_int8() == b.as_int8();
        case mc::type_id::double_type:
            return a.as_double() == b.as_double();
        case mc::type_id::bool_type:
            return a.as_bool() == b.as_bool();
        case mc::type_id::object_type:
            return compare_dicts(a.as_dict(), b.as_dict());
        default:
            return false;
        }
    }

private:
    bool compare_dicts(const mc::dict& a, const mc::dict& b) const {
        if (a.size() != b.size()) {
            return false;
        }

        for (const auto& entry : a) {
            auto it = b.find(entry.key);
            if (it == b.end()) {
                return false;
            }
            if (!(*this)(entry.value, it->value)) {
                return false;
            }
        }
        return true;
    }
};

} // namespace mc::expr

MC_REFLECT(mc::expr::relate_property, ((type, "type"))((object_name, "object_name"))((property_name, "property_name"))((full_name, "full_name")));

#endif // MC_EXPR_FUNCTION_PARSER_H