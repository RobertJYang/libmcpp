/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_IM_REF_COUNTED_H
#define MC_IM_REF_COUNTED_H

#include <atomic>
#include <cstddef>

namespace mc::im {

/**
 * 引用计数基类，提供引用计数管理能力
 */
class ref_base {
public:
    ref_base() = default;

    virtual ~ref_base() = default;

    // ref_base 的生命周期由 ref_ptr 管理，拷贝构造函数也是初始引用计数为0
    ref_base(const ref_base&) : m_ref_count(0) {
    }
    ref_base& operator=(const ref_base&) {
        return *this;
    }
    ref_base(ref_base&&) noexcept : m_ref_count(0) {
    }
    ref_base& operator=(ref_base&&) noexcept {
        return *this;
    }

    // 增加引用计数
    void add_ref() const {
        m_ref_count.fetch_add(1, std::memory_order_relaxed);
    }

    // 减少引用计数，如果引用计数为0则返回true
    bool release_ref() const {
        return m_ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1;
    }

    // 获取当前引用计数
    size_t ref_count() const {
        return m_ref_count.load(std::memory_order_relaxed);
    }

private:
    mutable std::atomic<size_t> m_ref_count{0};
};

} // namespace mc::im

#endif // MC_IM_REF_COUNTED_H