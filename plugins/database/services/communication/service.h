/*
* Copyright (c) 2023 Huawei Technologies Co., Ltd.
* openUBMC is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*         http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

#ifndef MC_DATABASE_COMMUNICATION_SERVICE_H
#define MC_DATABASE_COMMUNICATION_SERVICE_H

#include "mc/core/service.h"
#include "mc/database/common.h"
#include <memory>
#include <functional>
#include <boost/program_options.hpp>

namespace mc::database {

namespace po = boost::program_options;

// 消息类型
enum class message_type {
    client_register,
    client_unregister,
    object_create,
    object_query,
    object_remove,
    list_children,
    object_exists,
    property_update
};

// 消息处理回调类型
using message_handler = std::function<void(client_id, message_type, const void*, size_t, void*, size_t*)>;

// 通信服务
class communication_service : public service_base<communication_service> {
public:
    communication_service(std::string name = "communication");
    ~communication_service() override;
    
    // 服务接口实现
    bool init(dict args) override;
    bool start() override;
    bool stop() override;
    void cleanup() override;
    bool is_healthy() const override;
    
    // 注册消息处理器
    void register_handler(message_type type, message_handler handler);
    
    // 向客户端发送消息
    void send_message(client_id client, message_type type, const void* data, size_t data_size);
    
    // 注册客户端
    client_id register_client();
    
    // 注销客户端
    void unregister_client(client_id client);
    
    // 注册配置选项
    static void register_options(po::options_description& cli_opts, po::options_description& cfg_opts) {
        cfg_opts.add_options()
            ("communication.max_clients", po::value<size_t>()->default_value(100), 
             "通信服务的最大客户端数量")
            ("communication.timeout_ms", po::value<int>()->default_value(5000), 
             "通信操作超时时间(毫秒)");
    }
    
private:
    class impl;
    std::unique_ptr<impl> m_impl;
};

} // namespace mc::database

#endif // MC_DATABASE_COMMUNICATION_SERVICE_H 