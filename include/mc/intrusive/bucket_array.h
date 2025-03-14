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

/**
 * @file bucket_array.h
 * @brief 哈希表桶数组管理类
 */
#ifndef MC_INTRUSIVE_BUCKET_ARRAY_H
#define MC_INTRUSIVE_BUCKET_ARRAY_H

#include <memory>
#include <vector>

namespace mc {
namespace intrusive {

/**
 * @brief 哈希表桶数组管理类
 *
 * 该类负责管理哈希表的桶数组，自动处理内存分配和释放
 */
class bucket_array {
public:
    /**
     * @brief 构造函数
     *
     * @param bucket_count 桶数量，至少为1
     */
    explicit bucket_array(std::size_t bucket_count = 1) {
        // 确保桶数量至少为1
        m_bucket_count = std::max<std::size_t>(1, bucket_count);
        m_buckets.resize(m_bucket_count);
    }

    // 禁止拷贝
    bucket_array(const bucket_array&)            = delete;
    bucket_array& operator=(const bucket_array&) = delete;

    // 允许移动
    bucket_array(bucket_array&& other) noexcept
        : m_buckets(std::move(other.m_buckets)), m_bucket_count(other.m_bucket_count) {
        other.m_bucket_count = 0;
    }

    bucket_array& operator=(bucket_array&& other) noexcept {
        if (this != &other) {
            m_buckets            = std::move(other.m_buckets);
            m_bucket_count       = other.m_bucket_count;
            other.m_bucket_count = 0;
        }
        return *this;
    }

    /**
     * @brief 获取桶数组指针
     *
     * @return 桶数组指针
     */
    void** data() {
        return m_buckets.data();
    }

    /**
     * @brief 获取桶数量
     *
     * @return 桶数量
     */
    std::size_t size() const {
        return m_bucket_count;
    }

private:
    std::vector<void*> m_buckets;
    std::size_t        m_bucket_count;
};

} // namespace intrusive
} // namespace mc

#endif // MC_INTRUSIVE_BUCKET_ARRAY_H