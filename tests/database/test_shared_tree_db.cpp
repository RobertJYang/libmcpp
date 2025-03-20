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

/**
 * @file test_shared_tree_db.cpp
 * @brief 共享树数据库单元测试
 */

#include <gtest/gtest.h>
#include <mc/database/shared_tree_db.h>
#include <mc/variant.h>
#include <thread>
#include <vector>

using namespace mc;
using namespace mc::database;

// 基本创建和访问测试
TEST(SharedTreeDB, BasicCreateAndAccess) {
    // 创建数据库
    auto db = shared_tree_db::create("test_tree_db", 1024 * 1024);
    ASSERT_TRUE(db != nullptr);
    
    // 设置根节点数据
    EXPECT_TRUE(db->set_data("/", "root"));
    EXPECT_EQ(db->get_data("/"), "root");
    
    // 创建节点
    EXPECT_TRUE(db->create_node("/test", "test data"));
    EXPECT_TRUE(db->has_node("/test"));
    EXPECT_EQ(db->get_data("/test"), "test data");
    
    // 访问不存在的节点
    EXPECT_FALSE(db->has_node("/nonexistent"));
    EXPECT_TRUE(db->get_data("/nonexistent").is_null());
}

// 嵌套节点测试
TEST(SharedTreeDB, NestedNodes) {
    auto db = shared_tree_db::create("test_nested_db", 1024 * 1024);
    
    // 创建嵌套节点
    EXPECT_TRUE(db->create_node("/parent", "parent"));
    EXPECT_TRUE(db->create_node("/parent/child", "child"));
    EXPECT_TRUE(db->create_node("/parent/child/grandchild", "grandchild"));
    
    // 验证节点存在
    EXPECT_TRUE(db->has_node("/parent"));
    EXPECT_TRUE(db->has_node("/parent/child"));
    EXPECT_TRUE(db->has_node("/parent/child/grandchild"));
    
    // 验证节点数据
    EXPECT_EQ(db->get_data("/parent"), "parent");
    EXPECT_EQ(db->get_data("/parent/child"), "child");
    EXPECT_EQ(db->get_data("/parent/child/grandchild"), "grandchild");
    
    // 删除中间节点
    EXPECT_TRUE(db->delete_node("/parent/child"));
    
    // 验证删除结果
    EXPECT_TRUE(db->has_node("/parent"));
    EXPECT_FALSE(db->has_node("/parent/child"));
    EXPECT_FALSE(db->has_node("/parent/child/grandchild"));
}

// 复杂数据类型测试
TEST(SharedTreeDB, ComplexDataTypes) {
    auto db = shared_tree_db::create("test_complex_db", 1024 * 1024);
    
    // 存储不同类型的数据
    EXPECT_TRUE(db->create_node("/int", 42));
    EXPECT_TRUE(db->create_node("/double", 3.14159));
    EXPECT_TRUE(db->create_node("/bool", true));
    EXPECT_TRUE(db->create_node("/string", "Hello, world!"));
    
    // 存储字典
    dict config{
        {"server", "localhost"},
        {"port", 8080},
        {"enabled", true},
        {"timeout", 30.5}
    };
    EXPECT_TRUE(db->create_node("/config", config));
    
    // 存储数组
    std::vector<int> numbers{1, 2, 3, 4, 5};
    EXPECT_TRUE(db->create_node("/numbers", variant(numbers)));
    
    // 验证数据
    EXPECT_EQ(db->get_data("/int").as<int>(), 42);
    EXPECT_FLOAT_EQ(db->get_data("/double").as<double>(), 3.14159);
    EXPECT_EQ(db->get_data("/bool").as<bool>(), true);
    EXPECT_EQ(db->get_data("/string").as<std::string>(), "Hello, world!");
    
    // 验证字典和数组
    auto retrieved_config = db->get_data("/config").as<dict>();
    EXPECT_EQ(retrieved_config["server"].as<std::string>(), "localhost");
    EXPECT_EQ(retrieved_config["port"].as<int>(), 8080);
    
    auto retrieved_numbers = db->get_data("/numbers").as<std::vector<int>>();
    ASSERT_EQ(retrieved_numbers.size(), 5);
    EXPECT_EQ(retrieved_numbers[2], 3);
}

// 事务测试
TEST(SharedTreeDB, Transactions) {
    auto db = shared_tree_db::create("test_transaction_db", 1024 * 1024);
    
    // 初始化一些数据
    EXPECT_TRUE(db->create_node("/data", "initial"));
    
    // 创建事务
    auto transaction = db->create_transaction();
    
    // 修改数据
    EXPECT_TRUE(transaction->set_data("/data", "modified"));
    EXPECT_TRUE(transaction->create_node("/new_node", "new data"));
    
    // 验证事务内可见
    EXPECT_EQ(transaction->get_data("/data"), "modified");
    EXPECT_TRUE(transaction->has_node("/new_node"));
    
    // 验证事务外不可见
    EXPECT_EQ(db->get_data("/data"), "initial");
    EXPECT_FALSE(db->has_node("/new_node"));
    
    // 提交事务
    EXPECT_TRUE(transaction->commit());
    
    // 验证变更已提交
    EXPECT_EQ(db->get_data("/data"), "modified");
    EXPECT_TRUE(db->has_node("/new_node"));
}

// 回滚测试
TEST(SharedTreeDB, TransactionRollback) {
    auto db = shared_tree_db::create("test_rollback_db", 1024 * 1024);
    
    // 初始化数据
    EXPECT_TRUE(db->create_node("/data", "initial"));
    
    {
        // 创建事务，并在析构时自动回滚
        auto transaction = db->create_transaction();
        EXPECT_TRUE(transaction->set_data("/data", "modified"));
        EXPECT_TRUE(transaction->create_node("/temp", "temp data"));
        
        // 验证事务内可见
        EXPECT_EQ(transaction->get_data("/data"), "modified");
        EXPECT_TRUE(transaction->has_node("/temp"));
        
        // 析构时自动回滚
    }
    
    // 验证变更已回滚
    EXPECT_EQ(db->get_data("/data"), "initial");
    EXPECT_FALSE(db->has_node("/temp"));
}

// 导出/导入测试
TEST(SharedTreeDB, ExportImport) {
    auto db1 = shared_tree_db::create("test_export_db", 1024 * 1024);
    
    // 创建一些数据
    EXPECT_TRUE(db1->create_node("/a", 1));
    EXPECT_TRUE(db1->create_node("/b", true));
    EXPECT_TRUE(db1->create_node("/c/d", "nested"));
    
    // 导出为dict
    auto exported = db1->export_to_dict();
    
    // 创建新数据库
    auto db2 = shared_tree_db::create("test_import_db", 1024 * 1024);
    
    // 导入数据
    EXPECT_TRUE(db2->import_from_dict(exported));
    
    // 验证导入的数据
    EXPECT_TRUE(db2->has_node("/a"));
    EXPECT_TRUE(db2->has_node("/b"));
    EXPECT_TRUE(db2->has_node("/c/d"));
    
    EXPECT_EQ(db2->get_data("/a"), 1);
    EXPECT_EQ(db2->get_data("/b"), true);
    EXPECT_EQ(db2->get_data("/c/d"), "nested");
}

// 遍历测试
TEST(SharedTreeDB, Traversal) {
    auto db = shared_tree_db::create("test_traverse_db", 1024 * 1024);
    
    // 创建一些数据
    EXPECT_TRUE(db->create_node("/a", 1));
    EXPECT_TRUE(db->create_node("/b", 2));
    EXPECT_TRUE(db->create_node("/c/d", 3));
    EXPECT_TRUE(db->create_node("/c/e", 4));
    
    // 记录遍历的节点
    std::vector<std::string> visited_paths;
    std::vector<int> visited_values;
    
    db->traverse([&](std::string_view path, const variant& data) {
        visited_paths.emplace_back(path);
        if (!data.is_null() && !path.empty() && path != "/") {
            visited_values.push_back(data.as<int>());
        }
    });
    
    // 验证所有节点都被访问
    EXPECT_EQ(visited_paths.size(), 5); // 根节点 + 4个创建的节点
    EXPECT_EQ(visited_values.size(), 4);
    
    // 验证数值总和
    int sum = 0;
    for (int value : visited_values) {
        sum += value;
    }
    EXPECT_EQ(sum, 10); // 1 + 2 + 3 + 4
}

// 并发测试
TEST(SharedTreeDB, ConcurrentAccess) {
    auto db = shared_tree_db::create("test_concurrent_db", 1024 * 1024);
    
    // 初始化一些数据
    EXPECT_TRUE(db->create_node("/counter", 0));
    
    // 创建多个线程，每个线程增加计数器
    constexpr int num_threads = 5;
    constexpr int iterations = 10;
    
    auto increment_counter = [db](int thread_id) {
        for (int i = 0; i < iterations; ++i) {
            // 创建事务
            auto transaction = db->create_transaction();
            
            // 读取当前值
            int current = transaction->get_data("/counter").as<int>();
            
            // 模拟一些处理时间
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            
            // 增加并写回
            transaction->set_data("/counter", current + 1);
            
            // 创建线程专用节点
            transaction->create_node("/thread_" + std::to_string(thread_id) + "/iteration_" + std::to_string(i), i);
            
            // 提交事务
            transaction->commit();
        }
    };
    
    // 运行线程
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(increment_counter, i);
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证最终计数
    EXPECT_EQ(db->get_data("/counter").as<int>(), num_threads * iterations);
    
    // 验证每个线程都创建了其节点
    for (int i = 0; i < num_threads; ++i) {
        for (int j = 0; j < iterations; ++j) {
            std::string path = "/thread_" + std::to_string(i) + "/iteration_" + std::to_string(j);
            EXPECT_TRUE(db->has_node(path));
            EXPECT_EQ(db->get_data(path).as<int>(), j);
        }
    }
} 