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

/**
 * @file test_index.cpp
 * @brief 数据库索引单元测试
 */

#include <algorithm>
#include <functional>
#include <gtest/gtest.h>
#include <mc/db/index.h>
#include <mc/db/key.h>
#include <mc/db/key_extractor.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct user : mc::db::object_base {
    using id_type = int;

    user() = default;
    user(int id, std::string name, int age, double score = 0.0, std::string city = std::string(),
         std::string department = std::string())
        : m_name(name), m_age(age), m_score(score), m_city(city), m_department(department) {
        set_object_id(id);
    }

    std::string m_name;
    int         m_age;
    double      m_score;
    std::string m_city;
    std::string m_department;

    const std::string& name() const {
        return m_name;
    }

    int age() const {
        return m_age;
    }

    double score() const {
        return m_score;
    }
};
} // namespace

// 测试 mc::db::index 的基本功能
TEST(database_index_test, mc_database_index_basic) {
    // 创建测试用户数据
    user u1(1, "张三", 20);
    user u2(2, "李四", 25);
    user u3(3, "王五", 22);

    auto name_extractor = mc::db::make_key<user, const std::string&, &user::name>();
    auto index          = mc::db::make_index<user, true>(name_extractor);

    // 添加用户到索引
    EXPECT_TRUE(index->add(u1));
    EXPECT_TRUE(index->add(u2));
    EXPECT_TRUE(index->add(u3));

    // 按名称查找用户
    auto found_u1 = index->find("张三");
    ASSERT_NE(found_u1, index->end());
    EXPECT_EQ(found_u1->get_object_id(), 1);

    auto found_u2 = index->find("李四");
    ASSERT_NE(found_u2, index->end());
    EXPECT_EQ(found_u2->get_object_id(), 2);

    // 查找不存在的用户
    auto not_found = index->find("赵六");
    EXPECT_EQ(not_found, index->end());

    // 删除用户
    EXPECT_TRUE(index->remove("张三").has_value());

    // 验证删除后无法找到
    found_u1 = index->find("张三");
    EXPECT_EQ(found_u1, index->end());
}

// 测试 mc::db 函数对象键提取器
TEST(database_index_test, mc_database_index_functor_key) {
    // 创建测试用户数据
    user u1(1, "张三", 20);
    user u2(2, "李四", 25);
    user u3(3, "王五", 22);

    // 使用函数对象创建键提取器
    auto extractor = mc::db::make_key<user>([](const user& u) -> int {
        return u.age();
    });

    // 创建索引
    auto index = mc::db::make_index<user, true>(extractor);

    // 添加用户并验证
    EXPECT_TRUE(index->add(u1));
    EXPECT_TRUE(index->add(u2));
    EXPECT_TRUE(index->add(u3));

    // 按年龄查找
    auto found = index->find(25);
    ASSERT_NE(found, index->end());
    EXPECT_EQ(found->get_object_id(), 2);
}

// 测试 mc::db 复合操作
TEST(database_index_test, mc_database_index_operations) {
    // 创建测试用户数据
    user u1(1, "张三", 20);
    user u2(2, "李四", 25);
    user u3(3, "王五", 22);

    // 使用mc::database的成员函数键提取器
    auto name_extractor = mc::db::make_key<user, const std::string&, &user::name>();

    // 创建索引
    auto index = mc::db::make_index<user, true>(name_extractor);

    // 添加用户到索引
    EXPECT_TRUE(index->add(u1));
    EXPECT_TRUE(index->add(u2));
    EXPECT_TRUE(index->add(u3));

    // 查找用户
    auto found_u1 = index->find("张三");
    ASSERT_NE(found_u1, index->end());
    EXPECT_EQ(found_u1->get_object_id(), 1);

    // 删除用户并验证
    EXPECT_TRUE(index->remove("张三").has_value());

    found_u1 = index->find("张三");
    EXPECT_EQ(found_u1, index->end());

    // 重新插入，验证可以再次添加
    user u1_new(4, "赵六", 21); // 新用户，名字不同
    EXPECT_TRUE(index->add(u1_new));

    // 查找新插入的用户
    auto found_u4 = index->find("赵六");
    ASSERT_NE(found_u4, index->end());
    EXPECT_EQ(found_u4->get_object_id(), 4); // 应该是新用户
    EXPECT_EQ(found_u4->age(), 21);          // 验证是新用户
}

// 测试组合键索引及前缀查询迭代
TEST(database_index_test, compound_key_test) {
    // 创建测试用户数据
    user u1(1, "张三", 20);
    user u2(2, "张三", 25);
    user u3(3, "李四", 20);
    user u4(4, "李四", 25);
    user u5(5, "王五", 30);

    // 创建组合索引：名字 + 年龄
    auto name_extractor     = mc::db::make_key<user, const std::string&, &user::name>();
    auto age_extractor      = mc::db::make_key<user, int, &user::age>();
    auto name_age_extractor = mc::db::make_key(name_extractor, age_extractor);
    auto index              = mc::db::make_index<user, true>(name_age_extractor);

    // 添加用户到索引
    EXPECT_TRUE(index->add(u1));
    EXPECT_TRUE(index->add(u2));
    EXPECT_TRUE(index->add(u3));
    EXPECT_TRUE(index->add(u4));
    EXPECT_TRUE(index->add(u5));

    // 1. 直接查找特定用户
    {
        auto it = index->find(u1);
        ASSERT_FALSE(it.is_end());
        EXPECT_EQ(it->get_object_id(), 1);
    }

    // 2. 只使用组合索引的前半截查询: 张三
    {
        auto it = index->find("张三");
        ASSERT_FALSE(it.is_end());

        // 应该找到多个"张三"的用户
        std::vector<int> found_ids;
        for (; !it.is_end() && it->m_name == "张三"; ++it) {
            found_ids.push_back(it->get_object_id());
        }

        // 应该找到两个"张三"的用户 (ID: 1, 2)
        ASSERT_EQ(found_ids.size(), 2);
        std::sort(found_ids.begin(), found_ids.end());
        EXPECT_EQ(found_ids[0], 1);
        EXPECT_EQ(found_ids[1], 2);
    }

    // 3. 只使用组合索引的后半截查询: 名字，使用 equal_range
    {
        auto it = index->equal_range("张三");
        ASSERT_FALSE(it.first.is_end());
        ASSERT_TRUE(it.first != it.second);

        // 应该找到多个"张三"的用户
        std::vector<int> found_ids;
        for (; it.first != it.second; ++it.first) {
            found_ids.push_back(it.first->get_object_id());
        }

        // 应该找到两个"张三"的用户 (ID: 1, 2)
        ASSERT_EQ(found_ids.size(), 2);
        std::sort(found_ids.begin(), found_ids.end());
        EXPECT_EQ(found_ids[0], 1);
        EXPECT_EQ(found_ids[1], 2);
    }

    // 4. 使用find方法查询
    {
        auto it = index->find("李四");

        // 应该找到"李四"的用户
        std::vector<int> found_ids;
        for (; !it.is_end() && it->m_name == "李四"; ++it) {
            found_ids.push_back(it->get_object_id());
        }

        // 应该找到两个"李四"的用户 (ID: 3, 4)
        ASSERT_EQ(found_ids.size(), 2);
        std::sort(found_ids.begin(), found_ids.end());
        EXPECT_EQ(found_ids[0], 3);
        EXPECT_EQ(found_ids[1], 4);

        size_t i   = 0;
        auto   ret = index->equal_range("李四");
        for (auto it = ret.first; it != ret.second; ++it) {
            EXPECT_EQ(found_ids[i++], it->get_object_id());
        }
    }

    // 5. 使用全量组合key查询
    {
        auto it = index->find("李四", 25);

        // 应该找到"李四"的用户
        std::vector<int> found_ids;
        for (; !it.is_end() && it->m_name == "李四"; ++it) {
            found_ids.push_back(it->get_object_id());
        }

        // 应该找到两个"李四"的用户 (ID: 4)
        ASSERT_EQ(found_ids.size(), 1);
        std::sort(found_ids.begin(), found_ids.end());
        EXPECT_EQ(found_ids[0], 4);

        size_t i   = 0;
        auto   ret = index->equal_range("李四", 25);
        for (auto it = ret.first; it != ret.second; ++it) {
            EXPECT_EQ(found_ids[i++], it->get_object_id());
        }

        auto ret1 = index->equal_range("李四", 20);
        EXPECT_EQ(ret1.first->m_age, 20);
        EXPECT_EQ(ret1.second->m_age, 25); // 结束位置就是(李四, 25)
    }

    // 6. 使用begin/end迭代所有元素
    {
        std::vector<int> all_ids;
        for (auto& u : *index) {
            all_ids.push_back(u.get_object_id());
        }

        // 应该找到所有5个用户
        ASSERT_EQ(all_ids.size(), 5);
        std::sort(all_ids.begin(), all_ids.end());
        EXPECT_EQ(all_ids[0], 1);
        EXPECT_EQ(all_ids[1], 2);
        EXPECT_EQ(all_ids[2], 3);
        EXPECT_EQ(all_ids[3], 4);
        EXPECT_EQ(all_ids[4], 5);
    }
}

// 测试浮点数索引功能
TEST(database_index_test, float_index_test) {
    // 创建测试用户数据，包含正数、负数和零的分数
    user u1(1, "张三", 20, 85.5);
    user u2(2, "李四", 25, -12.3);
    user u3(3, "王五", 22, 0.0);
    user u4(4, "赵六", 28, 92.7);
    user u5(5, "钱七", 30, -5.8);

    // 创建分数提取器
    auto score_extractor = mc::db::make_key<user, double, &user::score>();

    // 1. 测试正序索引
    {
        auto index = mc::db::make_index<user, true>(score_extractor);

        // 添加用户到索引
        EXPECT_TRUE(index->add(u1));
        EXPECT_TRUE(index->add(u2));
        EXPECT_TRUE(index->add(u3));
        EXPECT_TRUE(index->add(u4));
        EXPECT_TRUE(index->add(u5));

        // 验证正序排序结果
        std::vector<double> scores;
        for (auto& u : *index) {
            scores.push_back(u.m_score);
        }
        EXPECT_EQ(scores, (std::vector<double>{-12.3, -5.8, 0.0, 85.5, 92.7}));

        // 测试查找功能
        auto found = index->find(85.5);
        ASSERT_FALSE(found.is_end());
        EXPECT_EQ(found->get_object_id(), 1);
    }

    // 2. 测试浮点数精度
    {
        user u6(6, "孙八", 35, 1.23456789);
        user u7(7, "周九", 40, 1.23456788);

        auto index = mc::db::make_index<user, true>(score_extractor);

        // 添加用户到索引
        EXPECT_TRUE(index->add(u6));
        EXPECT_TRUE(index->add(u7));

        // 验证浮点数精度
        auto found = index->find(1.23456789);
        ASSERT_FALSE(found.is_end());
        EXPECT_EQ(found->get_object_id(), 6);

        found = index->find(1.23456788);
        ASSERT_FALSE(found.is_end());
        EXPECT_EQ(found->get_object_id(), 7);
    }
}

// 测试浮点数作为非唯一键索引功能
TEST(database_index_test, non_unique_key_test) {
    // 创建测试用户数据，包含相同分数的用户
    user u1(1, "张三", 20, 85.5);
    user u2(2, "李四", 25, 85.5); // 与张三分数相同
    user u3(3, "王五", 22, 92.7);
    user u4(4, "赵六", 28, 92.7); // 与王五分数相同
    user u5(5, "钱七", 30, 0.0);

    // 创建分数提取器，设置为非唯一键
    auto score_extractor = mc::db::make_key<user, double, &user::score>();

    // 1. 测试正序非唯一索引
    {
        auto index = mc::db::make_index<user, false>(score_extractor);

        // 添加用户到索引
        EXPECT_TRUE(index->add(u1));
        EXPECT_TRUE(index->add(u2));
        EXPECT_TRUE(index->add(u3));
        EXPECT_TRUE(index->add(u4));
        EXPECT_TRUE(index->add(u5));

        // 验证正序排序结果
        std::vector<double> scores;
        for (auto& u : *index) {
            scores.push_back(u.m_score);
        }
        EXPECT_EQ(scores, (std::vector<double>{0.0, 85.5, 85.5, 92.7, 92.7}));

        // 测试查找功能 - 查找分数为85.5的用户
        auto found = index->find(85.5);
        ASSERT_FALSE(found.is_end());

        // 应该找到两个分数为85.5的用户
        std::vector<int> found_ids;
        for (; !found.is_end() && found->m_score == 85.5; ++found) {
            found_ids.push_back(found->get_object_id());
        }
        ASSERT_EQ(found_ids.size(), 2);
        std::sort(found_ids.begin(), found_ids.end());
        EXPECT_EQ(found_ids[0], 1); // 张三
        EXPECT_EQ(found_ids[1], 2); // 李四
    }

    // 2. 测试非唯一键的更新操作
    {
        auto index = mc::db::make_index<user, false>(score_extractor);

        // 添加初始用户
        EXPECT_TRUE(index->add(u1));
        EXPECT_TRUE(index->add(u2));

        // 更新用户分数
        user u1_new(1, "张三", 20, 95.0); // 更新张三的分数
        EXPECT_TRUE(index->update(u1, u1_new));

        // 验证更新后的结果
        auto found = index->find(95.0);
        ASSERT_FALSE(found.is_end());
        EXPECT_EQ(found->get_object_id(), 1);

        // 验证原来的分数85.5仍然存在（李四的分数）
        found = index->find(85.5);
        ASSERT_FALSE(found.is_end());
        EXPECT_EQ(found->get_object_id(), 2);
    }

    // 3. 测试非唯一键的删除操作
    {
        auto index = mc::db::make_index<user, false>(score_extractor);

        // 添加用户
        EXPECT_TRUE(index->add(u1));
        EXPECT_TRUE(index->add(u2));
        EXPECT_TRUE(index->add(u3));
        EXPECT_TRUE(index->add(u4));

        // 删除一个分数为85.5的用户
        auto result = index->remove(u1);
        ASSERT_TRUE(result.has_value());

        // 验证另一个分数为85.5的用户仍然存在
        auto found = index->find(85.5);
        ASSERT_FALSE(found.is_end());
        EXPECT_EQ(found->get_object_id(), 2); // 李四仍然存在

        // 验证其他分数为92.7的用户仍然存在
        found = index->find(92.7);
        ASSERT_FALSE(found.is_end());
        std::vector<int> found_ids;
        for (; !found.is_end() && found->m_score == 92.7; ++found) {
            found_ids.push_back(found->get_object_id());
        }
        ASSERT_EQ(found_ids.size(), 2);
        std::sort(found_ids.begin(), found_ids.end());
        EXPECT_EQ(found_ids[0], 3); // 王五
        EXPECT_EQ(found_ids[1], 4); // 赵六
    }
}

// 测试非唯一索引的组合键场景
TEST(database_index_test, non_unique_compound_key_test) {
    // 创建组合键提取器
    using key_extractor = mc::db::detail::composite_key<
        mc::db::detail::member_key<user, int, &user::m_age>,
        mc::db::detail::member_key<user, std::string, &user::m_city>,
        mc::db::detail::member_key<user, std::string, &user::m_department>>;

    // 创建非唯一索引
    auto idx = mc::db::make_index<user, false>(key_extractor());

    // 创建测试数据
    std::vector<user> users = {
        {1, "张三", 25, 0.0, "北京", "研发部"}, {2, "李四", 25, 0.0, "北京", "测试部"},
        {3, "王五", 30, 0.0, "上海", "研发部"}, {4, "赵六", 30, 0.0, "上海", "测试部"},
        {5, "钱七", 35, 0.0, "广州", "研发部"}, {6, "孙八", 35, 0.0, "广州", "测试部"}};

    // 添加数据到索引
    for (const auto& u : users) {
        ASSERT_TRUE(idx->add(u));
    }

    // 测试场景1：按年龄和城市查询
    {
        auto [begin, end] = idx->equal_range(25, "北京");
        std::vector<std::string> names;
        for (auto it = begin; it != end; ++it) {
            names.push_back(it->m_name);
        }
        ASSERT_EQ(names.size(), 2);
        ASSERT_TRUE(std::find(names.begin(), names.end(), "张三") != names.end());
        ASSERT_TRUE(std::find(names.begin(), names.end(), "李四") != names.end());
    }

    // 测试场景2：按年龄查询
    {
        auto [begin, end] = idx->equal_range(30);
        std::vector<std::string> names;
        for (auto it = begin; it != end; ++it) {
            names.push_back(it->m_name);
        }
        ASSERT_EQ(names.size(), 2);
        ASSERT_TRUE(std::find(names.begin(), names.end(), "王五") != names.end());
        ASSERT_TRUE(std::find(names.begin(), names.end(), "赵六") != names.end());
    }
}
