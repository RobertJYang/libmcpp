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

#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM
#include <mc/engine/shm_object.h>
#include <mc/engine/shm_object_ops.h>
#include <mc/engine/shm_property_sync.h>
#include <mc/log/log.h>
#endif

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

#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM

mc::shared_ptr<abstract_object> try_reconstruct_object(shm_object* sh) noexcept
{
    if (sh == nullptr) {
        return {};
    }

    if (!shm_object_check(*sh)) {
        sh->flags |= shm_object_flags::isolated;
        wlog("shm_object CRC 校验失败 → 隔离 object_id=${id}", ("id", sh->object_id));
        return {};
    }

    auto cn = shm_object_class_name(*sh);
    if (cn.empty()) {
        sh->flags |= shm_object_flags::isolated;
        wlog("shm_object class_name 为空 → 隔离 object_id=${id}", ("id", sh->object_id));
        return {};
    }

    auto obj = try_create_abstract_object(cn);
    if (!obj) {
        sh->flags |= shm_object_flags::isolated;
        wlog("class 未注册 → 隔离 class_name=${c} object_id=${id}",
             ("c", std::string(cn))("id", sh->object_id));
        return {};
    }

    try {
        obj->set_shm_handle(sh);
        obj->set_object_id(sh->object_id);
        obj->set_object_name(std::string(shm_object_name(*sh)));
        obj->set_object_path(std::string(shm_object_path(*sh)));
        obj->set_position(std::string(shm_object_position(*sh)));

        // property 反向回填：sh.properties → obj 内 property<T> 实例
        shm_load_properties_into(*obj, *sh);
    } catch (const std::exception& e) {
        sh->flags |= shm_object_flags::isolated;
        wlog("reconstruct 异常 → 隔离 class_name=${c} object_id=${id} err=${e}",
             ("c", std::string(cn))("id", sh->object_id)("e", e.what()));
        return {};
    } catch (...) {
        sh->flags |= shm_object_flags::isolated;
        wlog("reconstruct 未知异常 → 隔离 class_name=${c} object_id=${id}",
             ("c", std::string(cn))("id", sh->object_id));
        return {};
    }

    return obj;
}

#endif  // MCENGINE_USE_SHM

}  // namespace mc::engine
