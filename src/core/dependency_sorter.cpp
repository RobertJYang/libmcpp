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

#include "core/include/dependency_sorter.h"
#include "mc/exception.h"
#include <queue>

namespace mc::core {
namespace internal {

std::vector<std::string>
dependency_sorter::sort_for_startup(const std::unordered_map<std::string, dependency_node>& graph) {
    std::vector<std::string> result;
    std::queue<std::string>  queue;

    // 计算每个节点的入度（依赖的数量）
    std::unordered_map<std::string, int> in_degrees;
    for (const auto& [name, node] : graph) {
        in_degrees[name] = node.dependencies.size(); // 入度是依赖的数量
        if (node.dependencies.empty()) {             // 没有依赖的项目先启动
            queue.push(name);
        }
    }

    // 拓扑排序
    while (!queue.empty()) {
        std::string name = queue.front();
        queue.pop();
        result.push_back(name);

        // 更新依赖此项目的项目的入度
        const auto& node = graph.at(name);
        for (const auto& dependent : node.dependents) {
            in_degrees[dependent]--;
            if (in_degrees[dependent] == 0) {
                queue.push(dependent);
            }
        }
    }

    // 检查是否有循环依赖
    if (result.size() != graph.size()) {
        MC_THROW(mc::parse_error_exception, "依赖图中存在循环依赖");
    }

    return result;
}

std::vector<std::string> dependency_sorter::sort_for_shutdown(
    const std::unordered_map<std::string, dependency_node>& graph) {
    std::vector<std::string> result;
    std::queue<std::string>  queue;

    // 计算每个节点的入度（被依赖的数量）
    std::unordered_map<std::string, int> in_degrees;
    for (const auto& [name, node] : graph) {
        in_degrees[name] = node.dependents.size(); // 入度是被依赖的数量
        if (node.dependents.empty()) {             // 没有被依赖的项目先停止
            queue.push(name);
        }
    }

    // 拓扑排序
    while (!queue.empty()) {
        std::string name = queue.front();
        queue.pop();
        result.push_back(name);

        // 更新此项目依赖的项目的入度
        const auto& node = graph.at(name);
        for (const auto& dependency : node.dependencies) {
            if (graph.find(dependency) != graph.end()) {
                in_degrees[dependency]--;
                if (in_degrees[dependency] == 0) {
                    queue.push(dependency);
                }
            }
        }
    }

    // 检查是否有循环依赖
    if (result.size() != graph.size()) {
        MC_THROW(mc::parse_error_exception, "依赖图中存在循环依赖");
    }

    return result;
}

bool dependency_sorter::has_circular_dependency(
    const std::unordered_map<std::string, dependency_node>& graph) {
    // 复制入度
    std::unordered_map<std::string, int> in_degrees;
    std::queue<std::string>              queue;
    int                                  visited_count = 0;

    // 计算入度并添加所有入度为0的节点到队列
    for (const auto& [name, node] : graph) {
        in_degrees[name] = node.dependencies.size();
        if (in_degrees[name] == 0) {
            queue.push(name);
        }
    }

    // 卡恩算法检测循环
    while (!queue.empty()) {
        std::string name = queue.front();
        queue.pop();
        visited_count++;

        const auto& node = graph.at(name);
        for (const auto& dependent : node.dependents) {
            in_degrees[dependent]--;
            if (in_degrees[dependent] == 0) {
                queue.push(dependent);
            }
        }
    }

    // 如果访问的节点数量小于图中的节点数量，则存在循环
    return visited_count < graph.size();
}

} // namespace internal
} // namespace mc