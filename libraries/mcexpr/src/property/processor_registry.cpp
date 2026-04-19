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

#include <mc/engine/property/processor.h>
#include <mc/engine/property/processors/function_call_processor.h>
#include <mc/engine/property/processors/ref_object_processor.h>
#include <mc/engine/property/processors/ref_property_processor.h>
#include <mc/engine/property/processors/sync_property_processor.h>

namespace mc::engine {

namespace {
bool g_processors_registered = false;
}

/**
 * @brief 注册所有属性处理器
 *
 * 这个函数在系统启动时被调用，注册所有可用的属性处理器
 */
MC_API void register_property_processors()
{
    if (g_processors_registered) {
        return;
    }

    auto& factory = property_processor_factory::get_instance();

    // 注册处理器，按照优先级顺序（从最具体到最一般）
    factory.register_processor(std::make_unique<ref_object_processor>());
    factory.register_processor(std::make_unique<ref_property_processor>());
    factory.register_processor(std::make_unique<sync_property_processor>());
    factory.register_processor(std::make_unique<function_call_processor>());

    g_processors_registered = true;
}

/**
 * @brief 自动注册类
 *
 * 利用全局对象构造函数自动注册处理器
 */
class auto_register_processors {
public:
    auto_register_processors()
    {
        register_property_processors();
    }
};

// 静态实例，确保在程序启动时自动注册处理器
static auto_register_processors g_auto_register;

} // namespace mc::engine