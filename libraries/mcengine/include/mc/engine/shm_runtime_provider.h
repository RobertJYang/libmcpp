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

#ifndef MC_ENGINE_SHM_RUNTIME_PROVIDER_H
#define MC_ENGINE_SHM_RUNTIME_PROVIDER_H

#include <memory>

#include <mc/common.h>
#include <mc/shm/shm_runtime.h>
#include <mc/string_view.h>

// mc::engine::shm_runtime_provider
//
// mcengine 在 USE_SHM=ON 下接入 shm_runtime 的统一入口。
//
// 设计契约：
//   - 单进程内只有一个 shm_runtime 实例（lazy 创建），所有 service /
//     object_table / class_factory 共享该实例
//   - region_name 决定了哪几个进程"看到同一份 SHM 数据"。运行期由
//     install_runtime() 显式注入；缺省按 MC_SHM_REGION 环境变量 /
//     "mcengine.default" 推断
//   - 所有 mcengine 测试都用 reset_for_test() 重置 provider；测试用例可在
//     SetUp 阶段先 install 一个独占 runtime 实例避免 region name 冲突
//
// 调用顺序：
//   1. （可选）install_runtime(custom_runtime)  — 应用启动 / 测试 SetUp
//   2. instance() 拿到 shm_runtime& — 由 service / object_table 在 alloc 构造期调用
//   3. （测试）reset_for_test() — 释放 provider 持有的 runtime
//
// 错误语义：
//   - install_runtime(nullptr) → 无效，忽略
//   - 已 install 后再次 install → 覆盖（旧 runtime 由 caller 决定何时析构）
//   - lazy 创建失败（region 分配失败、name 冲突等）→ 抛 invalid_arg_exception，
//     调用栈顶的 service::add 会冒泡

namespace mc::engine {

class MC_API shm_runtime_provider {
public:
    // 拿到当前 mcengine 共享的 shm_runtime 实例。未 install 且未 lazy 初始化时
    // 触发 lazy 创建（region_name 选取逻辑见上方）。
    static mc::shm::shm_runtime& instance();

    // 返回当前是否已经存在 runtime（不会触发 lazy 创建）。
    static bool has_instance() noexcept;

    // 显式注入一个外部持有的 runtime。adopt=true 时 provider 接管 ownership
    // （shared_ptr）；adopt=false 时仅记录裸引用，由 caller 保证生命周期 >=
    // mcengine 进程使用周期。
    static void install_runtime(std::shared_ptr<mc::shm::shm_runtime> runtime) noexcept;

    // 测试辅助：清空 provider 持有的 runtime，下次 instance() 走 lazy 创建。
    // 不影响外部仍持有的 shared_ptr<shm_runtime>。
    static void reset_for_test() noexcept;

    // 默认 region_name 推断：MC_SHM_REGION 环境变量 → 编译期常量 fallback
    static mc::string_view default_region_name() noexcept;

private:
    shm_runtime_provider() = delete;
};

}  // namespace mc::engine

#endif  // MC_ENGINE_SHM_RUNTIME_PROVIDER_H
