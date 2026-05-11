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

/**
 * @file quark_provider.h
 * @brief quark provider 抽象接口
 */
#ifndef MC_SRC_QUARK_PROVIDER_H
#define MC_SRC_QUARK_PROVIDER_H

#include <cstddef>

#include <mc/quark.h>
#include <mc/string_view.h>

#include "quark_record.h"

namespace mc::quark_detail {

/** @brief quark 后端抽象 */
class quark_provider {
public:
    virtual ~quark_provider();

    virtual quark::id_type intern(mc::string_view value)        = 0;
    virtual quark::id_type lookup(mc::string_view value) noexcept = 0;
    virtual const quark_record* resolve(quark::id_type id) noexcept = 0;
    virtual std::size_t         size() const noexcept             = 0;
};

/** @brief 活跃 provider */
quark_provider& active_provider() noexcept;

} // namespace mc::quark_detail

#endif // MC_SRC_QUARK_PROVIDER_H
