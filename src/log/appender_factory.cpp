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
#include <unordered_map>

namespace mc {
namespace log {

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
    impl() = default;
    ~impl() {
        cleanup();
    }

    appender_ptr create(const std::string& name) {
        auto it = m_creators.find(name);
        if (it != m_creators.end()) {
            return it->second();
        }
        return nullptr;
    }

    bool load(const std::string& library_path, const std::string& appender_name) {
        auto lib_it = m_libraries.find(library_path);
        if (lib_it != m_libraries.end()) {
            return true;
        }

        void* handle = dlopen(library_path.c_str(), RTLD_LAZY);
        if (!handle) {
            elog("无法加载动态库: ${path}, 错误: ${error}",
                 ("path", library_path)("error", dlerror()));
            return false;
        }

        auto creator = reinterpret_cast<appender_creator_c>(dlsym(handle, "create_appender_c"));
        if (!creator) {
            elog("无法找到创建函数: ${path}, 错误: ${error}",
                 ("path", library_path)("error", dlerror()));
            dlclose(handle);
            return false;
        }

        auto destroyer =
            reinterpret_cast<appender_destroyer_c>(dlsym(handle, "destroy_appender_c"));
        if (!destroyer) {
            elog("无法找到销毁函数: ${path}, 错误: ${error}",
                 ("path", library_path)("error", dlerror()));
            dlclose(handle);
            return false;
        }

        m_libraries[library_path] = {handle, creator, destroyer};
        register_creator(appender_name, [this, lib_path = library_path]() {
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
            for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
                if (entry.is_regular_file() && entry.path().extension() == ".so") {
                    auto appender_name = entry.path().stem().string();
                    load(entry.path().string(), appender_name);
                }
            }
        } catch (const std::exception& e) {
            elog("加载追加器失败: ${path}, 错误: ${error}", ("path", dir_path)("error", e.what()));
        }
    }

private:
    void register_creator(const std::string& name, std::function<appender_ptr()> creator) {
        m_creators[name] = std::move(creator);
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
};

// appender_factory 实现
appender_factory& appender_factory::instance() {
    static appender_factory instance;
    return instance;
}

appender_factory::appender_factory() : m_impl(std::make_unique<impl>()) {
}
appender_factory::~appender_factory() = default;

appender_ptr appender_factory::create(const std::string& name) {
    return m_impl->create(name);
}

bool appender_factory::load(const std::string& library_path, const std::string& appender_name) {
    return m_impl->load(library_path, appender_name);
}

void appender_factory::load_all(const std::string& dir_path) {
    m_impl->load_all(dir_path);
}

} // namespace log
} // namespace mc