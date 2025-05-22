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
 * @file path_iterator.h
 * @brief 高效路径迭代器
 */
#ifndef MC_ENGINE_PATH_ITERATOR_H
#define MC_ENGINE_PATH_ITERATOR_H

#include <string_view>

namespace mc::engine {

/**
 * @brief 路径分隔符
 */
constexpr char IMMUTABLE_PATH_SEP = '/';

/**
 * @brief 路径迭代器类
 *
 * 用于高效遍历路径的各个段，支持前向和后向迭代
 */
class path_iterator {
public:
    /**
     * @brief 构造函数
     * @param path 要遍历的路径
     */
    explicit path_iterator(std::string_view path);

    /**
     * @brief 重置迭代器到初始状态
     */
    void reset();

    /**
     * @brief 移动到下一个路径段
     * @return 如果成功移动则返回true，如果已经到达最后一个段则返回false
     */
    bool to_next();

    /**
     * @brief 移动到前一个路径段
     * @return 如果成功移动则返回true，如果已经在第一个段则返回false
     */
    bool to_prev();

    /**
     * @brief 获取当前路径段
     * @return 当前路径段，如果迭代器未初始化或路径为空则返回空字符串
     */
    std::string_view current() const;

    /**
     * @brief 获取父路径
     * @return 父路径，如果迭代器未初始化或路径为空则返回空字符串
     */
    std::string_view parent_path() const;

    /**
     * @brief 检查路径是否为空或根路径
     * @return 如果路径为空或为根路径('/')则返回true
     */
    bool is_empty_or_root_path() const;

private:
    /**
     * @brief 向前跳过分隔符
     * @param pos 起始位置
     * @return 跳过分隔符后的位置
     */
    size_t skip_separators_forward(size_t pos) const;

    /**
     * @brief 向后跳过分隔符
     * @param pos 起始位置
     * @return 跳过分隔符后的位置
     */
    size_t skip_separators_backward(size_t pos) const;

    /**
     * @brief 查找路径段的结束位置
     * @param start_pos 段的起始位置
     * @return 段的结束位置
     */
    size_t find_segment_end(size_t start_pos) const;

    /**
     * @brief 查找前一个路径段的起始位置
     * @param pos 当前位置
     * @return 前一个段的起始位置
     */
    size_t find_prev_segment_start(size_t pos) const;

    /**
     * @brief 查找下一个路径段
     * @param start 输出参数，下一个段的起始位置
     * @param end 输出参数，下一个段的结束位置
     * @return 如果找到下一个段则返回true
     */
    bool find_next_segment(size_t& start, size_t& end) const;

    /**
     * @brief 查找前一个路径段
     * @param start 输出参数，前一个段的起始位置
     * @param end 输出参数，前一个段的结束位置
     * @return 如果找到前一个段则返回true
     */
    bool find_prev_segment(size_t& start, size_t& end) const;

    // 成员变量
    std::string_view m_path;           // 原始路径
    size_t           m_start;          // 当前段的起始位置
    size_t           m_end;            // 当前段的结束位置
    bool             m_is_initialized; // 是否已初始化
};

} // namespace mc::engine

#endif // MC_ENGINE_PATH_ITERATOR_H
