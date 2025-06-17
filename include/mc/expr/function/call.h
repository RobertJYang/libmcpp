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

#ifndef MC_EXPR_FUNCTION_CALL_H
#define MC_EXPR_FUNCTION_CALL_H

#include <mc/dict.h>
#include <mc/reflect.h>
#include <mc/variant.h>
#include <string>

namespace mc::expr {

struct func_call {
    std::string      func;
    mc::mutable_dict params;

private:
    friend struct mc::reflect::reflector<func_call>;
};

inline void to_variant(const func_call& fc, mc::variant& v) {
    mc::mutable_dict d;
    d.insert("func", mc::variant(fc.func));
    d.insert("params", mc::variant(fc.params));
    v = mc::variant(d);
}

inline void from_variant(const mc::variant& v, func_call& fc) {
    const auto& d = v.as_dict();
    fc.func       = d.at("func").as_string();
    fc.params     = d.at("params").as_dict();   
}

inline bool is_function_call(const mc::variant& v) {
    if (!v.is_dict()) {
        return false;
    }

    const auto& d = v.as_dict();
    if (!d.contains("func") || !d.contains("params")) {
        return false;
    }

    if (!d.at("func").is_string()) {
        return false;
    }

    if (!d.at("params").is_dict()) {
        return false;
    }

    return true;
}

// 声明 is_relate_property 函数
bool is_relate_property(const mc::variant& value);

class func {
public:
    func() = default;
    
    // 修改构造函数参数顺序，使其与测试用例匹配
    func(const std::string& result, const mc::dict& args) : m_result(result), m_args(args) {}

    void validate_result();
    void validate_args();
    mc::variant call(const std::string_view& position, mc::mutable_dict& params);
    mc::mutable_dict get_relate_properties(const std::string_view& position, mc::mutable_dict& params);

    // 添加访问 m_args 的方法
    const mc::dict& get_args() const { return m_args; }
    void set_args(const mc::dict& args) { m_args = args; }

private:
    friend struct mc::reflect::reflector<func>;

    std::string m_result;
    mc::dict    m_args;
};

} // namespace mc::expr 

MC_REFLECT(mc::expr::func, ((m_result, "result"))((m_args, "args")));
MC_REFLECT(mc::expr::func_call, ((func, "func"))((params, "params")));

#endif // MC_EXPR_FUNCTION_CALL_H