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
#include <mc/log.h>
#include <mc/log/appender_factory.h>
#include <mc/log/appenders/console_appender.h>
#include <mc/log/appenders/file_appender.h>
#include <mc/variant.h>
#include <unordered_map>

namespace mc {
namespace log {

appender::~appender() {
}

const std::string& appender::get_name() const {
    return m_name;
}

void appender::set_name(const std::string& name) {
    m_name = name;
}

class appender_factory::impl {
    struct library_info {
        void*                handle;    // 动态库句柄
        appender_creator_c   creator;   // 创建函数
        appender_destroyer_c destroyer; // 销毁函数

        library_info(void* h = nullptr, appender_creator_c c = nullptr,
                     appender_destroyer_c d = nullptr)
            : handle(h), creator(c), destroyer(d) {
        }
    };

public:
    impl() {
        register_builtin_appenders();
    }

    ~impl() {
        cleanup();
    }

    appender_ptr create_by_type(const std::string& type) {
        auto it = m_creators.find(type);
        if (it != m_creators.end()) {
            return it->second();
        }
        return nullptr;
    }

    appender_ptr create(const std::string& name, const std::string& type, const dict& config) {
        // 检查是否已存在同名appender
        auto it = m_appenders.find(name);
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
        m_appenders[name] = appender;

        return appender;
    }

    bool load(const std::string& library_path, const std::string& appender_type) {
        auto lib_it = m_libraries.find(library_path);
        if (lib_it != m_libraries.end()) {
            return true;
        }

        void* handle = dlopen(library_path.c_str(), RTLD_LAZY);
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

        auto destroyer =
            reinterpret_cast<appender_destroyer_c>(dlsym(handle, "destroy_appender_c"));
        if (!destroyer) {
            elog("Failed to find destroyer function: ${path}, error: ${error}",
                 ("path", library_path)("error", dlerror()));
            dlclose(handle);
            return false;
        }

        m_libraries[library_path] = {handle, creator, destroyer};
        register_creator(appender_type, [this, lib_path = library_path]() {
            auto lib_it = m_libraries.find(lib_path);
            if (lib_it != m_libraries.end()) {
                return create_appender_instance(lib_it->second);
            }
            return appender_ptr();
        });

        return true;
    }

    void load_all(const std::string& dir_path) {
        try {
            for (const auto& entry : mc::filesystem::directory_iterator(dir_path)) {
                if (entry.status().type() == mc::filesystem::file_type::regular &&
                    entry.path().extension() == ".so") {
                    auto appender_type = entry.path().stem().string();
                    load(entry.path().string(), appender_type);
                }
            }
        } catch (const std::exception& e) {
            elog("Failed to load appenders: ${path}, error: ${error}", ("path", dir_path)("error", e.what()));
        }
    }

    appender_ptr get_appender(const std::string& name) {
        auto it = m_appenders.find(name);
        if (it != m_appenders.end()) {
            return it->second;
        }
        return nullptr;
    }

    appender_ptr get_or_create_appender(const std::string& name, const std::string& type,
                                        const mc::dict& config) {
        // 检查是否已存在同名appender
        auto it = m_appenders.find(name);
        if (it != m_appenders.end()) {
            appender_ptr& appender = it->second;
            appender->init(config);
            return appender;
        }

        // 不存在，创建新的
        return create(name, type, config);
    }

    void register_creator(const std::string& type, std::function<appender_ptr()> creator) {
        m_creators[type] = std::move(creator);
    }

private:
    // 注册内置追加器
    void register_builtin_appenders() {
        register_creator("console", []() {
            return std::make_shared<console_appender>();
        });

        register_creator("file", []() {
            return std::make_shared<file_appender>();
        });
    }

    void cleanup() {
        for (const auto& [path, lib_info] : m_libraries) {
            if (lib_info.handle) {
                dlclose(lib_info.handle);
            }
        }
        m_libraries.clear();
        m_creators.clear();
    }

    // 从库信息创建追加器实例
    appender_ptr create_appender_instance(const library_info& lib_info) {
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

    std::unordered_map<std::string, std::function<appender_ptr()>> m_creators;
    std::unordered_map<std::string, library_info>                  m_libraries;
    std::unordered_map<std::string, appender_ptr>                  m_appenders;
};

// appender_factory 实现
appender_factory& appender_factory::instance() {
    static appender_factory instance;
    return instance;
}

appender_factory::appender_factory()
    : m_impl(std::make_unique<impl>()) {
}
appender_factory::~appender_factory() = default;

appender_ptr appender_factory::create_impl(const std::string& type) {
    return m_impl->create_by_type(type);
}

appender_ptr appender_factory::create(const std::string& name, const std::string& type,
                                      const mc::dict& config) {
    return m_impl->create(name, type, config);
}

bool appender_factory::load(const std::string& library_path, const std::string& appender_type) {
    return m_impl->load(library_path, appender_type);
}

void appender_factory::load_all(const std::string& dir_path) {
    m_impl->load_all(dir_path);
}

appender_ptr appender_factory::get_appender(const std::string& name) {
    return m_impl->get_appender(name);
}

appender_ptr appender_factory::get_or_create_appender(const std::string& name,
                                                      const std::string& type,
                                                      const mc::dict&    config) {
    return m_impl->get_or_create_appender(name, type, config);
}

void appender_factory::register_creator(const std::string&            type,
                                        std::function<appender_ptr()> creator) {
    m_impl->register_creator(type, std::move(creator));
}

} // namespace log
} // namespace mc