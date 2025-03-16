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

#include "mc/core/config_manager.h"
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <boost/program_options.hpp>
#include <iostream>

namespace mc {
namespace test {

namespace po = boost::program_options;

// 辅助函数：创建临时配置文件
std::string create_temp_config_file(const std::string& content) {
    std::string temp_file = "temp_config.ini";
    std::ofstream file(temp_file);
    file << content;
    file.close();
    return temp_file;
}

// 配置文件加载测试
TEST(ConfigManagerTest, ConfigFileLoading) {
    // 创建一个临时配置文件
    std::string config_content = 
        "test_option = test_value\n"
        "int_option = 123\n"
        "bool_option = true\n";
    
    std::string config_file = create_temp_config_file(config_content);
    
    // 确保测试结束时清理文件
    struct FileGuard {
        std::string filename;
        ~FileGuard() { 
            try {
                if (!filename.empty() && std::filesystem::exists(filename)) {
                    std::filesystem::remove(filename); 
                }
            } catch (...) {}
        }
    } guard{config_file};
    
    // 加载配置文件
    config_manager config;
    
    // 注册将要从配置文件中解析的选项
    config.get_options().cfg.add_options()
        ("test_option", po::value<std::string>(), "测试选项")
        ("int_option", po::value<int>(), "整数选项")
        ("bool_option", po::value<bool>(), "布尔选项");
    
    config.load_config_file(config_file);
    
    // 验证选项正确加载
    EXPECT_TRUE(config.has_option("test_option"));
    EXPECT_EQ(config.get_option<std::string>("test_option"), "test_value");
    
    EXPECT_TRUE(config.has_option("int_option"));
    EXPECT_EQ(config.get_option<int>("int_option"), 123);
    
    EXPECT_TRUE(config.has_option("bool_option"));
    EXPECT_TRUE(config.get_option<bool>("bool_option"));
}

// 静默模式测试
TEST(ConfigManagerTest, SilentMode) {
    config_manager config;
    
    // 设置静默模式
    config.set_silent(true);
    
    // 尝试加载不存在的配置文件，应该不抛出异常
    EXPECT_NO_THROW(config.load_config_file("nonexistent_file.ini"));
    
    // 尝试解析错误的命令行参数
    const char* argv[] = {
        "program",
        "--unknown-option" // 未知选项
    };
    int argc = sizeof(argv) / sizeof(argv[0]);
    
    // 应该返回成功（未知选项被忽略）
    bool result = config.parse_command_line(argc, const_cast<char**>(argv));
    EXPECT_TRUE(result);
}

} // namespace test
} // namespace mc 