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

#ifndef MC_LOG_BACKEND_H
#define MC_LOG_BACKEND_H

#include <mc/log/log_message.h>
#include <string>
#include <memory>

namespace mc {
namespace log {

/**
 * @brief 日志后端接口
 * 
 * 日志后端负责处理日志消息的实际输出
 */
class log_backend {
public:
    /**
     * @brief 虚析构函数
     */
    virtual ~log_backend() = default;
    
    /**
     * @brief 初始化日志后端
     * 
     * @param config 配置字符串，可以是JSON或其他格式
     * @return bool 初始化是否成功
     */
    virtual bool init(const std::string& config) = 0;
    
    /**
     * @brief 写入日志消息
     * 
     * @param msg 日志消息
     */
    virtual void write(const message& msg) = 0;
    
    /**
     * @brief 刷新日志缓冲
     */
    virtual void flush() = 0;
    
    /**
     * @brief 关闭日志后端
     */
    virtual void close() = 0;
    
    /**
     * @brief 获取后端名称
     * 
     * @return std::string 后端名称
     */
    virtual std::string name() const = 0;
};

/**
 * @brief 日志后端创建函数类型（C 风格接口）
 */
using log_backend_creator_c = void* (*)();

/**
 * @brief 日志后端销毁函数类型（C 风格接口）
 */
using log_backend_destroyer_c = void (*)(void*);

/**
 * @brief 日志后端创建函数类型（C++ 风格接口，内部使用）
 */
using log_backend_creator = std::shared_ptr<log_backend> (*)();

/**
 * @brief 日志后端注册宏
 * 
 * 用于在动态库中注册日志后端
 */
#define MC_REGISTER_LOG_BACKEND(backend_class) \
    extern "C" void* create_log_backend_c() { \
        auto ptr = std::make_shared<backend_class>(); \
        return new std::shared_ptr<mc::log::log_backend>(ptr); \
    } \
    \
    extern "C" void destroy_log_backend_c(void* ptr) { \
        auto shared_ptr = static_cast<std::shared_ptr<mc::log::log_backend>*>(ptr); \
        delete shared_ptr; \
    }

} // namespace log
} // namespace mc

#endif // MC_LOG_BACKEND_H 