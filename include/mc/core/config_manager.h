/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/**
 * @file config_manager.h
 * @brief 配置管理器，支持多种格式的配置文件（JSON、TOML等）
 */
#ifndef MC_CONFIG_MANAGER_H
#define MC_CONFIG_MANAGER_H

#include <mc/core/config_schema.h>
#include <mc/log.h>

#include <boost/program_options.hpp>

#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mc::core {

/**
 * @brief 配置加载器接口
 */
class MC_API config_loader {
public:
    virtual ~config_loader() = default;

    /**
     * @brief 加载配置文件
     * @param file_path 配置文件路径
     * @return 配置数据
     */
    virtual variant load(const std::string& file_path) = 0;

    /**
     * @brief 获取支持的文件扩展名
     * @return 支持的文件扩展名列表
     */
    virtual std::vector<std::string> supported_extensions() const = 0;
};

/**
 * @brief JSON配置加载器
 */
class MC_API json_config_loader : public config_loader {
public:
    variant                  load(const std::string& file_path) override;
    std::vector<std::string> supported_extensions() const override {
        return {".json"};
    }
};

/**
 * @brief TOML配置加载器（预留接口）
 */
class MC_API toml_config_loader : public config_loader {
public:
    variant                  load(const std::string& file_path) override;
    std::vector<std::string> supported_extensions() const override {
        return {".toml"};
    }
};

/**
 * @brief 配置管理器
 */
class MC_API config_manager {
public:
    config_manager();
    ~config_manager() = default;

    /**
     * @brief 从命令行解析配置相关参数
     * @param argc 参数数量
     * @param argv 参数数组
     * @return 解析成功返回true，失败返回false
     */
    bool parse_command_line(int argc, char** argv);

    /**
     * @brief 加载配置文件
     * @param file_path 配置文件路径，为空则使用默认路径
     * @return 加载成功返回true，失败返回false
     */
    bool load_config_file(const std::string& file_path = "");

    /**
     * @brief 添加配置
     * @param config 配置对象
     * @return 添加成功返回true，失败返回false
     */
    bool add_config(const variant& config);

    /**
     * @brief 获取配置对象
     * @tparam T 配置类型
     * @param name 配置名称
     * @return 配置对象
     */
    template <typename T>
    std::optional<T> get_config(const std::string& name) const {
        std::string kind(get_kind<T>());
        auto        it = m_configs.find(kind);
        if (it == m_configs.end()) {
            return std::nullopt;
        }

        for (const auto& config : it->second) {
            try {
                T obj;
                from_variant(config, obj);
                if (obj.meta.name == name) {
                    return obj;
                }
            } catch (const std::exception& e) {
                elog("config parse error: ${error}", ("error", e.what()));
            }
        }

        return std::nullopt;
    }

    /**
     * @brief 获取所有指定类型的配置
     * @tparam T 配置类型
     * @return 配置对象列表
     */
    template <typename T>
    std::vector<T> get_configs() const {
        std::vector<T> result;

        std::string kind(get_kind<T>());
        auto        it = m_configs.find(kind);
        if (it == m_configs.end()) {
            return result;
        }

        for (const auto& config : it->second) {
            try {
                T obj;
                from_variant(config, obj);
                result.push_back(obj);
            } catch (const std::exception& e) {
                elog("config parse error: ${error}", ("error", e.what()));
            }
        }

        return result;
    }

    /**
     * @brief 获取插件列表
     * @return 插件名称列表
     */
    std::vector<std::string> get_plugin_names() const;

    /**
     * @brief 获取插件目录
     * @return 插件目录路径
     */
    std::string get_plugin_dir() const;

    /**
     * @brief 获取线程数
     * @return 线程数
     */
    unsigned int get_thread_count() const;

private:
    template <typename T>
    static std::string_view get_kind() {
        if constexpr (std::is_same_v<T, config::app_config>) {
            return "Application";
        } else if constexpr (std::is_same_v<T, config::supervisor_config>) {
            return "Supervisor";
        } else if constexpr (std::is_same_v<T, config::service_config>) {
            return "Service";
        } else if constexpr (std::is_same_v<T, config::plugin_config>) {
            return "Plugin";
        } else {
            static_assert(sizeof(T) == 0, "unsupported config type");
            return "";
        }
    }

    void process_config(const variant& config);
    bool validate_config(const std::string& kind, const variant& config);
    void process_app_config(const variant& config);

    std::unique_ptr<config_loader> create_loader(const std::string& file_path) const;

    std::unique_ptr<config_loader>                m_loader;
    std::unordered_map<std::string, mc::variants> m_configs;
    std::vector<std::string>                      m_plugin_names;

    boost::program_options::options_description m_opts;
    boost::program_options::variables_map       m_variables;

    std::string  m_config_file;
    std::string  m_plugin_dir;
    unsigned int m_thread_count{0};
};

} // namespace mc::core

#endif // MC_CONFIG_MANAGER_H