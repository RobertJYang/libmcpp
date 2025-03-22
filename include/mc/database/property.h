/*
* Copyright (c) 2023 Huawei Technologies Co., Ltd.
* openUBMC is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*         http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

#ifndef MC_DATABASE_PROPERTY_H
#define MC_DATABASE_PROPERTY_H

#include <string>
#include <mc/variant.h>
#include <type_traits>

namespace mc::database {

// 前置声明
class db_object_base;

template<typename T>
class db_object;

// 统一的属性模板
template<typename T>
class property {
public:
    // 构造函数
    property() = default;
    
    // 赋值操作符
    property& operator=(const T& value) {
        m_value = value;
        m_dirty = true;
        
        // 通过父对象通知属性变更
        if (m_parent && !m_name.empty()) {
            mc::variant var = value;
            static_cast<db_object<T>*>(m_parent)->update_property(m_name.c_str(), var);
        }
        return *this;
    }
    
    // 类型转换操作符（读取访问）
    operator T() const {
        return m_value;
    }
    
    // 获取值（读取访问）
    const T& get() const {
        return m_value;
    }

private:
    T m_value{};
    bool m_dirty{false};
    std::string m_name;
    db_object_base* m_parent{nullptr};
    
    // 允许db_object访问私有成员
    template<typename U>
    friend class db_object;
};

} // namespace mc::database

// 为mc::database::property类型添加转换支持，定义在mc命名空间
namespace mc {
    
template<typename T>
void to_variant(const mc::database::property<T>& var, mc::variant& vo) {
    to_variant(var.get(), vo);
}

template<typename T>
void from_variant(const mc::variant& var, mc::database::property<T>& vo) {
    T value;
    from_variant(var, value);
    vo = value;
}

} // namespace mc

#endif // MC_DATABASE_PROPERTY_H 