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

#ifndef MC_MODULE_H
#define MC_MODULE_H

#include <mc/log.h>
#include <string>
#include <vector>

namespace mc {

/**
 * @brief 模块信息
 */
struct module_info {
    std::string              name;         // 模块名称
    std::string              version;      // 模块版本
    std::vector<std::string> dependencies; // 依赖的模块
};

/**
 * @brief 模块基类
 */
class module {
public:
    virtual ~module() = default;

    // 获取模块信息
    virtual const module_info& get_info() const = 0;

    // 初始化模块（注册服务）
    virtual bool init() = 0;

    // 启动模块
    virtual bool start() = 0;

    // 停止模块
    virtual bool stop() = 0;

    // 卸载模块
    virtual bool unload() = 0;
};

/**
 * @brief 模块基类模板，提供基础状态管理
 *
 * @tparam Impl 派生类类型（CRTP模式）
 */
template <typename Impl>
class module_base : public module {
public:
    module_base()           = default;
    ~module_base() override = default;

    // 获取模块信息
    const module_info& get_info() const override = 0;

    // 初始化模块（注册服务）
    bool init() override {
        if (m_initialized) {
            dlog("模块'${name}'已经初始化，忽略重复调用",
                 ("name", static_cast<const Impl*>(this)->get_info().name));
            return true;
        }

        bool result = static_cast<Impl*>(this)->module_init();
        if (result) {
            m_initialized = true;
        }
        return result;
    }

    // 启动模块
    bool start() override {
        if (m_running) {
            ilog("模块'${name}'已经启动，忽略重复调用",
                 ("name", static_cast<const Impl*>(this)->get_info().name));
            return true;
        }

        if (!m_initialized) {
            wlog("模块'${name}'尚未初始化，无法启动",
                 ("name", static_cast<const Impl*>(this)->get_info().name));
            return false;
        }

        bool result = static_cast<Impl*>(this)->module_start();
        if (result) {
            m_running = true;
        }
        return result;
    }

    // 停止模块
    bool stop() override {
        if (!m_running) {
            ilog("模块'${name}'已经停止，忽略重复调用",
                 ("name", static_cast<const Impl*>(this)->get_info().name));
            return true;
        }

        bool result = static_cast<Impl*>(this)->module_stop();
        if (result) {
            m_running = false;
        }
        return result;
    }

    // 卸载模块
    bool unload() override {
        if (!m_initialized) {
            ilog("模块'${name}'未初始化，忽略卸载",
                 ("name", static_cast<const Impl*>(this)->get_info().name));
            return true;
        }

        if (m_running) {
            wlog("模块'${name}'仍在运行，尝试先停止",
                 ("name", static_cast<const Impl*>(this)->get_info().name));
            if (!stop()) {
                elog("模块'${name}'停止失败，无法卸载",
                     ("name", static_cast<const Impl*>(this)->get_info().name));
                return false;
            }
        }

        bool result = static_cast<Impl*>(this)->module_unload();
        if (result) {
            m_initialized = false;
        }
        return result;
    }

protected:
    // 派生类需要实现的实际操作方法
    bool module_init() {
        return true;
    }
    bool module_start() {
        return true;
    }
    bool module_stop() {
        return true;
    }
    bool module_unload() {
        return true;
    }

private:
    bool m_initialized{false}; // 是否已初始化
    bool m_running{false};     // 是否正在运行
};

} // namespace mc

#endif // MC_MODULE_H