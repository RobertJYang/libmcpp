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

#ifndef MC_CORE_INTERNAL_DEPENDENCY_SORTER_H
#define MC_CORE_INTERNAL_DEPENDENCY_SORTER_H

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mc::core {
namespace internal {

/**
 * @brief 依赖关系排序工具类
 *
 * 提供用于服务依赖关系排序的通用工具函数，可被service_manager和supervisor共同使用。
 */
class dependency_sorter {
public:
    /**
     * @brief 依赖节点结构
     */
    struct dependency_node {
        std::string                     name;          // 节点名称
        std::unordered_set<std::string> dependencies;  // 此节点依赖的节点
        std::unordered_set<std::string> dependents;    // 依赖此节点的节点
        int                             in_degree = 0; // 入度，用于拓扑排序
    };

    /**
     * @brief 构建依赖图
     *
     * @param items 项目映射表，键为项目名称，值为项目对象
     * @param dependency_getter 获取项目依赖关系的函数
     * @return 依赖图
     */
    template <typename T>
    static std::unordered_map<std::string, dependency_node>
    build_dependency_graph(const std::unordered_map<std::string, T>&                items,
                           const std::function<std::vector<std::string>(const T&)>& dependency_getter)
    {
        std::unordered_map<std::string, dependency_node> graph;

        // 构建初始节点和依赖关系
        for (const auto& [name, item] : items) {
            dependency_node node;
            node.name = name;

            // 获取依赖关系
            auto dependencies = dependency_getter(item);
            for (const auto& dep : dependencies) {
                node.dependencies.insert(dep);
                node.in_degree++;
            }

            graph[name] = node;
        }

        // 构建被依赖关系
        for (const auto& [name, node] : graph) {
            for (const auto& dep : node.dependencies) {
                if (graph.find(dep) != graph.end()) {
                    graph[dep].dependents.insert(name);
                }
            }
        }

        return graph;
    }

    /**
     * @brief 执行拓扑排序，返回启动顺序（从无依赖到有依赖）
     *
     * @param graph 依赖图
     * @return 排序后的项目名称列表
     * @throws mc::parse_error_exception 如果存在循环依赖
     */
    static std::vector<std::string> sort_for_startup(const std::unordered_map<std::string, dependency_node>& graph);

    /**
     * @brief 执行拓扑排序，返回停止顺序（从被依赖到无被依赖）
     *
     * @param graph 依赖图
     * @return 排序后的项目名称列表
     * @throws mc::parse_error_exception 如果存在循环依赖
     */
    static std::vector<std::string> sort_for_shutdown(const std::unordered_map<std::string, dependency_node>& graph);

    /**
     * @brief 检查是否存在循环依赖
     *
     * @param graph 依赖图
     * @return 是否存在循环依赖
     */
    static bool has_circular_dependency(const std::unordered_map<std::string, dependency_node>& graph);
};

} // namespace internal
} // namespace mc::core

#endif // MC_CORE_INTERNAL_DEPENDENCY_SORTER_H