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

#ifndef MC_APP_BASE_APP_H
#define MC_APP_BASE_APP_H

#include <mc/common.h>
#include <mc/object.h>

namespace mc::app {

struct app_options {
    int    argc{0};
    char** argv{nullptr};
};

class MC_API base_app : public mc::object {
public:
    static base_app* get() noexcept;
    static base_app& instance();
    static void      reset_for_test() noexcept;

    base_app(const base_app&)            = delete;
    base_app& operator=(const base_app&) = delete;
    base_app(base_app&&)                 = delete;
    base_app& operator=(base_app&&)      = delete;

    ~base_app() override;

protected:
    base_app();
};

} // namespace mc::app

#endif // MC_APP_BASE_APP_H
