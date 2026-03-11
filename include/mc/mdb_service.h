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

#ifndef MC_MDB_SERVICE_H
#define MC_MDB_SERVICE_H

#include <mc/common.h>
#include <mc/dict.h>
#include <mc/time.h>
#include <mc/variant.h>

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace mc {

namespace dbus {
class message;
class sd_bus;
} // namespace dbus

/**
 * @brief MDB服务客户端命名空间，用于封装对'bmc.kepler.maca'服务的调用
 */
namespace mdb::service {

/**
 * @brief 获取对象
 * @param bus [in] sd_bus 上下文（shared_ptr 确保生命周期安全）
 * @param path [in] 对象路径
 * @param interfaces [in] 接口名称列表
 * @return 返回对象信息
 */
MC_API mc::variant get_object(mc::dbus::sd_bus*   bus,
                              std::string_view    path,
                              const mc::variants& interfaces);

/**
 * @brief 获取子对象
 * @param bus [in] sd_bus 上下文（shared_ptr 确保生命周期安全）
 * @param path [in] 对象路径
 * @param depth [in] 深度
 * @param interfaces [in] 接口名称列表
 * @return 返回子对象信息
 */
MC_API mc::variant get_sub_objects(mc::dbus::sd_bus* bus, std::string_view path,
                                   int32_t depth, const mc::variants& interfaces);

/**
 * @brief 获取子路径
 * @param bus [in] sd_bus 上下文（shared_ptr 确保生命周期安全）
 * @param path [in] 对象路径
 * @param depth [in] 深度
 * @param interfaces [in] 接口名称列表
 * @return 返回子路径列表
 */
MC_API mc::variant get_sub_paths(mc::dbus::sd_bus* bus, std::string_view path,
                                 int32_t depth, const mc::variants& interfaces);

/**
 * @brief 获取父对象
 * @param bus [in] sd_bus 上下文（shared_ptr 确保生命周期安全）
 * @param path [in] 对象路径
 * @param interfaces [in] 接口名称列表
 * @return 返回父对象信息
 */
MC_API mc::variant get_parent_objects(mc::dbus::sd_bus*   bus,
                                      std::string_view    path,
                                      const mc::variants& interfaces);

/**
 * @brief 获取服务名称
 * @param bus [in] sd_bus 上下文（shared_ptr 确保生命周期安全）
 * @param sender [in] 发送者名称
 * @return 返回服务名称
 */
MC_API mc::variant get_service_name(mc::dbus::sd_bus* bus, std::string_view sender);

/**
 * @brief 获取服务名称列表
 * @param bus [in] sd_bus 上下文（shared_ptr 确保生命周期安全）
 * @return 返回服务名称列表
 */
MC_API mc::variant get_service_names(mc::dbus::sd_bus* bus);

/**
 * @brief 获取路径
 * @param bus [in] sd_bus 上下文（shared_ptr 确保生命周期安全）
 * @param interface [in] 接口名称
 * @param filter [in] 过滤条件
 * @param ignore_case [in] 是否忽略大小写
 * @param enable_cache [in] 是否启用缓存
 * @return 返回路径
 */
MC_API mc::variant get_path(mc::dbus::sd_bus* bus, std::string_view interface,
                            std::string_view filter, bool ignore_case, bool enable_cache);

/**
 * @brief 获取接口拥有者列表
 * @param bus [in] sd_bus 上下文（shared_ptr 确保生命周期安全）
 * @param interface [in] 接口名称
 * @return 返回接口拥有者列表
 */
MC_API mc::variant get_interface_owners(mc::dbus::sd_bus* bus,
                                        std::string_view  interface);

/**
 * @brief 验证路径是否有效
 * @param bus [in] sd_bus 上下文（shared_ptr 确保生命周期安全）
 * @param path [in] 对象路径
 * @param ignore_case [in] 是否忽略大小写
 * @return 返回路径是否有效
 */
MC_API mc::variant is_valid_path(mc::dbus::sd_bus* bus, std::string_view path,
                                 bool ignore_case);

/**
 * @brief 分页获取子路径
 * @param bus [in] sd_bus 上下文（shared_ptr 确保生命周期安全）
 * @param path [in] 对象路径
 * @param depth [in] 深度
 * @param interfaces [in] 接口名称列表
 * @param skip [in] 跳过数量
 * @param top [in] 获取数量
 * @return 返回子路径列表
 */
MC_API mc::variant get_sub_paths_paging(mc::dbus::sd_bus* bus, std::string_view path,
                                        int32_t depth, const mc::variants& interfaces, int32_t skip,
                                        int32_t top);

/**
 * @brief 获取类列表
 * @param bus [in] sd_bus 上下文（shared_ptr 确保生命周期安全）
 * @param service [in] 服务名称
 * @return 返回类列表
 */
MC_API mc::variant get_classes(mc::dbus::sd_bus* bus, std::string_view service);

/**
 * @brief 获取对象列表
 * @param bus [in] sd_bus 上下文（shared_ptr 确保生命周期安全）
 * @param class_name [in] 类名称
 * @return 返回对象列表
 */
MC_API mc::variant get_object_list(mc::dbus::sd_bus* bus, std::string_view class_name);

/**
 * @brief 获取对象拥有者
 * @param bus [in] sd_bus 上下文（shared_ptr 确保生命周期安全）
 * @param object_name [in] 对象名称
 * @return 返回对象拥有者
 */
MC_API mc::variant get_object_owner(mc::dbus::sd_bus* bus, std::string_view object_name);

/**
 * @brief 获取匹配的对象
 * @param bus [in] sd_bus 上下文（shared_ptr 确保生命周期安全）
 * @param object_name [in] 对象名称
 * @param interface_pattern [in] 接口模式
 * @return 返回匹配的对象列表
 */
MC_API mc::variant get_matched_objects(mc::dbus::sd_bus* bus,
                                       std::string_view  object_name,
                                       std::string_view  interface_pattern);

/**
 * @brief 获取追踪对象
 * @param bus [in] sd_bus 上下文（shared_ptr 确保生命周期安全）
 * @return 返回追踪对象信息
 */
MC_API mc::variant get_traced_object(mc::dbus::sd_bus* bus);

/**
 * @brief 清理所有缓存
 * @param bus [in] sd_bus 上下文（用于清理信号订阅）
 */
MC_API void clear_cache();

/**
 * @brief 设置get_path缓存的最大数量
 * @param max_size [in] 最大缓存数量，0表示不限制
 * @note 当缓存达到上限时，将采用LRU策略淘汰最久未使用的条目
 */
MC_API void set_max_cache_size(size_t max_size);

/**
 * @brief 获取当前设置的最大缓存数量
 * @return 返回最大缓存数量，0表示不限制
 */
MC_API size_t get_max_cache_size();

/**
 * @brief 获取当前缓存的条目数量
 * @return 返回当前缓存的条目数量
 */
MC_API size_t get_cache_size();

} // namespace mdb::service

} // namespace mc

#endif // MC_MDB_SERVICE_H
