/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_EXPR_FUNCTION_COLLECTION_H
#define MC_EXPR_FUNCTION_COLLECTION_H

#include <mc/dict.h>
#include <mc/engine/service.h>
#include <mc/variant.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace mc::expr {

class MC_API func_collection {
public:
    func_collection();

    static func_collection& get_instance();

    void                                 add(const std::string_view& position, std::shared_ptr<mc::engine::service> service, mc::mutable_dict& functions);
    mc::variant                          get(const std::string_view& position, const std::string_view& func_name);
    std::shared_ptr<mc::engine::service> get_service(const std::string_view& position);
    mc::mutable_dict                     get(const std::string_view& position);
    mc::mutable_dict                     remove(const std::string_view& position);
    bool                                 contains(const std::string_view& position);
    void                                 clear();
    size_t                               size() const;
    bool                                 empty() const;

private:
    mutable std::mutex                                                    m_mutex;
    mc::mutable_dict                                                      m_functions;
    std::unordered_map<std::string, std::shared_ptr<mc::engine::service>> m_services;
};

} // namespace mc::expr

#endif // MC_EXPR_FUNCTION_COLLECTION_H