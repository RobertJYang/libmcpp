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

/**
 * @file singleton.h
 * @brief 单例模块，提供线程安全的单例功能，支持自定义初始化和销毁
 * 
 * 功能：
 * 1. 支持用户注册初始化函数，可在销毁后重新创建单例
 * 2. 支持泄露和非泄露模式
 *    - 泄露模式：程序退出时不会销毁单例 (singleton_leaky<T>)
 *    - 非泄露模式：程序退出时销毁单例 (singleton<T>)
 * 3. 支持线程安全的访问
 */
#ifndef MC_SINGLETON_H
#define MC_SINGLETON_H

#include "mc/common.h"
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>
#include <unordered_map>
#include <typeindex>
#include <vector>
#include <atomic>
#include <stdexcept>

namespace mc {

/**
 * @brief 单例类型标记
 */
enum class singleton_tag {
    default_tag,  // 默认标签，允许同一类型有多个不同标签的单例
};

/**
 * @brief 单例创建选项
 */
struct singleton_options {
    bool leaky = false;  // 是否为泄露模式单例
};

/**
 * @brief 单例管理器类，管理所有单例的创建和销毁
 */
class singleton_manager {
public:
    using destroy_fn_t = std::function<void()>;
    using type_key_t = std::pair<std::type_index, std::size_t>;
    
    // 单例访问
    static singleton_manager& instance();
    
    // 注册单例销毁函数
    void register_singleton(const type_key_t& key, destroy_fn_t destroy_fn, bool leaky);
    
    // 销毁所有非泄露单例
    void destroy_instances();
    
    // 清空所有单例相关信息（仅用于测试）
    void reset_for_test();

private:
    singleton_manager() = default;
    ~singleton_manager();
    
    struct type_key_hash {
        std::size_t operator()(const type_key_t& key) const {
            return std::hash<std::type_index>()(key.first) ^ std::hash<std::size_t>()(key.second);
        }
    };
    
    // 禁止拷贝和移动
    singleton_manager(const singleton_manager&) = delete;
    singleton_manager& operator=(const singleton_manager&) = delete;
    singleton_manager(singleton_manager&&) = delete;
    singleton_manager& operator=(singleton_manager&&) = delete;
    
    // 成员变量
    std::mutex m_mutex;
    std::unordered_map<type_key_t, destroy_fn_t, type_key_hash> m_non_leaky_instances;
    std::vector<destroy_fn_t> m_destroy_queue;
};

namespace detail {

/**
 * @brief 单例实现细节
 */
template <typename T, typename Tag = singleton_tag>
class singleton_impl {
public:
    // 单例访问函数，支持自定义创建函数
    template <typename... Args>
    static T& get_instance(Args&&... args) {
        auto creator = [&]() { return new T(std::forward<Args>(args)...); };
        return get_instance_with_creator(std::move(creator), singleton_options{});
    }
    
    // 使用自定义创建函数和选项获取单例实例
    template <typename Creator>
    static T& get_instance_with_creator(Creator creator, const singleton_options& options = {}) {
        if (LIKELY(instance_ptr.load(std::memory_order_acquire) != nullptr)) {
            return *instance_ptr.load(std::memory_order_acquire);
        }
        
        std::lock_guard<std::mutex> lock(creation_mutex);
        if (instance_ptr.load(std::memory_order_relaxed) == nullptr) {
            T* instance = creator();
            instance_ptr.store(instance, std::memory_order_release);
            
            // 注册销毁函数
            singleton_manager::type_key_t key = get_type_key();
            auto destroy_fn = [instance]() {
                delete instance;
                instance_ptr.store(nullptr, std::memory_order_release);
            };
            
            singleton_manager::instance().register_singleton(key, destroy_fn, options.leaky);
        }
        
        return *instance_ptr.load(std::memory_order_acquire);
    }
    
    // 重置单例状态（仅用于测试）
    static void reset_for_test() {
        if (instance_ptr.load(std::memory_order_relaxed) != nullptr) {
            std::lock_guard<std::mutex> lock(creation_mutex);
            T* instance = instance_ptr.load(std::memory_order_relaxed);
            if (instance) {
                delete instance;
                instance_ptr.store(nullptr, std::memory_order_release);
            }
        }
    }
    
    // 检查单例是否已创建
    static bool created() {
        return instance_ptr.load(std::memory_order_relaxed) != nullptr;
    }
    
    // 获取单例实例指针（如果存在）
    static T* get_instance_if_created() {
        return instance_ptr.load(std::memory_order_relaxed);
    }

private:
    // 获取类型唯一标识
    static singleton_manager::type_key_t get_type_key() {
        static const std::size_t tag_id = reinterpret_cast<std::size_t>(&tag_id);
        return {std::type_index(typeid(T)), tag_id};
    }
    
    // 禁止构造和析构
    singleton_impl() = delete;
    ~singleton_impl() = delete;
    
    // 类型标识
    static constexpr const char* tag = nullptr;
    
    // 成员变量
    static std::atomic<T*> instance_ptr;
    static std::mutex creation_mutex;
};

template <typename T, typename Tag>
std::atomic<T*> singleton_impl<T, Tag>::instance_ptr{nullptr};

template <typename T, typename Tag>
std::mutex singleton_impl<T, Tag>::creation_mutex;

} // namespace detail

/**
 * @brief 单例类，提供简单的接口获取单例（默认非泄露模式）
 */
template <typename T, typename Tag = singleton_tag>
class singleton {
public:
    /**
     * @brief 获取单例实例
     * @param args 传递给构造函数的参数
     * @return 单例实例引用
     */
    template <typename... Args>
    static T& instance(Args&&... args) {
        auto creator = [&]() { return new T(std::forward<Args>(args)...); };
        return instance_with_creator(std::move(creator));
    }
    
    /**
     * @brief 使用自定义创建函数获取单例实例
     * @param creator 创建函数，返回T*
     * @return 单例实例引用
     */
    template <typename Creator>
    static T& instance_with_creator(Creator creator) {
        singleton_options options;
        options.leaky = false; // 非泄露模式
        return detail::singleton_impl<T, Tag>::get_instance_with_creator(std::move(creator), options);
    }
    
    /**
     * @brief 重置单例状态（仅用于测试）
     */
    static void reset_for_test() {
        detail::singleton_impl<T, Tag>::reset_for_test();
    }
    
    /**
     * @brief 检查单例是否已创建
     * @return true如果单例已创建，否则false
     */
    static bool created() {
        return detail::singleton_impl<T, Tag>::created();
    }
    
    /**
     * @brief 获取单例实例指针（如果存在）
     * @return 单例实例指针，如果未创建返回nullptr
     */
    static T* try_get() {
        return detail::singleton_impl<T, Tag>::get_instance_if_created();
    }

private:
    singleton() = delete;
    ~singleton() = delete;
};

/**
 * @brief 泄露单例类，提供简单的接口获取泄露模式的单例
 */
template <typename T, typename Tag = singleton_tag>
class singleton_leaky {
public:
    /**
     * @brief 获取单例实例（泄露模式）
     * @param args 传递给构造函数的参数
     * @return 单例实例引用
     */
    template <typename... Args>
    static T& instance(Args&&... args) {
        auto creator = [&]() { return new T(std::forward<Args>(args)...); };
        return instance_with_creator(std::move(creator));
    }
    
    /**
     * @brief 使用自定义创建函数获取单例实例（泄露模式）
     * @param creator 创建函数，返回T*
     * @return 单例实例引用
     */
    template <typename Creator>
    static T& instance_with_creator(Creator creator) {
        singleton_options options;
        options.leaky = true; // 泄露模式
        return detail::singleton_impl<T, Tag>::get_instance_with_creator(std::move(creator), options);
    }
    
    /**
     * @brief 重置单例状态（仅用于测试）
     */
    static void reset_for_test() {
        detail::singleton_impl<T, Tag>::reset_for_test();
    }
    
    /**
     * @brief 检查单例是否已创建
     * @return true如果单例已创建，否则false
     */
    static bool created() {
        return detail::singleton_impl<T, Tag>::created();
    }
    
    /**
     * @brief 获取单例实例指针（如果存在）
     * @return 单例实例指针，如果未创建返回nullptr
     */
    static T* try_get() {
        return detail::singleton_impl<T, Tag>::get_instance_if_created();
    }

private:
    singleton_leaky() = delete;
    ~singleton_leaky() = delete;
};

/**
 * @brief 清空所有单例（仅用于测试）
 */
inline void reset_singletons_for_test() {
    singleton_manager::instance().reset_for_test();
}

} // namespace mc

#endif // MC_SINGLETON_H 