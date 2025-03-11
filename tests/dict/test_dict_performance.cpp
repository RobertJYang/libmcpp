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
 * @file test_dict_performance.cpp
 * @brief 测试 dict 和 mutable_dict 类的性能
 */
#include <gtest/gtest.h>
#include <mc/dict.h>
#include <mc/variant.h>
#include <chrono>
#include <random>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>

using namespace mc;

// 辅助函数：生成随机字符串
std::string random_string(size_t length) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);
    
    std::string result;
    result.reserve(length);
    
    for (size_t i = 0; i < length; ++i) {
        result += alphanum[dis(gen)];
    }
    
    return result;
}

// 辅助函数：生成随机键值对
std::vector<std::pair<std::string, variant>> generate_random_pairs(size_t count) {
    std::vector<std::pair<std::string, variant>> pairs;
    pairs.reserve(count);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> type_dis(0, 3);
    
    for (size_t i = 0; i < count; ++i) {
        std::string key = "key_" + std::to_string(i);
        variant value;
        
        switch (type_dis(gen)) {
            case 0:
                value = static_cast<int>(i);
                break;
            case 1:
                value = random_string(10);
                break;
            case 2:
                value = (i % 2 == 0);
                break;
            case 3:
                value = static_cast<double>(i) / 10.0;
                break;
        }
        
        pairs.emplace_back(key, value);
    }
    
    return pairs;
}

// 测试 dict 和 std::map 的插入性能
TEST(DictPerformanceTest, DISABLED_InsertionPerformance) {
    const size_t count = 10000;
    auto pairs = generate_random_pairs(count);
    
    // 测试 mutable_dict 插入性能
    auto start = std::chrono::high_resolution_clock::now();
    
    mutable_dict md;
    for (const auto& pair : pairs) {
        md[pair.first] = pair.second;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto md_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // 测试 std::map 插入性能
    start = std::chrono::high_resolution_clock::now();
    
    std::map<std::string, variant> m;
    for (const auto& pair : pairs) {
        m[pair.first] = pair.second;
    }
    
    end = std::chrono::high_resolution_clock::now();
    auto map_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // 测试 std::unordered_map 插入性能
    start = std::chrono::high_resolution_clock::now();
    
    std::unordered_map<std::string, variant> um;
    for (const auto& pair : pairs) {
        um[pair.first] = pair.second;
    }
    
    end = std::chrono::high_resolution_clock::now();
    auto umap_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // 输出性能结果
    std::cout << "Insertion performance for " << count << " elements:" << std::endl;
    std::cout << "mutable_dict: " << md_duration << " ms" << std::endl;
    std::cout << "std::map: " << map_duration << " ms" << std::endl;
    std::cout << "std::unordered_map: " << umap_duration << " ms" << std::endl;
    
    // 验证插入结果
    EXPECT_EQ(md.size(), count);
    EXPECT_EQ(m.size(), count);
    EXPECT_EQ(um.size(), count);
}

// 测试 dict 和 std::map 的查找性能
TEST(DictPerformanceTest, DISABLED_LookupPerformance) {
    const size_t count = 10000;
    auto pairs = generate_random_pairs(count);
    
    // 准备测试数据
    mutable_dict md;
    std::map<std::string, variant> m;
    std::unordered_map<std::string, variant> um;
    
    for (const auto& pair : pairs) {
        md[pair.first] = pair.second;
        m[pair.first] = pair.second;
        um[pair.first] = pair.second;
    }
    
    // 随机选择一些键进行查找
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, count - 1);
    
    const size_t lookup_count = 1000;
    std::vector<std::string> lookup_keys;
    lookup_keys.reserve(lookup_count);
    
    for (size_t i = 0; i < lookup_count; ++i) {
        lookup_keys.push_back("key_" + std::to_string(dis(gen)));
    }
    
    // 测试 dict 查找性能
    auto start = std::chrono::high_resolution_clock::now();
    
    for (const auto& key : lookup_keys) {
        if (md.contains(key)) {
            variant v = md[key];
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto md_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    // 测试 std::map 查找性能
    start = std::chrono::high_resolution_clock::now();
    
    for (const auto& key : lookup_keys) {
        auto it = m.find(key);
        if (it != m.end()) {
            variant v = it->second;
        }
    }
    
    end = std::chrono::high_resolution_clock::now();
    auto map_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    // 测试 std::unordered_map 查找性能
    start = std::chrono::high_resolution_clock::now();
    
    for (const auto& key : lookup_keys) {
        auto it = um.find(key);
        if (it != um.end()) {
            variant v = it->second;
        }
    }
    
    end = std::chrono::high_resolution_clock::now();
    auto umap_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    // 输出性能结果
    std::cout << "Lookup performance for " << lookup_count << " operations:" << std::endl;
    std::cout << "dict: " << md_duration << " us" << std::endl;
    std::cout << "std::map: " << map_duration << " us" << std::endl;
    std::cout << "std::unordered_map: " << umap_duration << " us" << std::endl;
}

// 测试 dict 和 std::map 的迭代性能
TEST(DictPerformanceTest, DISABLED_IterationPerformance) {
    const size_t count = 10000;
    auto pairs = generate_random_pairs(count);
    
    // 准备测试数据
    mutable_dict md;
    std::map<std::string, variant> m;
    std::unordered_map<std::string, variant> um;
    
    for (const auto& pair : pairs) {
        md[pair.first] = pair.second;
        m[pair.first] = pair.second;
        um[pair.first] = pair.second;
    }
    
    // 测试 dict 迭代性能
    auto start = std::chrono::high_resolution_clock::now();
    
    size_t sum = 0;
    for (const auto& entry : md) {
        sum += entry.key.size();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto md_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    // 测试 std::map 迭代性能
    start = std::chrono::high_resolution_clock::now();
    
    sum = 0;
    for (const auto& pair : m) {
        sum += pair.first.size();
    }
    
    end = std::chrono::high_resolution_clock::now();
    auto map_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    // 测试 std::unordered_map 迭代性能
    start = std::chrono::high_resolution_clock::now();
    
    sum = 0;
    for (const auto& pair : um) {
        sum += pair.first.size();
    }
    
    end = std::chrono::high_resolution_clock::now();
    auto umap_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    // 输出性能结果
    std::cout << "Iteration performance for " << count << " elements:" << std::endl;
    std::cout << "dict: " << md_duration << " us" << std::endl;
    std::cout << "std::map: " << map_duration << " us" << std::endl;
    std::cout << "std::unordered_map: " << umap_duration << " us" << std::endl;
    
    // 防止编译器优化掉循环
    EXPECT_GT(sum, 0);
}

// 测试 dict 和 std::map 的删除性能
TEST(DictPerformanceTest, DISABLED_ErasurePerformance) {
    const size_t count = 10000;
    auto pairs = generate_random_pairs(count);
    
    // 准备测试数据
    mutable_dict md;
    std::map<std::string, variant> m;
    std::unordered_map<std::string, variant> um;
    
    for (const auto& pair : pairs) {
        md[pair.first] = pair.second;
        m[pair.first] = pair.second;
        um[pair.first] = pair.second;
    }
    
    // 随机选择一些键进行删除
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, count - 1);
    
    const size_t erase_count = 1000;
    std::vector<std::string> erase_keys;
    erase_keys.reserve(erase_count);
    
    for (size_t i = 0; i < erase_count; ++i) {
        erase_keys.push_back("key_" + std::to_string(dis(gen)));
    }
    
    // 测试 mutable_dict 删除性能
    auto start = std::chrono::high_resolution_clock::now();
    
    for (const auto& key : erase_keys) {
        md.erase(key);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto md_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    // 测试 std::map 删除性能
    start = std::chrono::high_resolution_clock::now();
    
    for (const auto& key : erase_keys) {
        m.erase(key);
    }
    
    end = std::chrono::high_resolution_clock::now();
    auto map_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    // 测试 std::unordered_map 删除性能
    start = std::chrono::high_resolution_clock::now();
    
    for (const auto& key : erase_keys) {
        um.erase(key);
    }
    
    end = std::chrono::high_resolution_clock::now();
    auto umap_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    // 输出性能结果
    std::cout << "Erasure performance for " << erase_count << " operations:" << std::endl;
    std::cout << "mutable_dict: " << md_duration << " us" << std::endl;
    std::cout << "std::map: " << map_duration << " us" << std::endl;
    std::cout << "std::unordered_map: " << umap_duration << " us" << std::endl;
} 