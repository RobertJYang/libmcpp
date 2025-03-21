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
 * @file test_path_iterator.cpp
 * @brief 路径迭代器单元测试
 */

#include <gtest/gtest.h>
#include <mc/database/path_iterator.h>
#include <vector>
#include <string>
#include <chrono>

using namespace mc::database;

// 辅助函数：收集路径所有片段
std::vector<std::string> collect_segments(const std::string& path) {
    std::vector<std::string> segments;
    path_iterator it(path);
    
    while (it.to_next()) {
        segments.emplace_back(std::string(it.current()));
    }
    
    return segments;
}

// 测试基本路径导航
TEST(path_iterator_test, basic_navigation) {
    path_iterator it("/a/b/c");
    
    // 初始状态应该不指向任何段
    EXPECT_EQ(it.current(), "");
    
    // 导航到第一段
    EXPECT_TRUE(it.to_next());
    EXPECT_EQ(it.current(), "a");
    
    // 导航到第二段
    EXPECT_TRUE(it.to_next());
    EXPECT_EQ(it.current(), "b");
    
    // 导航到第三段
    EXPECT_TRUE(it.to_next());
    EXPECT_EQ(it.current(), "c");
    
    // 尝试再前进，应该返回false并停留在最后一段
    EXPECT_FALSE(it.to_next());
    EXPECT_EQ(it.current(), "c");
    
    // 导航回第二段
    EXPECT_TRUE(it.to_prev());
    EXPECT_EQ(it.current(), "b");
    
    // 导航回第一段
    EXPECT_TRUE(it.to_prev());
    EXPECT_EQ(it.current(), "a");
    
    EXPECT_FALSE(it.to_prev());
}

// 测试不同类型的路径
TEST(path_iterator_test, different_paths) {
    // 测试空路径
    EXPECT_EQ(collect_segments(""), std::vector<std::string>{});
    
    // 测试根路径
    EXPECT_EQ(collect_segments("/"), std::vector<std::string>{});
    
    // 测试单段路径
    EXPECT_EQ(collect_segments("/a"), std::vector<std::string>{"a"});
    EXPECT_EQ(collect_segments("a"), std::vector<std::string>{"a"});
    
    // 测试多段路径
    EXPECT_EQ(collect_segments("/a/b/c"), 
              (std::vector<std::string>{"a", "b", "c"}));
    EXPECT_EQ(collect_segments("a/b/c"),
              (std::vector<std::string>{"a", "b", "c"}));
}

// 测试特殊情况
TEST(path_iterator_test, special_cases) {
    // 路径末尾带斜杠
    EXPECT_EQ(collect_segments("/a/"), std::vector<std::string>{"a"});
    EXPECT_EQ(collect_segments("/a/b/"), (std::vector<std::string>{"a", "b"}));
    
    // 连续斜杠
    EXPECT_EQ(collect_segments("//a"), std::vector<std::string>{"a"});
    EXPECT_EQ(collect_segments("/a//b"), (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(collect_segments("/a///b//c/"), 
              (std::vector<std::string>{"a", "b", "c"}));
    
    // 复杂路径
    EXPECT_EQ(collect_segments("//a/b//c/d///"),
              (std::vector<std::string>{"a", "b", "c", "d"}));
}

// 测试重置功能
TEST(path_iterator_test, reset_functionality) {
    path_iterator it("/a/b/c");
    
    // 导航到最后一段
    EXPECT_TRUE(it.to_next());  // a
    EXPECT_TRUE(it.to_next());  // b
    EXPECT_TRUE(it.to_next());  // c
    EXPECT_EQ(it.current(), "c");
    
    // 重置迭代器
    it.reset();
    EXPECT_EQ(it.current(), "");
    
    // 重新导航应该从头开始
    EXPECT_TRUE(it.to_next());
    EXPECT_EQ(it.current(), "a");
}

// 测试路径导航的双向性
TEST(path_iterator_test, bidirectional_navigation) {
    path_iterator it("/one/two/three/four");
    
    // 向前导航
    EXPECT_TRUE(it.to_next());  // one
    EXPECT_TRUE(it.to_next());  // two
    EXPECT_TRUE(it.to_next());  // three
    EXPECT_EQ(it.current(), "three");
    
    // 向后导航
    EXPECT_TRUE(it.to_prev());  // two
    EXPECT_EQ(it.current(), "two");
    
    // 向前向后交替
    EXPECT_TRUE(it.to_next());  // three
    EXPECT_TRUE(it.to_next());  // four
    EXPECT_FALSE(it.to_next()); // 已到末尾
    EXPECT_TRUE(it.to_prev());  // three
    EXPECT_TRUE(it.to_prev());  // two
    EXPECT_TRUE(it.to_prev());  // one
    EXPECT_FALSE(it.to_prev()); // 已到开头
    
    EXPECT_EQ(it.current(), "one");
}

// 测试路径中包含特殊字符的情况
TEST(path_iterator_test, special_characters) {
    path_iterator it("/path.with/special_chars/and-symbols");
    
    EXPECT_TRUE(it.to_next());
    EXPECT_EQ(it.current(), "path.with");
    
    EXPECT_TRUE(it.to_next());
    EXPECT_EQ(it.current(), "special_chars");
    
    EXPECT_TRUE(it.to_next());
    EXPECT_EQ(it.current(), "and-symbols");
    
    EXPECT_FALSE(it.to_next());
}

// 测试中文路径
TEST(path_iterator_test, unicode_paths) {
    path_iterator it("/中文/路径/测试");
    
    EXPECT_TRUE(it.to_next());
    EXPECT_EQ(it.current(), "中文");
    
    EXPECT_TRUE(it.to_next());
    EXPECT_EQ(it.current(), "路径");
    
    EXPECT_TRUE(it.to_next());
    EXPECT_EQ(it.current(), "测试");
    
    EXPECT_FALSE(it.to_next());
}

// 测试边界情况的处理
TEST(path_iterator_test, boundary_conditions) {
    // 空路径
    path_iterator empty_it("");
    EXPECT_FALSE(empty_it.to_next());
    EXPECT_FALSE(empty_it.to_prev());
    EXPECT_EQ(empty_it.current(), "");
    
    // 只有斜杠的路径
    path_iterator root_it("/");
    EXPECT_FALSE(root_it.to_next());
    EXPECT_FALSE(root_it.to_prev());
    EXPECT_EQ(root_it.current(), "");
    
    // 单段路径
    path_iterator single_it("single");
    EXPECT_TRUE(single_it.to_next());
    EXPECT_EQ(single_it.current(), "single");
    EXPECT_FALSE(single_it.to_next());
    EXPECT_FALSE(single_it.to_prev());
}

// 测试基本功能
TEST(PathIterator, BasicFunctionality) {
    // 测试空路径
    {
        path_iterator iter("");
        EXPECT_FALSE(iter.to_next());
        EXPECT_TRUE(iter.current().empty());
    }
    
    // 测试根路径
    {
        path_iterator iter("/");
        EXPECT_FALSE(iter.to_next());
        EXPECT_TRUE(iter.current().empty());
    }
    
    // 测试单段路径
    {
        path_iterator iter("test");
        EXPECT_TRUE(iter.to_next());
        EXPECT_EQ(iter.current(), "test");
        EXPECT_FALSE(iter.to_next());
    }
    
    // 测试多段路径
    {
        path_iterator iter("/a/b/c");
        
        EXPECT_TRUE(iter.to_next());
        EXPECT_EQ(iter.current(), "a");
        
        EXPECT_TRUE(iter.to_next());
        EXPECT_EQ(iter.current(), "b");
        
        EXPECT_TRUE(iter.to_next());
        EXPECT_EQ(iter.current(), "c");
        
        EXPECT_FALSE(iter.to_next());
    }
    
    // 测试连续斜杠
    {
        path_iterator iter("/a//b///c");
        
        EXPECT_TRUE(iter.to_next());
        EXPECT_EQ(iter.current(), "a");
        
        EXPECT_TRUE(iter.to_next());
        EXPECT_EQ(iter.current(), "b");
        
        EXPECT_TRUE(iter.to_next());
        EXPECT_EQ(iter.current(), "c");
        
        EXPECT_FALSE(iter.to_next());
    }
    
    // 测试尾部斜杠
    {
        path_iterator iter("/a/b/c/");
        
        EXPECT_TRUE(iter.to_next());
        EXPECT_EQ(iter.current(), "a");
        
        EXPECT_TRUE(iter.to_next());
        EXPECT_EQ(iter.current(), "b");
        
        EXPECT_TRUE(iter.to_next());
        EXPECT_EQ(iter.current(), "c");
        
        EXPECT_FALSE(iter.to_next());
    }
}

// 测试反向遍历
TEST(PathIterator, ReverseTraversal) {
    path_iterator iter("/a/b/c/d");
    
    // 向前遍历到最后
    EXPECT_TRUE(iter.to_next());  // a
    EXPECT_TRUE(iter.to_next());  // b
    EXPECT_TRUE(iter.to_next());  // c
    EXPECT_TRUE(iter.to_next());  // d
    EXPECT_FALSE(iter.to_next()); // 已到末尾
    EXPECT_EQ(iter.current(), "d");
    
    // 向后遍历
    EXPECT_TRUE(iter.to_prev());  // c
    EXPECT_EQ(iter.current(), "c");
    
    EXPECT_TRUE(iter.to_prev());  // b
    EXPECT_EQ(iter.current(), "b");
    
    EXPECT_TRUE(iter.to_prev());  // a
    EXPECT_EQ(iter.current(), "a");
    
    EXPECT_FALSE(iter.to_prev()); // 已到开头
    EXPECT_EQ(iter.current(), "a");
}

// 测试重置功能
TEST(PathIterator, Reset) {
    path_iterator iter("/a/b/c");
    
    // 向前遍历
    EXPECT_TRUE(iter.to_next());  // a
    EXPECT_TRUE(iter.to_next());  // b
    EXPECT_EQ(iter.current(), "b");
    
    // 重置
    iter.reset();
    
    // 再次遍历
    EXPECT_TRUE(iter.to_next());  // a
    EXPECT_EQ(iter.current(), "a");
}

// 性能测试：对比不同路径长度的解析性能
TEST(PathIterator, PerformanceBenchmark) {
    // 创建不同长度的路径
    std::vector<std::string> paths = {
        "/a",
        "/a/b/c",
        "/a/b/c/d/e",
        "/a/b/c/d/e/f/g/h/i/j",
        "/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t"
    };
    
    const int iterations = 10000;
    
    // 测试每个路径的解析性能
    for (const auto& path : paths) {
        // 使用path_iterator
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            path_iterator iter(path);
            std::vector<std::string> segments;
            
            while (iter.to_next()) {
                segments.emplace_back(std::string(iter.current()));
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
        
        // 字符串分割性能对照组（模拟简单的split实现）
        auto start_time2 = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            std::vector<std::string> segments;
            std::string p = path;
            
            if (!p.empty() && p[0] == '/') {
                p = p.substr(1);
            }
            
            size_t start = 0;
            size_t end = p.find('/');
            
            while (end != std::string::npos) {
                segments.push_back(p.substr(start, end - start));
                start = end + 1;
                end = p.find('/', start);
            }
            
            if (start < p.size()) {
                segments.push_back(p.substr(start));
            }
        }
        
        auto end_time2 = std::chrono::high_resolution_clock::now();
        auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end_time2 - start_time2).count();
        
        // 输出性能比较结果（不做断言，仅作参考）
        std::cout << "Path [" << path << "] with " << (std::count(path.begin(), path.end(), '/') + (path.back() != '/' ? 1 : 0))
                  << " segments:" << std::endl;
        std::cout << "  path_iterator: " << duration << " us for " << iterations << " iterations" << std::endl;
        std::cout << "  string split:  " << duration2 << " us for " << iterations << " iterations" << std::endl;
        std::cout << "  Performance ratio: " << (duration2 > 0 ? (double)duration / duration2 : 0) << std::endl;
        std::cout << std::endl;
    }
}

// 测试边缘情况
TEST(PathIterator, EdgeCases) {
    // 测试特殊字符
    {
        path_iterator iter("/special/chars/!@#$%^&*()");
        
        EXPECT_TRUE(iter.to_next());
        EXPECT_EQ(iter.current(), "special");
        
        EXPECT_TRUE(iter.to_next());
        EXPECT_EQ(iter.current(), "chars");
        
        EXPECT_TRUE(iter.to_next());
        EXPECT_EQ(iter.current(), "!@#$%^&*()");
        
        EXPECT_FALSE(iter.to_next());
    }
    
    // 测试长路径段
    {
        std::string long_segment(1000, 'a');
        std::string path = "/short/" + long_segment + "/end";
        
        path_iterator iter(path);
        
        EXPECT_TRUE(iter.to_next());
        EXPECT_EQ(iter.current(), "short");
        
        EXPECT_TRUE(iter.to_next());
        EXPECT_EQ(iter.current(), long_segment);
        
        EXPECT_TRUE(iter.to_next());
        EXPECT_EQ(iter.current(), "end");
        
        EXPECT_FALSE(iter.to_next());
    }
}
