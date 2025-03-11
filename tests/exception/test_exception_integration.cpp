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
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <map>

// 变体模块异常类
class variant_exception : public mc::exception {
public:
    enum code_enum {
        code_value = mc::variant_error_code,
    };
    
    variant_exception(mc::log_message&& msg = mc::log_message(mc::log_level::error, "变体类型错误"))
        : exception(std::move(msg), mc::variant_error_code, "variant_error", "变体类型错误")
    {
    }
    
    variant_exception(const variant_exception& e) : exception(e) {}
    variant_exception(variant_exception&& e) : exception(std::move(e)) {}
    
    // 从基类构造
    explicit variant_exception(const mc::exception& e) : exception(e) {
        // 在测试中不直接访问m_impl
    }
    
    virtual std::shared_ptr<mc::exception> dynamic_copy_exception() const override {
        return std::make_shared<variant_exception>(*this);
    }
    
    virtual void dynamic_rethrow_exception() const override {
        throw *this;
    }
};

// 字典模块异常类
class dict_exception : public mc::exception {
public:
    enum code_enum {
        code_value = mc::dict_error_code,
    };
    
    dict_exception(mc::log_message&& msg = mc::log_message(mc::log_level::error, "字典错误"))
        : exception(std::move(msg), mc::dict_error_code, "dict_error", "字典操作错误")
    {
    }
    
    dict_exception(const dict_exception& e) : exception(e) {}
    dict_exception(dict_exception&& e) : exception(std::move(e)) {}
    
    // 从基类构造
    explicit dict_exception(const mc::exception& e) : exception(e) {
        // 在测试中不直接访问m_impl
    }
    
    virtual std::shared_ptr<mc::exception> dynamic_copy_exception() const override {
        return std::make_shared<dict_exception>(*this);
    }
    
    virtual void dynamic_rethrow_exception() const override {
        throw *this;
    }
};

// 模拟变体模块的函数
void variant_convert(const std::string& type) {
    if (type != "int" && type != "string" && type != "bool") {
        MC_THROW(variant_exception, "不支持的类型转换: " + type);
    }
}

// 模拟字典模块的函数
std::string dict_get(const std::map<std::string, std::string>& dict, const std::string& key) {
    auto it = dict.find(key);
    if (it == dict.end()) {
        MC_THROW(dict_exception, "键不存在: " + key);
    }
    return it->second;
}

// 测试变体模块异常
TEST(ExceptionIntegrationTest, VariantExceptionTest) {
    // 注册变体异常类型
    mc::exception_factory::instance().register_exception<variant_exception>();
    
    // 测试变体转换函数
    try {
        variant_convert("float");
        FAIL() << "应该抛出变体异常";
    } catch (const variant_exception& e) {
        EXPECT_EQ(e.code(), mc::variant_error_code);
        EXPECT_STREQ(e.name(), "variant_error");
        EXPECT_TRUE(e.to_string().find("不支持的类型转换: float") != std::string::npos);
    }
    
    // 测试通过异常工厂重新抛出
    try {
        try {
            variant_convert("double");
        } catch (const mc::exception& e) {
            mc::exception_factory::instance().rethrow(e);
        }
        FAIL() << "应该重新抛出变体异常";
    } catch (const variant_exception& e) {
        EXPECT_EQ(e.code(), mc::variant_error_code);
    }
}

// 测试字典模块异常
TEST(ExceptionIntegrationTest, DictExceptionTest) {
    // 注册字典异常类型
    mc::exception_factory::instance().register_exception<dict_exception>();
    
    // 创建测试字典
    std::map<std::string, std::string> test_dict = {
        {"name", "张三"},
        {"age", "30"}
    };
    
    // 测试字典获取函数
    try {
        dict_get(test_dict, "address");
        FAIL() << "应该抛出字典异常";
    } catch (const dict_exception& e) {
        EXPECT_EQ(e.code(), mc::dict_error_code);
        EXPECT_STREQ(e.name(), "dict_error");
        EXPECT_TRUE(e.to_string().find("键不存在: address") != std::string::npos);
    }
    
    // 测试通过异常工厂重新抛出
    try {
        try {
            dict_get(test_dict, "phone");
        } catch (const mc::exception& e) {
            mc::exception_factory::instance().rethrow(e);
        }
        FAIL() << "应该重新抛出字典异常";
    } catch (const dict_exception& e) {
        EXPECT_EQ(e.code(), mc::dict_error_code);
    }
}

// 测试模块间异常传递
TEST(ExceptionIntegrationTest, CrossModuleExceptionTest) {
    // 模拟跨模块调用
    auto cross_module_function = [](const std::string& key, const std::string& type) {
        std::map<std::string, std::string> config = {
            {"int_value", "123"},
            {"string_value", "hello"}
        };
        
        try {
            // 先从字典获取值
            std::string value = dict_get(config, key);
            
            // 然后尝试转换类型
            variant_convert(type);
            
            return value;
        } catch (const dict_exception& e) {
            // 捕获字典异常并添加上下文信息
            mc::exception cross_module = e;
            cross_module.append_log(mc::log_message(mc::log_level::error, 
                                                  "配置处理失败", 
                                                  __FILE__, 
                                                  __LINE__));
            throw cross_module;
        } catch (const variant_exception& e) {
            // 捕获变体异常并添加上下文信息
            mc::exception cross_module = e;
            cross_module.append_log(mc::log_message(mc::log_level::error, 
                                                  "类型转换失败", 
                                                  __FILE__, 
                                                  __LINE__));
            throw cross_module;
        }
    };
    
    // 测试字典异常传递
    try {
        cross_module_function("missing_key", "int");
        FAIL() << "应该抛出异常";
    } catch (const mc::exception& e) {
        std::string detail = e.to_detail_string();
        EXPECT_TRUE(detail.find("键不存在: missing_key") != std::string::npos);
        EXPECT_TRUE(detail.find("配置处理失败") != std::string::npos);
    }
    
    // 测试变体异常传递
    try {
        cross_module_function("int_value", "float");
        FAIL() << "应该抛出异常";
    } catch (const mc::exception& e) {
        std::string detail = e.to_detail_string();
        EXPECT_TRUE(detail.find("不支持的类型转换: float") != std::string::npos);
        EXPECT_TRUE(detail.find("类型转换失败") != std::string::npos);
    }
} 