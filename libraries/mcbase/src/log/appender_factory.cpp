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

#include <dlfcn.h>
#include <functional>
#include <mc/filesystem.h>
#include <mc/log/appender_factory.h>
#include <mc/log/appenders/console_appender.h>
#include <mc/log/log.h>
#include <mc/variant.h>
#include <string_view>
#include <unordered_map>

namespace mc {
namespace log {

appender::~appender()
{}

mc::string_view appender::get_name() const
{
    return m_name;
}

void appender::set_name(mc::string_view name)
{
    m_name = name;
}

void appender::set_level(level /*lvl*/)
{
    // 默认不实现
}

class appender_factory::impl {
    struct library_info {
        void*                handle;    // 动态库句柄
        appender_creator_c   creator;   // 创建函数
        appender_destroyer_c destroyer; // 销毁函数

        library_info(void* h = nullptr, appender_creator_c c = nullptr, appender_destroyer_c d = nullptr)
            : handle(h), creator(c), destroyer(d)
        {}
    };

public:
    impl()
    {
        register_builtin_appenders();
    }

    ~impl()
    {
        cleanup();
    }

    appender_ptr create_by_type(mc::string_view type)
    {
        const mc::string type_key(type);
        auto             it = m_creators.find(type_key);
        if (it != m_creators.end()) {
            return it->second();
        }
        return nullptr;
    }

    appender_ptr create(mc::string_view name, mc::string_view type, const dict& config)
    {
        const mc::string name_key(name);

        // 检查是否已存在同名appender
        auto it = m_appenders.find(name_key);
        if (it != m_appenders.end()) {
            // 已存在同名appender，返回错误
            elog("Appender with same name already exists: ${name}", ("name", name));
            return nullptr;
        }

        // 创建新的appender
        appender_ptr appender = create_by_type(type);
        if (!appender) {
            elog("Failed to create appender, unknown type: ${type}", ("type", type));
            return nullptr;
        }

        // 设置名称
        appender->set_name(name);

        // 初始化
        if (!appender->init(config)) {
            elog("Failed to initialize appender: ${name}", ("name", name));
            return nullptr;
        }

        // 保存实例
        m_appenders[name_key] = appender;

        return appender;
    }

    bool load(mc::string_view library_path, mc::string_view appender_type)
    {
        const mc::string path_key(library_path);
        auto             lib_it = m_libraries.find(path_key);
        if (lib_it != m_libraries.end()) {
            return true;
        }

        void* handle = dlopen(path_key.c_str(), RTLD_LAZY);
        if (!handle) {
            elog("Failed to load dynamic library: ${path}, error: ${error}",
                 ("path", library_path)("error", dlerror()));
            return false;
        }

        auto creator = reinterpret_cast<appender_creator_c>(dlsym(handle, "create_appender_c"));
        if (!creator) {
            elog("Failed to find creator function: ${path}, error: ${error}",
                 ("path", library_path)("error", dlerror()));
            dlclose(handle);
            return false;
        }

        auto destroyer = reinterpret_cast<appender_destroyer_c>(dlsym(handle, "destroy_appender_c"));
        if (!destroyer) {
            elog("Failed to find destroyer function: ${path}, error: ${error}",
                 ("path", library_path)("error", dlerror()));
            dlclose(handle);
            return false;
        }

        m_libraries[path_key] = {handle, creator, destroyer};
        register_creator(appender_type, [this, lib_path = path_key]() {
            auto lib_it = m_libraries.find(lib_path);
            if (lib_it != m_libraries.end()) {
                return create_appender_instance(lib_it->second);
            }
            return appender_ptr();
        });

        return true;
    }

    void load_all(mc::string_view dir_path)
    {
        try {
            for (const auto& entry : mc::filesystem::directory_iterator(
                     mc::filesystem::path(mc::to_std_string(mc::string(dir_path))))) {
                if (entry.status().type() == mc::filesystem::file_type::regular && entry.path().extension() == ".so") {
                    auto appender_type = entry.path().stem().string();
                    load(entry.path().string(), appender_type);
                }
            }
        } catch (const std::exception& e) {
            elog("Failed to load appenders: ${path}, error: ${error}", ("path", dir_path)("error", e.what()));
        }
    }

    appender_ptr get_appender(mc::string_view name)
    {
        const mc::string name_key(name);
        auto             it = m_appenders.find(name_key);
        if (it != m_appenders.end()) {
            return it->second;
        }
        return nullptr;
    }

    appender_ptr get_or_create_appender(mc::string_view name, mc::string_view type, const mc::dict& config)
    {
        const mc::string name_key(name);

        // 检查是否已存在同名appender
        auto it = m_appenders.find(name_key);
        if (it != m_appenders.end()) {
            appender_ptr& appender = it->second;
            appender->init(config);
            return appender;
        }

        // 不存在，创建新的
        return create(name, type, config);
    }

    void register_creator(mc::string_view type, std::function<appender_ptr()> creator)
    {
        m_creators[mc::string(type)] = std::move(creator);
    }

private:
    // 注册内置追加器
    void register_builtin_appenders()
    {
        register_creator("console", []() {
            return std::make_shared<console_appender>();
        });
    }

    void cleanup()
    {
        for (const auto& [path, lib_info] : m_libraries) {
            if (lib_info.handle) {
                dlclose(lib_info.handle);
            }
        }
        m_libraries.clear();
        m_creators.clear();
    }

    // 从库信息创建追加器实例
    appender_ptr create_appender_instance(const library_info& lib_info)
    {
        if (!lib_info.creator || !lib_info.destroyer) {
            return nullptr;
        }

        auto ptr = lib_info.creator();
        if (!ptr) {
            return nullptr;
        }

        auto shared_ptr = *static_cast<appender_ptr*>(ptr);
        lib_info.destroyer(ptr);
        return shared_ptr;
    }

    std::unordered_map<mc::string, std::function<appender_ptr()>> m_creators;
    std::unordered_map<mc::string, library_info>                  m_libraries;
    std::unordered_map<mc::string, appender_ptr>                  m_appenders;
};

// appender_factory 实现
appender_factory& appender_factory::instance()
{
    static appender_factory instance;
    return instance;
}

appender_factory::appender_factory() : m_impl(std::make_unique<impl>())
{}
appender_factory::~appender_factory() = default;

appender_ptr appender_factory::create_impl(mc::string_view type)
{
    return m_impl->create_by_type(type);
}

appender_ptr appender_factory::create(mc::string_view name, mc::string_view type, const mc::dict& config)
{
    return m_impl->create(name, type, config);
}

bool appender_factory::load(mc::string_view library_path, mc::string_view appender_type)
{
    return m_impl->load(library_path, appender_type);
}

void appender_factory::load_all(mc::string_view dir_path)
{
    m_impl->load_all(dir_path);
}

appender_ptr appender_factory::get_appender(mc::string_view name)
{
    return m_impl->get_appender(name);
}

appender_ptr appender_factory::get_or_create_appender(mc::string_view name, mc::string_view type,
                                                      const mc::dict& config)
{
    return m_impl->get_or_create_appender(name, type, config);
}

void appender_factory::register_creator(mc::string_view type, std::function<appender_ptr()> creator)
{
    m_impl->register_creator(type, std::move(creator));
}

} // namespace log
} // namespace mc