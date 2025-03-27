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

#include "mc/core/plugin.h"
#include "mc/core/service.h"
#include "mc/core/service_factory.h"
#include "mc/log.h"
#include "services/memory_manager/service.h"
#include "services/communication/service.h"
#include "services/object_tree/service.h"
#include "services/reference_tracker/service.h"

namespace mc::database {

/**
 * @brief 数据库插件实现
 * 
 * 该插件提供分布式对象数据库功能，包含以下服务：
 * - 内存管理服务：负责共享内存分配和管理
 * - 通信服务：负责客户端与服务端的消息传递
 * - 对象树服务：负责管理分布式对象的层次结构
 * - 引用跟踪服务：负责跟踪对象引用和管理生命周期
 */
class database_plugin : public plugin {
public:
    // 获取插件信息
    const plugin_info& get_info() const override {
        static plugin_info info {
            "database",  // 名称
            "1.0.0",     // 版本
            {}           // 依赖
        };
        return info;
    }
    
    // 插件初始化方法
    bool init(service_factory& factory) override {
        ilog("初始化数据库插件");
        
        // 注册服务类型
        factory.register_service<memory_manager_service>("memory_manager");
        factory.register_service<communication_service>("communication");
        factory.register_service<object_tree_service>("object_tree");
        factory.register_service<reference_tracker_service>("reference_tracker");
        
        return true;
    }
};

} // namespace mc::database

// 导出插件
MC_EXPORT_PLUGIN(mc::database::database_plugin) 