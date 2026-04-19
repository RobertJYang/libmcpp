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

#include <mc/common.h>
#include <mc/variant.h>

// mc::engine::shm_sync_property
//
// 把 property<T> 写路径产生的新值，下沉到 abstract_object::m_shm_handle 指向的
// shm_object（SHM 是真理之源）。
//
// 触发点：object_impl::notify_property_update_shm（property::set_value 链路）
//
// 行为契约：
//   - obj.get_shm_handle() == nullptr：no-op（OFF 模式 / 尚未 ensure_shm_handle）
//   - 不支持的 variant 类型：silently skip
//   - 失败（容量分配失败等）：silently skip；CRC/状态保持不变
//   - noexcept；任何异常均被吞掉，避免污染业务 set_value 路径
//
// USE_SHM=OFF 编译时，本符号仍可链接，但会因为 m_shm_handle 永远是 nullptr 而
// 提前返回；零额外开销。

namespace mc::engine {

class abstract_object;
class property_base;
struct shm_object;

MC_API void shm_sync_property(abstract_object&     obj,
                              const mc::variant&   value,
                              const property_base& prop) noexcept;

// 反向同步：从 sh.properties slab 把所有 property 值灌回 obj。
// 用于 reconstruct_fn 路径——cold-attach / takeover / recover 时把 SHM 真理之源
// 的 property 值回填到刚 new 出来的 abstract_object，避免重建出空壳对象。
//
// 行为契约：
//   - sh 必须是 valid 的 shm_object（CRC / isolated 检查在调用方完成）
//   - 不存在的 property name 静默跳过（class schema 演化容错：旧字段已删）
//   - tag/类型不被 abstract_object 识别时，跳过该 slot 而不是失败
//   - noexcept；任何异常吞掉，不污染 recover 主循环
//
// USE_SHM=OFF 时本符号是空 stub。
MC_API void shm_load_properties_into(abstract_object& obj, const shm_object& sh) noexcept;

}  // namespace mc::engine

#endif  // MC_ENGINE_SHM_PROPERTY_SYNC_H
