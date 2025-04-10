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

#ifndef MC_DATABASE_REFERENCE_TRACKER_SERVICE_H
#define MC_DATABASE_REFERENCE_TRACKER_SERVICE_H

#include "mc/core/service.h"
#include "mc/db/common.h"
#include <boost/program_options.hpp>
#include <memory>
#include <vector>

namespace mc::db {

namespace po = boost::program_options;

// 引用跟踪服务
class reference_tracker_service : public service_base<reference_tracker_service> {
public:
    reference_tracker_service(std::string name = "reference_tracker");
    ~reference_tracker_service() override;

    // 服务接口实现
    bool init(dict args) override;
    bool start() override;
    bool stop() override;
    void cleanup() override;
    bool is_healthy() const override;

    // 添加对象引用
    error_code add_reference(const path& object_path, client_id client);

    // 移除对象引用
    error_code remove_reference(const path& object_path, client_id client);

    // 获取对象引用计数
    size_t get_reference_count(const path& object_path);

    // 获取对象的所有客户端
    std::vector<client_id> get_referencing_clients(const path& object_path);

    // 客户端断开连接清理
    void cleanup_client_references(client_id client);

    // 注册配置选项
    static void register_options(po::options_description& cli_opts,
                                 po::options_description& cfg_opts) {
        cfg_opts.add_options()("reference_tracker.auto_cleanup",
                               po::value<bool>()->default_value(true),
                               "客户端断开连接时自动清理引用");
    }

private:
    class impl;
    std::unique_ptr<impl> m_impl;
};

} // namespace mc::db

#endif // MC_DATABASE_REFERENCE_TRACKER_SERVICE_H