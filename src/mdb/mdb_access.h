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
#ifndef MC_MDB_ACCESS_H
#define MC_MDB_ACCESS_H

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <mc/algorithm/lru_cache.h>
#include <mc/dbus/sd_bus.h>

#include "../introspection/introspection_cache.h"
#include "../introspection/introspection_types.h"
#include "proxy_object.h"

#include <mc/variant.h>

class proxy_object;
class introspection_cache;

namespace mdb_utils {
/**
 * @brief 将方法调用返回的 variants 转换为单个 variant
 * @param results 方法调用返回的 variants 数组
 * @return 转换后的 variant：
 *         - 如果 results 为空，返回空 variant
 *         - 如果 results 只有一个元素，直接返回该元素
 *         - 如果 results 有多个元素，返回包含所有元素的数组 variant
 */
inline mc::variant convert_method_result(const mc::variants& results) {
    if (results.empty()) {
        return mc::variant();
    } else if (results.size() == 1) {
        return results[0];
    } else {
        return mc::variant(results);
    }
}
} // namespace mdb_utils

class mdb_access {
public:
    /**
     * @brief 获取单例实例
     * @param max_cache_size 最大缓存数量，默认值为 100
     * @return 返回单例实例的引用
     * @note 第一次调用时可以指定最大缓存数量，后续调用会忽略该参数
     */
    static mdb_access& instance(size_t max_cache_size = 100);

    std::shared_ptr<proxy_object> get_object_by_short_call(
        mc::dbus::sd_bus*  bus,
        const std::string& path,
        const std::string& interface);

    std::shared_ptr<proxy_object> get_object(
        std::unique_ptr<mc::dbus::sd_bus> bus,
        const std::string&                path,
        const std::string&                interface);

    /**
     * @brief 使用指定的 service 名称获取代理对象
     * @param bus 总线连接对象（unique_ptr，所有权会转移）
     * @param service 服务名称（会进行基本格式校验）
     * @param path 对象路径
     * @param interface 接口名称
     * @return 返回 proxy_object 的 shared_ptr
     * @note 此方法不会查找 service，直接使用提供的 service 名称，但会对 service 名称进行格式校验
     */
    std::shared_ptr<proxy_object> get_object_with_service(
        std::unique_ptr<mc::dbus::sd_bus> bus,
        const std::string&                service,
        const std::string&                path,
        const std::string&                interface);

    /**
     * @brief 获取指定路径下所有子对象的代理对象
     * @param bus 总线连接对象（unique_ptr，所有权会转移）
     * @param path 父路径
     * @param interface 接口名称
     * @param depth 递归深度，默认为1
     * @return 返回 map，键为子路径字符串，值为对应的 proxy_object 的 shared_ptr
     * @note 如果某个子路径获取对象失败，会跳过该路径，不影响其他路径的处理
     */
    std::map<std::string, std::shared_ptr<proxy_object>> get_sub_objects(
        std::unique_ptr<mc::dbus::sd_bus> bus,
        const std::string&                path,
        const std::string&                interface,
        int32_t                           depth = 1);

    /**
     * @brief 清空缓存
     * @note 线程安全，会阻塞直到所有并发操作完成
     */
    void clear_cache();

    /**
     * @brief 设置最大缓存数量
     * @param max_size 最大缓存数量
     * @note 线程安全
     */
    void set_max_cache_size(size_t max_size);

    /**
     * @brief 获取当前缓存大小
     * @return 当前缓存的条目数量
     * @note 线程安全
     */
    size_t cache_size() const;

private:
    explicit mdb_access(size_t max_cache_size);
    mutable std::mutex                                                    m_mutex;
    mc::algorithm::lru_cache<std::string, std::shared_ptr<proxy_object>> m_cache;
};

#endif // MC_MDB_ACCESS_H
