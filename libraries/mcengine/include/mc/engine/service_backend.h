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

#ifndef MC_ENGINE_SERVICE_BACKEND_H
#define MC_ENGINE_SERVICE_BACKEND_H

#include <mc/common.h>
#include <mc/engine/match.h>
#include <mc/engine/message.h>

namespace mc::engine {

class abstract_object;

class MC_API service_backend {
public:
    virtual ~service_backend() = default;

    // 返回 true 表示继续走 mcengine 内部 emit；返回 false 表示 backend 已完成处理。
    virtual bool emit(const message& msg)
    {
        return true;
    }

    virtual void on_object_added(abstract_object& obj)
    {}
    virtual void on_object_removed(abstract_object& obj)
    {}
    virtual void on_match_added(match_id id, const match_rule& rule)
    {}
    virtual void on_match_removed(match_id id)
    {}
};

} // namespace mc::engine

#endif // MC_ENGINE_SERVICE_BACKEND_H
