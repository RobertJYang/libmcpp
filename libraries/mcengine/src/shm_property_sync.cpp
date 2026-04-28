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

#include "shm_property_sync.h"

#include <string>

#include <mc/engine/base.h>
#include <mc/variant.h>

#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM
#include <mc/engine/shm_object.h>

#include "shm_object_ops.h"
#include "shm_runtime_provider.h"
#include <mc/shm/shm_runtime.h>
#endif

namespace mc::engine {

#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM

namespace {

// 把 mc::variant 写到 shadow.properties[key]。
// 覆盖：bool / int* / uint* / double / string / blob(bytes)。
// 复合类型（array / dict / extension）暂不持久化，silent skip。
bool _write_one(shm_allocator& alloc, shm_object& shadow, std::string_view key, const mc::variant& v) noexcept
{
    try {
        if (v.is_bool()) {
            return shm_object_set_property_int64(alloc, shadow, key, v.as<bool>() ? 1 : 0);
        }
        if (v.is_int8() || v.is_int16() || v.is_int32() || v.is_int64()) {
            return shm_object_set_property_int64(alloc, shadow, key, v.as<int64_t>());
        }
        if (v.is_uint8() || v.is_uint16() || v.is_uint32() || v.is_uint64()) {
            // u64 → i64 可能越界；越界值仍以位模式持久化，读端按 i64 解析
            return shm_object_set_property_int64(alloc, shadow, key, static_cast<int64_t>(v.as<uint64_t>()));
        }
        if (v.is_double()) {
            return shm_object_set_property_double(alloc, shadow, key, v.as<double>());
        }
        if (v.is_string()) {
            auto s = v.as<mc::string>();
            return shm_object_set_property_blob(alloc, shadow, key, std::string_view(s.data(), s.size()),
                                                property_type_tag::string);
        }
        if (v.is_blob()) {
            auto b = v.as<mc::blob>();
            return shm_object_set_property_blob(alloc, shadow, key, std::string_view(b.data.data(), b.data.size()),
                                                property_type_tag::bytes);
        }
        // array / dict / extension：暂不持久化
        return false;
    } catch (...) {
        return false;
    }
}

} // namespace

void shm_sync_property(abstract_object& obj, const mc::variant& value, const property_base& prop) noexcept
{
    auto* shadow = obj.get_shm_handle();
    if (shadow == nullptr) {
        return;
    }
    if (!shm_runtime_provider::has_instance()) {
        return;
    }

    auto& rt    = shm_runtime_provider::instance();
    auto  arena = rt.user_arena();

    auto             key_view = prop.get_name();
    std::string_view name(key_view.data(), key_view.size());

    std::string_view iface{};
    if (auto* itf = prop.get_interface(); itf != nullptr) {
        auto iv = itf->get_interface_name();
        iface   = std::string_view(iv.data(), iv.size());
    }

    auto composite = shm_property_compose_key(iface, name);

    if (auto* itf = prop.get_interface(); itf != nullptr) {
        if (auto* info = itf->get_property_info(prop.get_name()); info != nullptr) {
            if (info->has_flags(MC_REFLECT_FLAG_NOCACHE)) {
                (void)shm_object_set_property_nocache_marker(arena, *shadow, composite);
                return;
            }
        }
    }

    (void)_write_one(arena, *shadow, composite, value);
}

void shm_load_properties_into(abstract_object& obj, const shm_object& sh) noexcept
{
    auto* slab = sh.properties.get();
    if (slab == nullptr || slab->slot_count == 0U) {
        return;
    }

    for (std::uint16_t i = 0; i < slab->slot_count; ++i) {
        const auto& slot   = slab->slots[i];
        auto*       key_bs = slot.key.get();
        if (key_bs == nullptr) {
            continue;
        }

        std::string_view key_sv = key_bs->as_string_view();
        std::string_view iface_sv;
        std::string_view name_sv;
        shm_property_decompose_key(key_sv, iface_sv, name_sv);

        try {
            mc::variant v;
            switch (slot.type) {
                case property_type_tag::int64:
                    v = slot.v_int64;
                    break;
                case property_type_tag::double_:
                    v = slot.v_double;
                    break;
                case property_type_tag::string: {
                    auto* blob = slot.v_blob.get();
                    if (blob == nullptr) {
                        continue;
                    }
                    auto blob_sv = blob->as_string_view();
                    v            = mc::string(blob_sv.data(), blob_sv.size());
                    break;
                }
                case property_type_tag::bytes: {
                    auto* blob = slot.v_blob.get();
                    if (blob == nullptr) {
                        continue;
                    }
                    auto sv = blob->as_string_view();
                    v       = mc::blob(sv.data(), sv.size());
                    break;
                }
                case property_type_tag::null:
                default:
                    continue;
            }

            (void)obj.set_property(name_sv, v, mc::string_view(iface_sv.data(), iface_sv.size()));
        } catch (...) {
            // 单 slot 解析失败不阻断其他 slot 的回填
            continue;
        }
    }
}

#else // MCENGINE_USE_SHM = OFF

void shm_sync_property(abstract_object& /*obj*/, const mc::variant& /*value*/, const property_base& /*prop*/) noexcept
{
    // OFF 模式：m_shm_handle 字段虽然存在但永远是 nullptr，无需任何动作。
}

void shm_load_properties_into(abstract_object& /*obj*/, const shm_object& /*sh*/) noexcept
{
    // OFF 模式：reconstruct 路径不存在，无需回填。
}

#endif // MCENGINE_USE_SHM

} // namespace mc::engine
