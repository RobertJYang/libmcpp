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

#ifndef MC_LOG_BACKEND_ADAPTER_H
#define MC_LOG_BACKEND_ADAPTER_H

#include <mc/log/appender.h>
#include <mc/log/log_backend.h>
#include <memory>

namespace mc {
namespace log {

/**
 * @brief 后端适配器
 * 
 * 将日志后端包装为追加器
 */
class backend_adapter : public appender {
public:
    /**
     * @brief 构造函数
     * 
     * @param backend 日志后端
     */
    explicit backend_adapter(std::shared_ptr<log_backend> backend)
        : appender(appender_config(backend->name())), m_backend(std::move(backend)) {}
    
    /**
     * @brief 追加日志消息
     * 
     * @param msg 日志消息
     */
    void append(const message& msg) override {
        if (m_backend && static_cast<int>(msg.get_level()) >= static_cast<int>(get_level())) {
            m_backend->write(msg);
        }
    }
    
private:
    std::shared_ptr<log_backend> m_backend; // 日志后端
};

} // namespace log
} // namespace mc

#endif // MC_LOG_BACKEND_ADAPTER_H 