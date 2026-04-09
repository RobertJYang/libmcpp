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

#ifndef MC_VARIANT_COPY_CONTEXT_H
#define MC_VARIANT_COPY_CONTEXT_H

#include <mc/common.h>
#include <mc/memory.h>
#include <unordered_map>

namespace mc::detail {

/**
 * @brief 深拷贝上下文
 *
 * 用于在递归深拷贝过程中：
 * 1. 检测循环引用，避免无限递归
 * 2. 记录已拷贝对象的映射关系，遇到循环时返回已拷贝的对象
 * 3. 保持拷贝后的引用关系与原始对象一致
 *
 * 设计原则：
 * - 本类仅作为映射记录器，不持有对象的所有权（不增加引用计数）
 * - 假设：copy_context 是 deep_copy 的局部变量，生命周期严格短于被记录的对象
 * - 保证：所有被记录的对象在深拷贝过程中都被返回值或 variant 持有，引用计数 >= 1
 * - 性能：避免了不必要的引用计数操作（add_ref/release_ref）
 *
 * 使用场景：
 * - variant::deep_copy() - 递归拷贝 variant
 * - dict::deep_copy() - 递归拷贝 dict
 * - variants::deep_copy() - 递归拷贝 array
 * - variant_extension::deep_copy() - 递归拷贝扩展类型
 *
 * 循环引用处理策略：
 * - 遇到已访问的对象时，返回已拷贝的对象，保持引用关系
 * - 不抛出异常，深拷贝结果与原始对象保持相同的拓扑结构
 *
 * @example
 * ```cpp
 * // 自引用场景
 * dict d1;
 * d1["self"] = d1;
 *
 * copy_context ctx;
 * dict d2 = d1.deep_copy_impl(ctx);
 * // d2["self"] 指向 d2 自己，保持了引用关系
 * ```
 */
class MC_API copy_context {
public:
    /**
     * @brief 默认构造函数
     */
    copy_context() = default;

    /**
     * @brief 禁止拷贝
     */
    copy_context(const copy_context&)            = delete;
    copy_context& operator=(const copy_context&) = delete;

    /**
     * @brief 允许移动
     */
    copy_context(copy_context&&) noexcept            = default;
    copy_context& operator=(copy_context&&) noexcept = default;

    /**
     * @brief 析构函数，释放所有引用计数
     */
    ~copy_context();

    /**
     * @brief 检查对象是否已经被拷贝
     */
    template <typename T>
    bool has_copied(const T* ptr) const;

    /**
     * @brief 获取已拷贝的对象
     *
     * @tparam T 对象类型（必须继承自 mc::memory::shared_base）
     * @param ptr 原始对象指针
     * @return shared_ptr<T> 已拷贝的对象，如果不存在返回 nullptr
     */
    template <typename T>
    mc::shared_ptr<T> get_copied(const T* ptr) const;

    /**
     * @brief 记录已拷贝的对象
     *
     * @tparam T 对象类型（必须继承自 mc::memory::shared_base）
     * @param original_ptr 原始对象指针
     * @param copied_ptr 拷贝后的对象 shared_ptr
     */
    template <typename T>
    void record_copied(const T* original_ptr, const mc::shared_ptr<T>& copied_ptr);

    /**
     * @brief 清空所有记录，释放所有引用计数
     */
    void clear();

    /**
     * @brief 获取已拷贝对象数量
     */
    size_t size() const;

    /**
     * @brief 判断是否为空
     */
    bool empty() const;

private:
    bool                     has_copied_impl(const void* ptr) const;
    mc::memory::shared_base* get_copied_impl(const void* ptr) const;
    void                     record_copied_impl(const void* original_ptr, mc::memory::shared_base* copied_base);

    std::unordered_map<const void*, mc::memory::shared_base*> m_copied_objects;
};

template <typename T>
bool copy_context::has_copied(const T* ptr) const
{
    return has_copied_impl(static_cast<const void*>(ptr));
}

template <typename T>
mc::shared_ptr<T> copy_context::get_copied(const T* ptr) const
{
    static_assert(std::is_base_of_v<mc::memory::shared_base, T>, "T must inherit from mc::memory::shared_base");

    mc::memory::shared_base* base = get_copied_impl(ptr);
    if (!base) {
        return nullptr;
    }

    return mc::shared_ptr<T>(static_cast<T*>(base));
}

template <typename T>
void copy_context::record_copied(const T* original_ptr, const mc::shared_ptr<T>& copied_ptr)
{
    static_assert(std::is_base_of_v<mc::memory::shared_base, T>, "T must inherit from mc::memory::shared_base");

    if (!original_ptr || !copied_ptr) {
        return;
    }

    record_copied_impl(original_ptr, copied_ptr.get());
}

} // namespace mc::detail

#endif // MC_VARIANT_COPY_CONTEXT_H
