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

#include <mc/engine/engine.h>

#include <algorithm>
#include <mutex>
#include <utility>

namespace mc::engine {

namespace {

std::mutex& path_template_backend_mutex()
{
    static std::mutex s_mutex;
    return s_mutex;
}

path_template_backend_list& path_template_backends()
{
    static path_template_backend_list s_backends;
    return s_backends;
}

} // namespace

void engine::add_path_template_backend(path_template_backend_ptr backend)
{
    if (!backend) {
        return;
    }

    std::lock_guard lock(path_template_backend_mutex());

    auto&       backends = path_template_backends();
    const auto& name     = backend->name();
    auto it = std::find_if(backends.begin(), backends.end(), [&](const auto& current) {
        return current && current->name() == name;
    });
    if (it != backends.end()) {
        *it = std::move(backend);
        return;
    }

    backends.push_back(std::move(backend));
}

path_template_backend_list engine::get_path_template_backends()
{
    std::lock_guard lock(path_template_backend_mutex());
    return path_template_backends();
}

void engine::set_path_template_backends(path_template_backend_list backends)
{
    std::lock_guard lock(path_template_backend_mutex());
    path_template_backends() = std::move(backends);
}

bool engine::resolve_path_template(mc::string_view path_pattern, const abstract_object& object, mc::string& path)
{
    auto backends = get_path_template_backends();
    for (const auto& backend : backends) {
        if (backend && backend->try_resolve(path_pattern, object, path)) {
            return true;
        }
    }
    return false;
}

} // namespace mc::engine
