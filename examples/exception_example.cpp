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

#include <mc/exception.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

// 自定义异常类型示例
class config_exception : public mc::exception {
public:
    enum code_enum {
        code_value = 200, // 自定义异常代码
    };
    
    config_exception(mc::log_message&& msg = mc::log_message(mc::log_level::error, "配置错误"))
        : exception(std::move(msg), code_value, "config_error", "配置文件错误")
    {
    }
    
    config_exception(const config_exception& e) : exception(e) {}
    config_exception(config_exception&& e) : exception(std::move(e)) {}
    
    // 从基类构造
    explicit config_exception(const mc::exception& e) : exception(e) {
        // 在示例中不直接访问m_impl
    }
    
    virtual std::shared_ptr<mc::exception> dynamic_copy_exception() const override {
        return std::make_shared<config_exception>(*this);
    }
    
    virtual void dynamic_rethrow_exception() const override {
        throw *this;
    }
};

// 模拟配置文件读取函数
void read_config_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        // 使用文件未找到异常
        MC_THROW(mc::file_not_found_exception, "无法打开配置文件: " + filename);
    }
    
    // 模拟解析配置文件
    std::string line;
    int line_number = 0;
    while (std::getline(file, line)) {
        line_number++;
        
        // 检查配置行格式
        if (!line.empty() && line[0] != '#') {
            size_t pos = line.find('=');
            if (pos == std::string::npos) {
                // 使用解析错误异常
                mc::log_message msg(mc::log_level::error, 
                                   "配置行格式错误，缺少'='符号", 
                                   filename, 
                                   line_number);
                throw mc::parse_error_exception(std::move(msg));
            }
        }
    }
}

// 模拟配置处理函数
void process_config(const std::string& filename) {
    try {
        read_config_file(filename);
        std::cout << "配置文件处理成功！" << std::endl;
    } catch (const mc::file_not_found_exception& e) {
        // 捕获特定异常
        std::cerr << "错误: " << e.to_string() << std::endl;
        
        // 重新抛出自定义异常
        MC_THROW(config_exception, "配置初始化失败: " + std::string(e.what()));
    } catch (const mc::parse_error_exception& e) {
        // 捕获解析错误并添加上下文信息
        mc::exception copy = e;
        MC_RETHROW_EXCEPTION(copy, "配置文件格式无效");
    } catch (const mc::exception& e) {
        // 捕获所有MC库异常
        std::cerr << "MC异常: " << e.to_detail_string() << std::endl;
    } catch (const std::exception& e) {
        // 捕获并包装标准库异常
        mc::std_exception_wrapper wrapper = mc::std_exception_wrapper::from_current_exception(e);
        std::cerr << "标准库异常: " << wrapper.to_string() << std::endl;
    } catch (...) {
        // 捕获所有未知异常
        std::cerr << "未知异常" << std::endl;
    }
}

// 模拟参数验证函数
void validate_parameter(int value, int min, int max) {
    // 使用断言宏
    MC_ASSERT(value >= min && value <= max, 
             "参数值 " + std::to_string(value) + 
             " 超出有效范围 [" + std::to_string(min) + 
             ", " + std::to_string(max) + "]");
}

// 模拟字典查找函数
std::string find_in_dictionary(const std::vector<std::pair<std::string, std::string>>& dict, 
                              const std::string& key) {
    for (const auto& pair : dict) {
        if (pair.first == key) {
            return pair.second;
        }
    }
    
    // 使用键未找到异常
    MC_THROW(mc::key_not_found_exception, "键 '" + key + "' 不存在于字典中");
}

// 主函数
int main() {
    std::cout << "MC库异常处理示例" << std::endl;
    std::cout << "===================" << std::endl;
    
    // 注册异常类型
    mc::exception_factory::instance().register_exception<mc::file_not_found_exception>();
    mc::exception_factory::instance().register_exception<mc::parse_error_exception>();
    mc::exception_factory::instance().register_exception<mc::key_not_found_exception>();
    mc::exception_factory::instance().register_exception<config_exception>();
    
    // 示例1: 处理配置文件
    std::cout << "\n示例1: 处理配置文件" << std::endl;
    try {
        process_config("non_existent_config.ini");
    } catch (const config_exception& e) {
        std::cerr << "配置异常: " << e.to_detail_string() << std::endl;
    }
    
    // 示例2: 参数验证
    std::cout << "\n示例2: 参数验证" << std::endl;
    try {
        validate_parameter(15, 1, 10);
    } catch (const mc::assert_exception& e) {
        std::cerr << "断言异常: " << e.to_string() << std::endl;
    }
    
    // 示例3: 字典查找
    std::cout << "\n示例3: 字典查找" << std::endl;
    try {
        std::vector<std::pair<std::string, std::string>> dictionary = {
            {"name", "张三"},
            {"age", "30"},
            {"city", "北京"}
        };
        
        std::string value = find_in_dictionary(dictionary, "email");
        std::cout << "找到值: " << value << std::endl;
    } catch (const mc::exception& e) {
        std::cerr << "MC异常: " << e.to_string() << std::endl;
    }
    
    // 示例4: 捕获标准库异常
    std::cout << "\n示例4: 捕获标准库异常" << std::endl;
    try {
        try {
            std::vector<int> v = {1, 2, 3};
            std::cout << v.at(10) << std::endl; // 会抛出std::out_of_range
        } MC_CAPTURE_AND_WRAP_EXCEPTION("访问向量元素时");
    } catch (const mc::std_exception_wrapper& e) {
        std::cerr << "包装的标准异常: " << e.to_detail_string() << std::endl;
    }
    
    return 0;
} 