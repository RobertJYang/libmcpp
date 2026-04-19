/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <mc/engine/abstract_object_factory.h>

#include <mutex>
#include <string>
#include <unordered_map>

namespace mc::engine {

namespace {

struct factory_registry {
    std::mutex                                                       mutex;
    std::unordered_map<std::string, abstract_object_factory_fn>      factories;
};

factory_registry& _instance() noexcept
{
    static factory_registry inst;
    return inst;
}

}  // namespace

void register_abstract_object_factory(mc::string_view            class_name,
                                      abstract_object_factory_fn factory) noexcept
{
    if (factory == nullptr || class_name.empty()) {
        return;
    }
    auto&                       reg = _instance();
    std::lock_guard<std::mutex> lock(reg.mutex);
    reg.factories[std::string(class_name)] = factory;
}

abstract_object_factory_fn find_abstract_object_factory(mc::string_view class_name) noexcept
{
    auto&                       reg = _instance();
    std::lock_guard<std::mutex> lock(reg.mutex);
    auto                        it = reg.factories.find(std::string(class_name));
    return it == reg.factories.end() ? nullptr : it->second;
}

mc::shared_ptr<abstract_object> try_create_abstract_object(mc::string_view class_name) noexcept
{
    auto factory = find_abstract_object_factory(class_name);
    if (factory == nullptr) {
        return {};
    }
    return factory();
}

void clear_abstract_object_factories_for_test() noexcept
{
    auto&                       reg = _instance();
    std::lock_guard<std::mutex> lock(reg.mutex);
    reg.factories.clear();
}

}  // namespace mc::engine
