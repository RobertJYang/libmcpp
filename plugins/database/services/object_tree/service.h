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

#ifndef MC_DATABASE_OBJECT_TREE_SERVICE_H
#define MC_DATABASE_OBJECT_TREE_SERVICE_H

#include "mc/core/service.h"
#include "mc/db/common.h"
#include <boost/program_options.hpp>
#include <memory>
#include <vector>

namespace mc::db {

namespace po = boost::program_options;

// 对象树服务
class object_tree_service : public service_base<object_tree_service> {
public:
    object_tree_service(std::string name = "object_tree");
    ~object_tree_service() override;

    // 服务接口实现
    bool init(dict args) override;
    bool start() override;
    bool stop() override;
    void cleanup() override;
    bool is_healthy() const override;

    // 注册对象
    error_code register_object(const path& object_path, void* data, size_t size);

    // 更新对象属性
    error_code update_object_property(const path& object_path, const char* property_name,
                                      const void* value, size_t value_size);

    // 获取对象
    void* get_object(const path& object_path, error_code& ec);

    // 列出子对象
    std::vector<std::string> list_children(const path& parent_path, error_code& ec);

    // 检查对象是否存在
    bool exists(const path& object_path);

    // 删除对象
    error_code remove_object(const path& object_path);

    // 注册配置选项
    static void register_options(po::options_description& cli_opts,
                                 po::options_description& cfg_opts) {
        cfg_opts.add_options()("object_tree.max_objects", po::value<size_t>()->default_value(10000),
                               "对象树服务的最大对象数量")(
            "object_tree.max_depth", po::value<size_t>()->default_value(10), "对象树的最大深度");
    }

private:
    class impl;
    std::unique_ptr<impl> m_impl;
};

} // namespace mc::db

#endif // MC_DATABASE_OBJECT_TREE_SERVICE_H