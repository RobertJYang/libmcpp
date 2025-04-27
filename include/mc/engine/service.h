/**
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

#ifndef MC_ENGINE_MIDDLEWARE_DBUS_SERVICE_H
#define MC_ENGINE_MIDDLEWARE_DBUS_SERVICE_H
#include <mc/common.h>
#include <mc/core/service.h>
#include <mc/db/object.h>

#include <memory>

namespace mc::engine {
class engine;
struct service_impl;

class service : public mc::service_base<service>, public mc::noncopyable_nonmovable {
public:
    service(std::string_view name);
    ~service() override;

    bool init(dict args = {}) override;
    bool start() override;
    bool stop() override;
    void cleanup() override;
    bool is_healthy() const override;

protected:
    std::unique_ptr<service_impl> m_impl;
};

using service_ptr = std::shared_ptr<service>;

} // namespace mc::engine

#endif // MC_ENGINE_MIDDLEWARE_SERVICE_H
