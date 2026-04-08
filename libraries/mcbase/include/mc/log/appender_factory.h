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

#ifndef MC_LOG_APPENDER_FACTORY_H
#define MC_LOG_APPENDER_FACTORY_H

#include <functional>
#include <mc/log/appender.h>
#include <mc/variant.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace mc {
namespace log {

/**
 * @brief 追加器创建函数类型（C 风格接口）
 */
using appender_creator_c = void* (*)();

/**
 * @brief 追加器销毁函数类型（C 风格接口）
 */
using appender_destroyer_c = void (*)(void*);

/**
 * @brief 追加器工厂类
 *
 * 负责创建和管理追加器实例，包括从动态库加载追加器
 */
class MC_API appender_factory {
public:
    /**
     * @brief 获取单例实例
     *
     * @return appender_factory& 单例实例引用
     */
    static appender_factory& instance();

    /**
     * @brief 注册追加器创建函数
     *
     * @param type 追加器类型
     * @param creator 创建函数
     */
    void register_creator(const std::string& type, std::function<appender_ptr()> creator);

    /**
     * @brief 根据类型创建追加器实例（不设置名称，不保存实例）
     *
     * @param type 追加器类型
     * @return 追加器实例
     */
    template <typename T>
    std::shared_ptr<T> create_by_type(const std::string& type)
    {
        return std::dynamic_pointer_cast<T>(create_impl(type));
    }

    /**
     * @brief 创建追加器实例并设置名称
     *
     * 创建指定类型的追加器实例，设置名称，并保存到实例管理器中
     *
     * @param name 追加器名称
     * @param type 追加器类型
     * @param config 追加器配置
     * @return appender_ptr 追加器实例
     */
    appender_ptr create(const std::string& name, const std::string& type, const dict& config);

    /**
     * @brief 从动态库加载追加器
     *
     * @param library_path 动态库路径
     * @param appender_type 追加器类型
     * @return bool 是否成功加载
     */
    bool load(const std::string& library_path, const std::string& appender_type);

    /**
     * @brief 从目录加载所有追加器
     *
     * @param dir_path 目录路径
     */
    void load_all(const std::string& dir_path);

    /**
     * @brief 获取已存在的追加器实例
     *
     * @param name 追加器名称
     * @return appender_ptr 追加器实例，如果不存在则返回nullptr
     */
    appender_ptr get_appender(const std::string& name);

    /**
     * @brief 获取或创建追加器实例
     *
     * 如果指定名称的追加器已存在，则返回已有实例；否则创建新实例
     *
     * @param name 追加器名称
     * @param type 追加器类型
     * @param config 追加器配置
     * @return appender_ptr 追加器实例
     */
    appender_ptr get_or_create_appender(const std::string& name, const std::string& type, const mc::dict& config);

    /**
     * @brief 清理所有资源
     */
    static void cleanup();

    /**
     * @brief 析构函数
     */
    ~appender_factory();

    // 禁止拷贝和赋值
    appender_factory(const appender_factory&)            = delete;
    appender_factory& operator=(const appender_factory&) = delete;

private:
    appender_ptr create_impl(const std::string& type);

    appender_factory();
    class impl;
    std::unique_ptr<impl> m_impl;
};

} // namespace log
} // namespace mc

#endif // MC_LOG_APPENDER_FACTORY_H