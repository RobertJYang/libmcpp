/*
 * Copyright (c) 2023 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_DATABASE_MEMORY_MANAGER_SERVICE_H
#define MC_DATABASE_MEMORY_MANAGER_SERVICE_H

#include "mc/core/service.h"
#include <boost/program_options.hpp>
#include <memory>

namespace mc::db {

namespace po = boost::program_options;

// 内存管理服务
class memory_manager_service : public service_base<memory_manager_service> {
public:
    memory_manager_service(std::string name = "memory_manager");
    ~memory_manager_service() override;

    // 服务接口实现
    bool init(dict args) override;
    bool start() override;
    bool stop() override;
    void cleanup() override;
    bool is_healthy() const override;

    // 分配内存
    void* allocate(size_t size);

    // 释放内存
    void deallocate(void* ptr);

    // 注册配置选项
    static void register_options(po::options_description& cli_opts,
                                 po::options_description& cfg_opts) {
        cfg_opts.add_options()("memory_manager.max_memory_size",
                               po::value<size_t>()->default_value(1048576),
                               "内存管理服务的最大内存大小");
    }

private:
    class impl;
    std::unique_ptr<impl> m_impl;
};

} // namespace mc::db

#endif // MC_DATABASE_MEMORY_MANAGER_SERVICE_H