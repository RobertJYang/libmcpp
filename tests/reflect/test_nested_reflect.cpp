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
 * @file test_nested_reflect.cpp
 * @brief 测试反射嵌套场景
 */
#include <functional>
#include <gtest/gtest.h>
#include <map>
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/reflect.h>
#include <mc/variant.h>
#include <string>
#include <vector>

using namespace mc;
using namespace mc::reflect;

// 测试用的状态枚举
enum class test_status { ACTIVE, INACTIVE, PENDING };
MC_REFLECT_ENUM(test_status, (ACTIVE)(INACTIVE)(PENDING))

// 测试用的权限枚举
enum class test_permission { READ, WRITE, EXECUTE };
MC_REFLECT_ENUM(test_permission, (READ)(WRITE)(EXECUTE))

// 测试用的地址类
class test_address {
public:
    std::string m_city;
    std::string m_street;
    int         m_number;

    test_address() : m_city(""), m_street(""), m_number(0) {
    }
    test_address(const std::string& city, const std::string& street, int number)
        : m_city(city), m_street(street), m_number(number) {
    }

    bool operator==(const test_address& other) const {
        return m_city == other.m_city && m_street == other.m_street && m_number == other.m_number;
    }
};
MC_REFLECT(test_address, (m_city)(m_street)(m_number))

// 测试用的联系信息类
class test_contact {
public:
    std::string  m_email;
    std::string  m_phone;
    test_address m_address; // 嵌套对象

    test_contact() : m_email(""), m_phone("") {
    }
    test_contact(const std::string& email, const std::string& phone, const test_address& address)
        : m_email(email), m_phone(phone), m_address(address) {
    }

    bool operator==(const test_contact& other) const {
        return m_email == other.m_email && m_phone == other.m_phone && m_address == other.m_address;
    }
};
MC_REFLECT(test_contact, (m_email)(m_phone)(m_address))

// 测试用的用户类（包含嵌套对象和枚举）
class test_user {
public:
    std::string                         m_username;
    test_contact                        m_contact;     // 嵌套对象
    test_status                         m_status;      // 嵌套枚举
    std::vector<test_permission>        m_permissions; // 枚举数组
    std::map<std::string, test_address> m_addresses;   // 键值对嵌套

    test_user() : m_username(""), m_status(test_status::INACTIVE) {
    }
    test_user(const std::string& username, const test_contact& contact, test_status status,
              const std::vector<test_permission>&        permissions,
              const std::map<std::string, test_address>& addresses)
        : m_username(username), m_contact(contact), m_status(status), m_permissions(permissions),
          m_addresses(addresses) {
    }

    bool operator==(const test_user& other) const {
        return m_username == other.m_username && m_contact == other.m_contact &&
               m_status == other.m_status && m_permissions == other.m_permissions &&
               m_addresses == other.m_addresses;
    }
};
MC_REFLECT(test_user, (m_username)(m_contact)(m_status)(m_permissions)(m_addresses))

// 测试嵌套类反射
TEST(NestedReflectTest, NestedClassReflection) {
    // 创建嵌套对象
    test_address address("北京", "中关村", 123);
    test_contact contact("test@example.com", "12345678901", address);

    // 检查类型是否可反射
    EXPECT_TRUE(is_reflectable<test_address>());
    EXPECT_TRUE(is_reflectable<test_contact>());

    // 获取类型名称
    EXPECT_STREQ(reflector<test_address>::name(), "test_address");
    EXPECT_STREQ(reflector<test_contact>::name(), "test_contact");

    // 转换为变体
    variant var(contact);

    // 检查变体类型
    EXPECT_TRUE(var.is_object());

    // 检查字典内容
    const dict& d = var.as<dict>();
    EXPECT_EQ(d.size(), 3);
    EXPECT_EQ(d["m_email"], "test@example.com");
    EXPECT_EQ(d["m_phone"], "12345678901");
    EXPECT_TRUE(d["m_address"].is_object());

    // 检查嵌套对象
    const dict& addr_dict = d["m_address"].as<dict>();
    EXPECT_EQ(addr_dict["m_city"], "北京");
    EXPECT_EQ(addr_dict["m_street"], "中关村");
    EXPECT_EQ(addr_dict["m_number"], 123);

    // 从变体转回对象
    test_contact contact2 = var.as<test_contact>();
    EXPECT_EQ(contact2.m_email, "test@example.com");
    EXPECT_EQ(contact2.m_phone, "12345678901");
    EXPECT_EQ(contact2.m_address.m_city, "北京");
    EXPECT_EQ(contact2.m_address.m_street, "中关村");
    EXPECT_EQ(contact2.m_address.m_number, 123);
    EXPECT_EQ(contact, contact2);
}

// 测试嵌套枚举反射
TEST(NestedReflectTest, NestedEnumReflection) {
    // 创建测试对象
    test_address                 address("上海", "南京路", 456);
    test_contact                 contact("admin@example.com", "98765432101", address);
    std::vector<test_permission> permissions      = {test_permission::READ, test_permission::WRITE};
    std::map<std::string, test_address> addresses = {{"home", test_address("北京", "中关村", 123)},
                                                     {"work", test_address("上海", "南京路", 456)}};
    test_user user("admin", contact, test_status::ACTIVE, permissions, addresses);

    // 转换为变体
    variant var(user);

    // 检查变体类型
    EXPECT_TRUE(var.is_object());

    // 检查字典内容
    const dict& d = var.as<dict>();
    EXPECT_EQ(d["m_username"], "admin");
    EXPECT_EQ(d["m_status"], "ACTIVE");

    // 检查权限数组
    EXPECT_TRUE(d["m_permissions"].is_array());
    const auto& perms_array = d["m_permissions"].as<std::vector<variant>>();
    EXPECT_EQ(perms_array.size(), 2);
    EXPECT_EQ(perms_array[0], "READ");
    EXPECT_EQ(perms_array[1], "WRITE");

    // 从变体转回对象
    test_user user2 = var.as<test_user>();
    EXPECT_EQ(user2.m_username, "admin");
    EXPECT_EQ(user2.m_status, test_status::ACTIVE);
    EXPECT_EQ(user2.m_permissions.size(), 2);
    EXPECT_EQ(user2.m_permissions[0], test_permission::READ);
    EXPECT_EQ(user2.m_permissions[1], test_permission::WRITE);
    EXPECT_EQ(user, user2);
}

// 测试部分更新嵌套对象
TEST(NestedReflectTest, PartialNestedUpdate) {
    // 创建原始对象
    test_address                        address("北京", "中关村", 123);
    test_contact                        contact("test@example.com", "12345678901", address);
    std::vector<test_permission>        permissions = {test_permission::READ};
    std::map<std::string, test_address> addresses = {{"home", test_address("北京", "中关村", 123)}};
    test_user user("user1", contact, test_status::INACTIVE, permissions, addresses);

    // 使用初始化列表构造字典并更新对象
    mc::reflect::reflector<test_user>::from_variant(
        dict{{"m_status", "ACTIVE"},
             {"m_contact",
              dict{{"m_phone", "99999999999"}, {"m_address", dict{{"m_city", "上海"}}}}}},
        user);

    // 检查更新后的对象
    EXPECT_EQ(user.m_username, "user1");                    // 未更新
    EXPECT_EQ(user.m_status, test_status::ACTIVE);          // 已更新
    EXPECT_EQ(user.m_contact.m_email, "test@example.com");  // 未更新
    EXPECT_EQ(user.m_contact.m_phone, "99999999999");       // 已更新
    EXPECT_EQ(user.m_contact.m_address.m_city, "上海");     // 已更新
    EXPECT_EQ(user.m_contact.m_address.m_street, "中关村"); // 未更新
    EXPECT_EQ(user.m_contact.m_address.m_number, 123);      // 未更新
}

// 测试复杂嵌套结构的序列化和反序列化
TEST(NestedReflectTest, ComplexNestedSerialization) {
    // 创建复杂嵌套对象
    test_address                 home("北京", "中关村", 123);
    test_address                 work("上海", "南京路", 456);
    test_contact                 contact("admin@example.com", "12345678901", home);
    std::vector<test_permission> permissions      = {test_permission::READ, test_permission::WRITE,
                                                     test_permission::EXECUTE};
    std::map<std::string, test_address> addresses = {{"home", home}, {"work", work}};
    test_user user("admin", contact, test_status::ACTIVE, permissions, addresses);

    // 转换为变体
    variant var(user);

    // 检查变体类型
    EXPECT_TRUE(var.is_object());

    // 检查嵌套结构
    const dict& d = var.as<dict>();

    // 检查联系信息
    const dict& contact_dict = d["m_contact"].as<dict>();
    EXPECT_EQ(contact_dict["m_email"], "admin@example.com");
    EXPECT_EQ(contact_dict["m_phone"], "12345678901");

    // 检查地址
    const dict& addr_dict = contact_dict["m_address"].as<dict>();
    EXPECT_EQ(addr_dict["m_city"], "北京");

    // 检查地址映射
    const dict& addresses_dict = d["m_addresses"].as<dict>();
    EXPECT_EQ(addresses_dict.size(), 2);

    const dict& home_dict = addresses_dict["home"].as<dict>();
    EXPECT_EQ(home_dict["m_city"], "北京");
    EXPECT_EQ(home_dict["m_street"], "中关村");

    const dict& work_dict = addresses_dict["work"].as<dict>();
    EXPECT_EQ(work_dict["m_city"], "上海");
    EXPECT_EQ(work_dict["m_street"], "南京路");

    // 从变体转回对象
    test_user user2 = var.as<test_user>();
    EXPECT_EQ(user, user2);
}

// 测试嵌套对象的动态修改
TEST(NestedReflectTest, DynamicNestedModification) {
    // 创建原始对象
    test_address address("北京", "中关村", 123);
    test_contact contact("test@example.com", "12345678901", address);

    // 转换为变体
    variant var(contact);

    // 获取可变字典
    mutable_dict md(var.as<dict>());

    // 修改嵌套对象
    mutable_dict addr_md(md["m_address"].as<dict>());
    addr_md["m_city"]   = "广州";
    addr_md["m_number"] = 789;
    md["m_address"]     = addr_md;

    // 修改顶层属性
    md["m_email"] = "new@example.com";

    // 从修改后的变体转回对象
    test_contact modified = variant(md).as<test_contact>();

    // 检查修改结果
    EXPECT_EQ(modified.m_email, "new@example.com");
    EXPECT_EQ(modified.m_phone, "12345678901"); // 未修改
    EXPECT_EQ(modified.m_address.m_city, "广州");
    EXPECT_EQ(modified.m_address.m_street, "中关村"); // 未修改
    EXPECT_EQ(modified.m_address.m_number, 789);
}

// 测试嵌套集合的反射
TEST(NestedReflectTest, NestedCollections) {
    // 创建带有集合的测试对象
    test_address                 home("北京", "中关村", 123);
    test_address                 work("上海", "南京路", 456);
    std::vector<test_permission> permissions      = {test_permission::READ, test_permission::WRITE};
    std::map<std::string, test_address> addresses = {{"home", home}, {"work", work}};

    // 使用初始化列表构造复杂嵌套结构
    dict complex_dict{
        {"user_info",
         dict{{"username", "admin"},
              {"permissions", variants{"READ", "WRITE", "EXECUTE"}},
              {"addresses",
               dict{{"home", dict{{"m_city", "北京"}, {"m_street", "中关村"}, {"m_number", 123}}},
                    {"work",
                     dict{{"m_city", "上海"}, {"m_street", "南京路"}, {"m_number", 456}}}}}}}};

    // 从字典中提取数据
    const auto& user_info = complex_dict["user_info"];
    EXPECT_EQ(user_info["username"], "admin");

    // 提取权限数组
    const auto& perms = user_info["permissions"];
    EXPECT_EQ(perms.size(), 3);
    EXPECT_EQ(perms[0], "READ");
    EXPECT_EQ(perms[1], "WRITE");
    EXPECT_EQ(perms[2], "EXECUTE");

    // 提取地址映射
    const auto& addr_map = user_info["addresses"];
    EXPECT_EQ(addr_map.size(), 2);

    // 从地址映射中提取地址
    const auto&  home_addr      = addr_map["home"];
    test_address extracted_home = home_addr.as<test_address>();
    EXPECT_EQ(extracted_home.m_city, "北京");
    EXPECT_EQ(extracted_home.m_street, "中关村");
    EXPECT_EQ(extracted_home.m_number, 123);

    const auto&  work_addr      = addr_map["work"];
    test_address extracted_work = work_addr.as<test_address>();
    EXPECT_EQ(extracted_work.m_city, "上海");
    EXPECT_EQ(extracted_work.m_street, "南京路");
    EXPECT_EQ(extracted_work.m_number, 456);
}