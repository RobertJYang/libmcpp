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

#include <chrono>
#include <map>
#include <mc/database/table.h>
#include <mc/dict.h>
#include <mc/im/ref_ptr.h>
#include <mc/reflect.h>
#include <mc/variant.h>
#include <string>
#include <vector>

// 定义测试性能对比用户类
class performance_user {
public:
    performance_user() = default;

    performance_user(int id, std::string name, int age, std::string city, std::string email,
                     std::string phone, std::string address, std::string job, int salary,
                     bool active)
        : m_id(id), m_name(std::move(name)), m_age(age), m_city(std::move(city)),
          m_email(std::move(email)), m_phone(std::move(phone)), m_address(std::move(address)),
          m_job(std::move(job)), m_salary(salary), m_active(active), m_ref_count(0) {
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

    // 引用计数支持
    void add_ref() {
        m_ref_count++;
    }

    bool release_ref() {
        return (--m_ref_count == 0);
    }

    // 用于创建对象指针
    using object_id_type = uint32_t;
    using alloc_type     = std::allocator<performance_user>;
    using pointer        = mc::im::ref_ptr<performance_user>;

    // 内存管理相关接口
    alloc_type m_alloc; // 添加分配器实例

    void* operator new(size_t size) {
        alloc_type alloc;
        auto       ptr = alloc.allocate(1);
        return ptr;
    }

    void operator delete(void* ptr) {
        alloc_type alloc;
        alloc.deallocate(static_cast<performance_user*>(ptr), 1);
    }

    static pointer create(const performance_user& other) {
        return pointer(new performance_user(other));
    }

    // 成员变量
    uint32_t    m_id;
    std::string m_name;
    int         m_age;
    std::string m_city;
    std::string m_email;
    std::string m_phone;
    std::string m_address;
    std::string m_job;
    int         m_salary;
    bool        m_active;

private:
    uint32_t m_ref_count;
};

MC_REFLECT(performance_user,
           (m_id)(m_name)(m_age)(m_city)(m_email)(m_phone)(m_address)(m_job)(m_salary)(m_active))

// 定义表类型
using performance_table = mc::database::table<performance_user>;

namespace {

class table_meta_test : public ::testing::Test {
protected:
    void SetUp() override {
        // 添加测试数据
        for (int i = 1; i <= 100; i++) {
            std::string name    = "用户_" + std::to_string(i);
            std::string email   = "user_" + std::to_string(i) + "@example.com";
            std::string phone   = "123456789" + std::to_string(i % 10);
            std::string address = "北京市海淀区" + std::to_string(i) + "号";
            std::string job =
                (i % 5 == 0)
                    ? "工程师"
                    : ((i % 5 == 1) ? "经理"
                                    : ((i % 5 == 2) ? "销售" : ((i % 5 == 3) ? "人事" : "财务")));
            int  salary = 5000 + (i * 100);
            bool active = (i % 3 != 0);

            users.add(performance_user(
                i, name, 20 + (i % 30),
                (i % 4 == 0) ? "北京" : ((i % 4 == 1) ? "上海" : ((i % 4 == 2) ? "广州" : "深圳")),
                email, phone, address, job, salary, active));
        }
    }

    performance_table users;
};

// 性能测试：使用传统方式获取属性
TEST_F(table_meta_test, performance_test) {
    const int TEST_COUNT = 1000;

    // 准备一些查询条件
    std::vector<mc::mutable_dict> queries;
    for (int i = 0; i < 100; i++) {
        mc::mutable_dict query;
        query["m_age"] = 25 + (i % 20);
        query["m_city"] =
            (i % 4 == 0) ? "北京" : ((i % 4 == 1) ? "上海" : ((i % 4 == 2) ? "广州" : "深圳"));
        queries.push_back(query);
    }

    // 测试多次查询的总时间
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < TEST_COUNT; i++) {
        const auto& query   = queries[i % queries.size()];
        auto        results = users.query_by_dict(query);
    }

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "使用元数据优化后的查询耗时: " << duration.count() << " 毫秒" << std::endl;

    // 这个测试没有实际断言，只是为了衡量性能
    // 如果需要与传统方式进行比较，需要在同一环境下测试
}

// 验证获取属性的正确性
TEST_F(table_meta_test, property_access_test) {
    // 获取一个用户对象
    auto it = users.find_by_object_id(1);
    ASSERT_FALSE(it.is_end());

    performance_user user = *it;

    // 测试直接使用元数据获取属性
    auto name_value = mc::database::get_property(user, "m_name");
    ASSERT_TRUE(name_value.has_value());
    EXPECT_EQ(name_value.value().as<std::string>(), "用户_1");

    auto age_value = mc::database::get_property(user, "m_age");
    ASSERT_TRUE(age_value.has_value());
    EXPECT_EQ(age_value.value().as<int>(), 21); // 20 + (1 % 30)

    // 测试不存在的属性
    auto invalid_value = mc::database::get_property(user, "m_invalid");
    EXPECT_FALSE(invalid_value.has_value());
}

// 测试查询功能是否正常工作
TEST_F(table_meta_test, query_test) {
    // 单条件查询
    {
        mc::mutable_dict query;
        query["m_job"] = "工程师";

        auto results = users.query_by_dict(query);
        EXPECT_FALSE(results.empty());

        for (const auto& user : results) {
            EXPECT_EQ(user->m_job, "工程师");
        }
    }

    // 多条件查询
    {
        mc::mutable_dict query;
        query["m_city"]   = "北京";
        query["m_active"] = true;

        auto results = users.query_by_dict(query);
        EXPECT_FALSE(results.empty());

        for (const auto& user : results) {
            EXPECT_EQ(user->m_city, "北京");
            EXPECT_TRUE(user->m_active);
        }
    }
}

} // namespace