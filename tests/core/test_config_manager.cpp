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

#include "mc/core/json_config_manager.h"
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
    std::string temp_file = "temp_config.json";
    std::ofstream file(temp_file);
    file << content;
    file.close();
    return temp_file;
}

// 配置文件加载测试
TEST(JsonConfigManagerTest, ConfigFileLoading) {
    // 创建一个临时配置文件
    std::string config_content = R"(
[
  {
    "api_version": "v1",
    "kind": "Application",
    "meta": {
      "name": "test-app"
    },
    "plugin_dir": "./test_plugins",
    "plugins": ["test_plugin"],
    "threads": 4
  }
]
)";
    
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
    
    // 创建配置管理器
    json_config_manager config;
    
    // 加载配置文件
    ASSERT_TRUE(config.load_config_file(config_file));
    
    // 使用json_config_manager的特定API验证配置
    EXPECT_EQ(config.get_plugin_dir(), "./test_plugins");
    auto plugin_names = config.get_plugin_names();
    ASSERT_EQ(plugin_names.size(), 1);
    EXPECT_EQ(plugin_names[0], "test_plugin");
    EXPECT_EQ(config.get_thread_count(), 4);
}

// 命令行参数测试
TEST(JsonConfigManagerTest, CommandLineArgs) {
    // 创建一个临时配置文件
    std::string config_content = R"(
[
  {
    "api_version": "v1",
    "kind": "Application",
    "meta": {
      "name": "test-app"
    },
    "plugin_dir": "./default_plugins",
    "plugins": ["default_plugin"],
    "threads": 2
  }
]
)";
    
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
    
    // 创建配置管理器
    json_config_manager config;
    
    // 准备命令行参数
    const char* argv[] = {
        "test_program",
        "--config", config_file.c_str(),
        "--plugin-dir", "./cmd_line_plugins",
        "--plugin", "cmd_line_plugin1",
        "--plugin", "cmd_line_plugin2",
        "--threads", "8"
    };
    int argc = sizeof(argv) / sizeof(argv[0]);
    
    // 解析命令行参数
    ASSERT_TRUE(config.parse_command_line(argc, const_cast<char**>(argv)));
    
    // 加载配置文件
    ASSERT_TRUE(config.load_config_file());
    
    // 验证命令行参数覆盖了配置文件中的值
    EXPECT_EQ(config.get_plugin_dir(), "./cmd_line_plugins");
    
    auto plugin_names = config.get_plugin_names();
    ASSERT_EQ(plugin_names.size(), 2);
    EXPECT_EQ(plugin_names[0], "cmd_line_plugin1");
    EXPECT_EQ(plugin_names[1], "cmd_line_plugin2");
    
    EXPECT_EQ(config.get_thread_count(), 8);
}

} // namespace test
} // namespace mc 