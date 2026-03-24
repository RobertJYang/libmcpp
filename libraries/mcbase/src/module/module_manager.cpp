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
#include <mc/log/log.h>
#include <mc/module.h>
#include <mc/signal_slot.h>
#include <mc/singleton.h>
#include <mc/sync/mutex_box.h>

#include "module/include/module_loader.h"

namespace mc::module {

using factory_ptr     = mc::reflect::factory_ptr;
using factory_wptr    = mc::reflect::factory_wptr;
using factory_id_type = mc::reflect::factory_id_type;
using impl_ptr        = mc::shared_ptr<module_manager_impl>;

class factory_module : public module_base {
public:
    factory_module(mc::string_view name, factory_ptr factory) : m_name(name), m_factory(factory)
    {}

    ~factory_module() override
    {}

    mc::string_view name() const override
    {
        return m_name;
    }

    mc::string_view version() const override
    {
        return "1.0.0"; // 默认版本，可以后续从模块元数据获取
    }

    mc::reflect::reflection_factory* get_factory() const override
    {
        return m_factory.lock().get();
    }

protected:
    mc::string  m_name;
    factory_wptr m_factory;
};

class library_module : public factory_module {
public:
    library_module(mc::string_view name, factory_ptr factory) : factory_module(name, factory)
    {}

    ~library_module() override;

    void set_library_info(library_info_ptr info, impl_ptr impl)
    {
        m_info = info;
        m_impl = impl;
    }

    library_info_ptr get_library_info() const
    {
        return m_info;
    }

private:
    library_info_ptr m_info;
    impl_ptr         m_impl;
};

class module_manager_impl : public mc::enable_shared_from_this<module_manager_impl> {
public:
    using module_map    = std::unordered_map<mc::string_view, module_ptr>;
    using module_id_map = std::unordered_map<factory_id_type, module_ptr>;
    using library_map   = std::unordered_map<mc::string_view, library_info_ptr>;
    struct data_t {
        module_map    loaded_modules;
        module_id_map loaded_modules_by_id;
        library_map   handles;
    };
    using lock_ptr_type = mc::mutex_box<data_t>::w_locked_ptr;

    module_manager_impl();
    ~module_manager_impl();

    // 卸载所有模块（在 reset_for_test 前调用）
    void unload_all();

    void clear();

    module_ptr require(mc::string_view module_name);
    void       unload(mc::string_view module_name);
    void       unload(factory_id_type factory_id);
    void       close_handle(const library_info& info, bool need_unload);

    void remove_library(mc::string_view module_name, lock_ptr_type& lock_ptr);

    void add_module(module_ptr mod, data_t& data);
    void remove_module(module_ptr mod, lock_ptr_type& lock_ptr);

    module_ptr load_module_from_factory(mc::string_view module_name);
    module_ptr load_module_form_library(mc::string_view module_name);
    module_ptr add_library(mc::string_view module_name, library_info_ptr info, bool& is_reused, data_t& data);

    mc::mutex_box<data_t> m_data;
    module_loader         m_loader;
    mc::connection_type   m_factory_unregister_conn;
};

/* -------------------------------------------------------------------------- */
/*                               library_module                              */
/* -------------------------------------------------------------------------- */

library_module::~library_module()
{
    // 由于 reset_for_test() 会先调用 unload_all() 卸载所有模块，
    // 当 library_module 析构时，它应该已经被从 loaded_modules 中移除了。
    // 如果 m_impl 和 m_info 仍然有效，说明这是正常的模块卸载流程，
    // 我们需要从 handles 中移除 library info。
    if (m_info && m_impl) {
        m_impl->m_data.with_lock_ptr([&](auto lock_ptr) {
            m_impl->remove_library(m_info->module_name, lock_ptr);
        });
    }

    m_info.reset();
    m_impl.reset();
}

/* -------------------------------------------------------------------------- */
/*                               module_manager_impl                         */
/* -------------------------------------------------------------------------- */
module_manager_impl::module_manager_impl()
{
    auto& global              = mc::reflect::reflection_factory::global();
    m_factory_unregister_conn = global.on_factory_unregister.connect([this](auto factory_id) {
        unload(factory_id);
    });
}

module_manager_impl::~module_manager_impl()
{
    m_factory_unregister_conn.disconnect();
    clear();
}

void module_manager_impl::close_handle(const library_info& info, bool need_unload)
{
    try {
        info.close_func();
        dlog("unload dynamic module: ${name}", ("name", info.path));

        if (need_unload && m_loader.get_load_lib_func().unload) {
            m_loader.get_load_lib_func().unload(info.handle);
        }
    } catch (const std::exception& e) {
        elog("failed unload dynamic module: ${name} - ${error}", ("name", info.path)("error", e.what()));
    }
}

void module_manager_impl::unload_all()
{
    // 获取所有已加载模块的名称，然后在锁外逐个卸载
    std::vector<mc::string> module_names;
    m_data.with_lock([&](auto& data) {
        module_names.reserve(data.loaded_modules.size());
        for (const auto& [name, _] : data.loaded_modules) {
            module_names.push_back(mc::string(name));
        }
    });

    // 在锁外逐个卸载模块，避免死锁
    for (const auto& name : module_names) {
        unload(name);
    }
}

void module_manager_impl::clear()
{
    // 先提取所有需要清理的资源
    library_map handles;
    module_map modules;
    module_id_map modules_by_id;

    m_data.with_lock([&](auto& data) {
        handles = std::move(data.handles);
        modules = std::move(data.loaded_modules);
        modules_by_id = std::move(data.loaded_modules_by_id);
    });

    // 在锁外清理资源，避免死锁
    modules.clear();
    modules_by_id.clear();
    for (const auto& [name, handle] : handles) {
        close_handle(*handle, true);
    }
}

module_ptr module_manager_impl::require(mc::string_view module_name)
{
    return m_data.with_lock([&](data_t& data) -> module_ptr {
        auto it = data.loaded_modules.find(module_name);
        if (it != data.loaded_modules.end()) {
            return it->second;
        }

        // 尝试加载已经注册到工厂的模块
        auto mod = load_module_from_factory(module_name);
        if (mod) {
            add_module(mod, data);
            return mod;
        }

        // 尝试加载动态模块
        auto dynamic_mod = load_module_form_library(module_name);
        if (dynamic_mod) {
            add_module(dynamic_mod, data);
            return dynamic_mod;
        }

        return nullptr;
    });
}

void module_manager_impl::add_module(module_ptr mod, data_t& data)
{
    data.loaded_modules[mod->name()]                                = mod;
    data.loaded_modules_by_id[mod->get_factory()->get_factory_id()] = mod;
}

void module_manager_impl::remove_module(module_ptr mod, lock_ptr_type& lock_ptr)
{
    auto factory_id = mod->get_factory()->get_factory_id();
    lock_ptr->loaded_modules.erase(mod->name());
    lock_ptr->loaded_modules_by_id.erase(factory_id);
}

module_ptr module_manager_impl::load_module_from_factory(mc::string_view module_name)
{
    // 从全局反射工厂中查找模块
    auto& global  = mc::reflect::reflection_factory::global();
    auto  factory = global.get_factory(module_name);
    if (!factory) {
        return nullptr;
    }

    try {
        return mc::make_shared<factory_module>(module_name, factory);
    } catch (const std::exception& e) {
        elog("failed load module: ${name} - ${error}", ("name", module_name)("error", e.what()));
        return nullptr;
    }
}

module_ptr module_manager_impl::add_library(mc::string_view module_name, library_info_ptr info, bool& is_reused,
                                            data_t& data)
{
    auto it = data.handles.find(module_name);
    if (it != data.handles.end()) {
        // 在外部调用 module_manager::unload(module_name) 卸载模块后，如果还存在引用 module_ptr 时，
        // 我们并不会真正卸载 handle，以这里可以复用之前加载的 handle 不需要重新加载
        info      = it->second;
        is_reused = true;
    } else {
        auto* factory = info->open_func();
        if (!factory) {
            return nullptr;
        }

        info->module_name = module_name;
        info->factory     = factory;
        auto ret          = data.handles.emplace(module_name, info);
        if (!ret.second) {
            close_handle(*info, false); // 这里失败不用卸载动态库，底层 loader 会自动卸载
            return nullptr;
        }
    }

    auto mod = mc::make_shared<library_module>(module_name, info->factory->shared_from_this());
    if (!mod) {
        close_handle(*info, false); // 这里失败不用卸载动态库，底层 loader 会自动卸载
        return nullptr;
    }

    mod->set_library_info(info, shared_from_this());
    return mod;
}

module_ptr module_manager_impl::load_module_form_library(mc::string_view module_name)
{
    module_ptr mod = nullptr;
    if (!m_loader.load_module(module_name, [&](library_info_ptr info, bool& is_reused) {
        mod = add_library(module_name, std::move(info), is_reused, m_data.unsafe_get_data());
        return mod != nullptr;
    })) {
        return nullptr;
    }
    return mod;
}

void module_manager_impl::unload(mc::string_view module_name)
{
    m_data.with_lock_ptr([&](auto lock_ptr) {
        auto it = lock_ptr->loaded_modules.find(module_name);
        if (it != lock_ptr->loaded_modules.end()) {
            auto mod_ptr = it->second; // 先保持 mod_ptr 的引用，延迟到下面解锁才释放
            remove_module(mod_ptr, lock_ptr);

            // 这里需要先解锁，因为 reset 到 0 可能会调用 module 的析构函数，
            // 而 module 的析构函数可能会触发 ~library_module 析构，
            // 所以这里需要先解锁，避免死锁
            auto unlock = lock_ptr.scoped_unlock();
            mod_ptr.reset();
        }
    });
}

void module_manager_impl::unload(factory_id_type factory_id)
{
    m_data.with_lock_ptr([&](auto lock_ptr) {
        auto it = lock_ptr->loaded_modules_by_id.find(factory_id);
        if (it != lock_ptr->loaded_modules_by_id.end()) {
            auto mod_ptr = it->second;
            remove_module(mod_ptr, lock_ptr);

            auto unlock = lock_ptr.scoped_unlock();
            mod_ptr.reset();
        }
    });
}

void module_manager_impl::remove_library(mc::string_view module_name, lock_ptr_type& lock_ptr)
{
    auto it = lock_ptr->handles.find(module_name);
    if (it != lock_ptr->handles.end()) {
        library_info_ptr info_ptr = it->second;
        lock_ptr->handles.erase(it);

        auto unlock = lock_ptr.scoped_unlock();
        close_handle(*info_ptr, true);
    }
}

/* -------------------------------------------------------------------------- */
/*                                  module_manager                            */
/* -------------------------------------------------------------------------- */

module_manager::module_manager() : m_impl(mc::make_shared<module_manager_impl>())
{}

module_manager::~module_manager()
{}

module_manager& module_manager::instance()
{
    return mc::singleton<module_manager>::instance_with_creator([]() {
        return new module_manager;
    });
}

void module_manager::reset_for_test()
{
    // 在重置单例之前，先卸载所有模块
    // 这样可以确保在 module_manager_impl 析构时，
    // 不会有任何 library_module 析构函数尝试访问已失效的 m_data
    if (auto* instance = mc::singleton<module_manager>::try_get()) {
        instance->m_impl->unload_all();
    }

    // 然后再重置单例
    mc::singleton<module_manager>::reset_for_test();
}

module_ptr module_manager::require(mc::string_view module_name)
{
    return m_impl->require(module_name);
}

void module_manager::unload(mc::string_view module_name)
{
    m_impl->unload(module_name);
}

void module_manager::add_search_path(mc::string_view path)
{
    m_impl->m_loader.add_search_path(path);
}

std::vector<mc::string_view> module_manager::loaded_modules() const
{
    std::vector<mc::string_view> names;
    m_impl->m_data.with_lock([&](auto& data) {
        for (const auto& [name, _] : data.loaded_modules) {
            names.push_back(name);
        }
    });
    return names;
}

bool module_manager::is_loaded(mc::string_view module_name) const
{
    return m_impl->m_data.with_lock([&](auto& data) {
        return data.loaded_modules.find(module_name) != data.loaded_modules.end();
    });
}

} // namespace mc::module