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

/**
 * @file reflect_example.cpp
 * @brief 反射功能示例程序
 */
#include <iostream>
#include <mc/reflect.h>

using namespace mc;
using namespace mc::reflect;

/**
 * @brief 用户类，用于演示类反射
 */
class user {
public:
    std::string m_name;
    int m_age;
    bool m_is_admin;

    user() : m_name(""), m_age(0), m_is_admin(false) {}
    user(const std::string& name, int age, bool is_admin)
        : m_name(name), m_age(age), m_is_admin(is_admin) {}

    void print() const {
        std::cout << "用户信息：" << std::endl;
        std::cout << "  姓名: " << m_name << std::endl;
        std::cout << "  年龄: " << m_age << std::endl;
        std::cout << "  管理员: " << (m_is_admin ? "是" : "否") << std::endl;
    }
};

/**
 * @brief 权限枚举，用于演示枚举反射
 */
enum class permission {
    READ,
    WRITE,
    EXECUTE,
    ADMIN
};

// 使用反射宏注册类型
namespace mc {
namespace reflect {

template<>
struct reflector<user> {
    using is_defined = std::true_type;
    using is_enum = std::false_type;
    
    static const char* name() { return "user"; }
    
    template<typename Visitor>
    static void visit(Visitor&& visitor) {
        visitor("m_name", 
                [](const user& u) -> variant { return u.m_name; },
                [](user& u, const variant& v) { u.m_name = v.as<std::string>(); });
        visitor("m_age", 
                [](const user& u) -> variant { return u.m_age; },
                [](user& u, const variant& v) { u.m_age = v.as<int>(); });
        visitor("m_is_admin", 
                [](const user& u) -> variant { return u.m_is_admin; },
                [](user& u, const variant& v) { u.m_is_admin = v.as<bool>(); });
    }
    
    static void to_variant(const user& u, mutable_dict& dict) {
        visit([&](const char* name, auto getter, auto) {
            dict[name] = getter(u);
        });
    }
    
    static void from_variant(const dict& d, user& u) {
        visit([&](const char* name, auto, auto setter) {
            if (d.contains(name)) {
                setter(u, d[name]);
            }
        });
    }
};

template<>
struct reflector<permission> {
    using is_defined = std::true_type;
    using is_enum = std::true_type;
    
    static const char* name() { return "permission"; }
    
    static void to_variant(const permission& e, variant& var) {
        switch (e) {
            case permission::READ:
                var = "READ";
                break;
            case permission::WRITE:
                var = "WRITE";
                break;
            case permission::EXECUTE:
                var = "EXECUTE";
                break;
            case permission::ADMIN:
                var = "ADMIN";
                break;
            default:
                throw bad_enum_cast("无效的枚举值");
        }
    }
    
    static void from_variant(const variant& var, permission& e) {
        const std::string& s = var.as<std::string>();
        if (s == "READ") {
            e = permission::READ;
            return;
        }
        if (s == "WRITE") {
            e = permission::WRITE;
            return;
        }
        if (s == "EXECUTE") {
            e = permission::EXECUTE;
            return;
        }
        if (s == "ADMIN") {
            e = permission::ADMIN;
            return;
        }
        throw bad_enum_cast("无效的枚举字符串");
    }
};

} // namespace reflect
} // namespace mc

// 打印权限
void print_permission(permission p) {
    std::cout << "权限: ";
    switch (p) {
        case permission::READ:
            std::cout << "读取";
            break;
        case permission::WRITE:
            std::cout << "写入";
            break;
        case permission::EXECUTE:
            std::cout << "执行";
            break;
        case permission::ADMIN:
            std::cout << "管理员";
            break;
    }
    std::cout << std::endl;
}

/**
 * @brief 属性打印器，用于演示成员访问
 */
struct property_printer {
    const user& m_user;
    
    property_printer(const user& u) : m_user(u) {}
    
    template<typename T>
    void operator()(const char* name, const std::function<variant(const user&)>& getter, const std::function<void(user&, const variant&)>&) {
        std::cout << "属性 " << name << " = " << getter(m_user).as<T>() << std::endl;
    }
};

int main() {
    // 创建用户对象
    user u{"张三", 30, true};
    u.print();
    
    // 转换为变体
    variant var = mc::reflect::to_variant(u);
    
    // 检查变体类型
    if (var.get_type() == variant::type_id::object_type) {
        std::cout << "变体是一个对象类型" << std::endl;
        
        // 获取字典并修改值
        mc::mutable_dict dict = var.as<mc::mutable_dict>();
        dict["m_name"] = "李四";
        dict["m_age"] = 25;
        
        // 转换回用户对象
        user u2;
        mc::reflect::from_variant(dict, u2);
        u2.print();
    }
    
    // 枚举反射示例
    permission p = permission::READ;
    variant enum_var;
    mc::reflect::to_variant(p, enum_var);
    
    if (enum_var.get_type() == variant::type_id::string_type) {
        std::cout << "枚举值: " << enum_var.as<std::string>() << std::endl;
        
        // 从字符串转换为枚举
        variant write_var("WRITE");
        permission p2;
        mc::reflect::from_variant(write_var, p2);
        print_permission(p2);
    }
    
    // 成员访问示例
    std::cout << "\n成员访问示例:" << std::endl;
    property_printer printer(u);
    mc::reflect::reflector<user>::visit([&](const char* name, auto getter, auto setter) {
        if (std::string(name) == "m_name") {
            printer.operator()<std::string>(name, getter, setter);
        } else if (std::string(name) == "m_age") {
            printer.operator()<int>(name, getter, setter);
        } else if (std::string(name) == "m_is_admin") {
            printer.operator()<bool>(name, getter, setter);
        }
    });
    
    return 0;
} 