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

#ifndef MC_ENGINE_SHM_PROPERTY_SYNC_H
#define MC_ENGINE_SHM_PROPERTY_SYNC_H

#include <string>
#include <string_view>

#include <mc/common.h>
#include <mc/variant.h>

// heap property 值与 SHM property slab 的双向同步。
// 所有函数 noexcept；失败 silently skip。USE_SHM=OFF 时函数体提前返回。

namespace mc::engine {

class abstract_object;
class property_base;
struct shm_object;

// heap → SHM：把单个 property 的新值写入 obj 关联的 shm_object。
MC_API void shm_sync_property(abstract_object&     obj,
                              const mc::variant&   value,
                              const property_base& prop) noexcept;

// property key 编码：interface_name + 0x1F + property_name。
inline constexpr char shm_property_key_sep = '\x1f';

inline std::string shm_property_compose_key(std::string_view interface_name,
                                            std::string_view property_name)
{
    std::string out;
    out.reserve(interface_name.size() + 1U + property_name.size());
    out.append(interface_name.data(), interface_name.size());
    out.push_back(shm_property_key_sep);
    out.append(property_name.data(), property_name.size());
    return out;
}

inline void shm_property_decompose_key(std::string_view key,
                                       std::string_view& interface_name_out,
                                       std::string_view& property_name_out) noexcept
{
    auto pos = key.find(shm_property_key_sep);
    if (pos == std::string_view::npos) {
        interface_name_out = std::string_view{};
        property_name_out  = key;
        return;
    }
    interface_name_out = key.substr(0, pos);
    property_name_out  = key.substr(pos + 1U);
}

// SHM → heap：把 sh.properties 灌回 obj（recover/takeover 用）。
// sh 须 valid（CRC / isolated 检查由调用方完成）。未知 property/tag 静默跳过。
MC_API void shm_load_properties_into(abstract_object& obj, const shm_object& sh) noexcept;

}  // namespace mc::engine

#endif  // MC_ENGINE_SHM_PROPERTY_SYNC_H
