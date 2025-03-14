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
#include <mc/filesystem.h>
#include <mc/log/appender.h>
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
class appender_factory {
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
     * @param name 追加器名称
     * @param creator 创建函数
     */
    static void register_creator(const std::string& name, std::function<appender_ptr()> creator);

    /**
     * @brief 创建追加器实例
     *
     * @param name 追加器名称
     * @return appender_ptr 追加器实例
     */
    appender_ptr create(const std::string& name);

    /**
     * @brief 从动态库加载追加器
     *
     * @param library_path 动态库路径
     * @param appender_name 追加器名称
     * @return bool 是否成功加载
     */
    bool load(const std::string& library_path, const std::string& appender_name);

    /**
     * @brief 从目录加载所有追加器
     *
     * @param dir_path 目录路径
     */
    void load_all(const std::string& dir_path);

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
    appender_factory();
    class impl;
    std::unique_ptr<impl> m_impl;
};

} // namespace log
} // namespace mc

#endif // MC_LOG_APPENDER_FACTORY_H