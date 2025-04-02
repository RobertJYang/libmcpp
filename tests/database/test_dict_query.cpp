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

#include <gtest/gtest.h>

#include <map>
#include <mc/database/table.h>
#include <mc/dict.h>
#include <mc/reflect.h>
#include <mc/variant.h>
#include <string>
#include <vector>

namespace {

namespace mdb = mc::database;

// 定义测试用户类（支持反射）
class test_user {
public:
    test_user() = default;

    test_user(int id, std::string name, int age, std::string city)
        : m_id(id), m_name(std::move(name)), m_age(age), m_city(std::move(city)) {
    }

    // 用于对象ID
    uint32_t get_object_id() const {
        return m_id;
    }

    void set_object_id(uint32_t id) {
        m_id = id;
    }

    bool has_valid_id() const {
        return m_id > 0;
    }

    // 用于创建对象指针
    using object_id_type = uint32_t;
    using alloc_type     = std::allocator<test_user>;
    using pointer        = mc::im::ref_ptr<test_user, test_user*>;

    static pointer create(const test_user& other) {
        return pointer(new test_user(other));
    }

    // 成员变量
    uint32_t    m_id;
    std::string m_name;
    int         m_age;
    std::string m_city;
};

// 使用反射宏定义反射信息
MC_REFLECT(test_user, (m_id)(m_name)(m_age)(m_city))

// 定义标签类型
struct by_age : mdb::tag_base {};
struct by_city : mdb::tag_base {};

// 定义表类型
using user_table = mdb::table<
    test_user,
    mdb::indexed_by<
        mdb::ordered_unique<mdb::member<test_user, uint32_t, &test_user::m_id>>,
        mdb::ordered_unique<mdb::member<test_user, std::string, &test_user::m_name>>,
        mdb::ordered_non_unique<mdb::member<test_user, int, &test_user::m_age>, by_age>,
        mdb::ordered_non_unique<mdb::member<test_user, std::string, &test_user::m_city>, by_city>>>;

class dict_query_test : public ::testing::Test {
protected:
    void SetUp() override {
        // 添加测试数据
        users.add(test_user(1, "张三", 25, "北京"));
        users.add(test_user(2, "李四", 30, "上海"));
        users.add(test_user(3, "王五", 25, "广州"));
        users.add(test_user(4, "赵六", 35, "深圳"));
        users.add(test_user(5, "钱七", 40, "北京"));
    }

    void TearDown() override {
        // 清理测试数据
    }

    user_table users;
};

// 测试使用mc::variant_object进行查询
TEST_F(dict_query_test, variant_object_query) {
    // 单条件查询
    {
        mc::mutable_dict query;
        query["m_age"] = 25;

        auto results = users.query_by_dict(query);
        EXPECT_EQ(results.size(), 2);

        // 验证查询结果
        bool found_zhang = false;
        bool found_wang  = false;
        for (const auto& user : results) {
            if (user->m_name == "张三") {
                found_zhang = true;
                EXPECT_EQ(user->m_age, 25);
                EXPECT_EQ(user->m_city, "北京");
            } else if (user->m_name == "王五") {
                found_wang = true;
                EXPECT_EQ(user->m_age, 25);
                EXPECT_EQ(user->m_city, "广州");
            }
        }
        EXPECT_TRUE(found_zhang);
        EXPECT_TRUE(found_wang);
    }

    // 多条件查询
    {
        mc::mutable_dict query;
        query["m_age"]  = 25;
        query["m_city"] = "北京";

        auto results = users.query_by_dict(query);
        EXPECT_EQ(results.size(), 1);
        EXPECT_EQ(results[0]->m_name, "张三");
    }

    // 查询单个结果
    {
        mc::mutable_dict query;
        query["m_name"] = "李四";

        auto result = users.query_one_by_dict(query);
        ASSERT_TRUE(result != nullptr);
        EXPECT_EQ(result->m_name, "李四");
        EXPECT_EQ(result->m_age, 30);
    }

    // 不存在的条件
    {
        mc::mutable_dict query;
        query["m_name"] = "不存在";

        auto results = users.query_by_dict(query);
        EXPECT_TRUE(results.empty());

        auto result = users.query_one_by_dict(query);
        EXPECT_EQ(result, nullptr);
    }
}

// 测试使用std::map进行查询
TEST_F(dict_query_test, std_map_query) {
    // 单条件查询
    {
        std::map<std::string, mc::variant> query;
        query["m_city"] = "北京";

        auto results = users.query_by_dict(query);
        EXPECT_EQ(results.size(), 2);

        // 验证查询结果
        bool found_zhang = false;
        bool found_qian  = false;
        for (const auto& user : results) {
            if (user->m_name == "张三") {
                found_zhang = true;
                EXPECT_EQ(user->m_age, 25);
            } else if (user->m_name == "钱七") {
                found_qian = true;
                EXPECT_EQ(user->m_age, 40);
            }
        }
        EXPECT_TRUE(found_zhang);
        EXPECT_TRUE(found_qian);
    }

    // 多条件查询
    {
        std::map<std::string, mc::variant> query;
        query["m_city"] = "北京";
        query["m_age"]  = 40;

        auto results = users.query_by_dict(query);
        EXPECT_EQ(results.size(), 1);
        EXPECT_EQ(results[0]->m_name, "钱七");
    }
}

} // namespace