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

#include <mc/reflect/metadata.h>
#include <mc/sync/mutex_box.h>
#include <mc/sync/shared_mutex.h>

namespace mc::reflect {

static bool is_continuous_enum(const enum_member_info* values, size_t count) {
    if (count == 0) {
        return true;
    }

    auto first_value = values[0].value;
    for (size_t i = 1; i < count; ++i) {
        if (static_cast<size_t>(values[i].value) != i + first_value) {
            return false;
        }
    }

    return true;
}

struct enum_metadata::impl {
    using name_to_value_map_t = std::unordered_map<std::string_view, enum_value_type>;
    using value_to_name_map_t = std::unordered_map<enum_value_type, std::string_view>;
    struct data_t {
        mutable std::unique_ptr<name_to_value_map_t> name_to_value;
        mutable std::unique_ptr<value_to_name_map_t> value_to_name;
    };

    std::optional<std::string_view> get_name(enum_value_type value) const;
    std::optional<enum_value_type>  get_value(std::string_view name) const;

    void init_value_to_name_map(const data_t& data) const;
    void init_name_to_value_map(const data_t& data) const;

    std::optional<std::string_view> get_name_from_cache(const data_t& data, enum_value_type value) const;
    std::optional<enum_value_type>  get_value_from_cache(const data_t& data, std::string_view name) const;

    std::string_view        name;
    const enum_member_info* values;
    size_t                  count;
    bool                    is_continuous; // 是否连续

    mc::mutex_box<data_t, mc::shared_mutex> data;
};

void enum_metadata::impl::init_value_to_name_map(const data_t& data) const {
    if (data.value_to_name) {
        return;
    }

    data.value_to_name = std::make_unique<value_to_name_map_t>();
    for (size_t i = 0; i < count; ++i) {
        data.value_to_name->emplace(values[i].value, values[i].name);
    }
}

void enum_metadata::impl::init_name_to_value_map(const data_t& data) const {
    if (data.name_to_value) {
        return;
    }

    data.name_to_value = std::make_unique<name_to_value_map_t>();
    for (size_t i = 0; i < count; ++i) {
        data.name_to_value->emplace(values[i].name, values[i].value);
    }
}

std::optional<std::string_view> enum_metadata::impl::get_name_from_cache(const data_t& data, enum_value_type value) const {
    auto it = data.value_to_name->find(value);
    if (it == data.value_to_name->end()) {
        return std::nullopt;
    }

    return it->second;
}

std::optional<enum_value_type> enum_metadata::impl::get_value_from_cache(const data_t& data, std::string_view name) const {
    auto it = data.name_to_value->find(name);
    if (it == data.name_to_value->end()) {
        return std::nullopt;
    }

    return it->second;
}

std::optional<std::string_view> enum_metadata::impl::get_name(enum_value_type value) const {
    if (count == 0) {
        return std::nullopt;
    }

    if (is_continuous) {
        // 连续枚举，直接查数组返回
        if (value < values[0].value || value > values[count - 1].value) {
            return std::nullopt;
        }
        return values[value - values[0].value].name;
    }

    // 非连续枚举，从缓存映射表中查找
    return data.with_rlock_ptr([this, value](auto locked_ptr) -> std::optional<std::string_view> {
        if (locked_ptr->value_to_name) {
            return get_name_from_cache(*locked_ptr, value);
        }

        // 还没有构建缓存映射表，先解锁读锁再重新拿写锁构建缓存映射表
        locked_ptr.unlock();
        return data.with_lock([this, value](auto& data) -> std::optional<std::string_view> {
            if (!data.value_to_name) {
                // 第一次构造 value_to_name 映射表
                init_value_to_name_map(data);
            }

            return get_name_from_cache(data, value);
        });
    });
}

std::optional<enum_value_type> enum_metadata::impl::get_value(std::string_view enum_name) const {
    // 由于采用了延迟初始化方案，所以需要加锁等待。
    // 其实一个简单的原子指针操作就可以了，因为枚举缓存一旦构建就不会再变化，但目前还是加个读锁是考虑到动态模块中
    // 实现的枚举，在动态模块卸载时枚举元数据也会跟着销毁，加读锁可以保证析构函数会等待所有正在访问的线程返回才销毁。
    // TODO:: 理论上在动态模块卸载前要保证所有与动态模块有关的资源都应该提前销毁，到时候这里不用再加锁。
    return data.with_rlock_ptr([this, enum_name](auto locked_ptr) -> std::optional<enum_value_type> {
        if (locked_ptr->name_to_value) {
            return get_value_from_cache(*locked_ptr, enum_name);
        }

        // 还没有构建缓存映射表，先解锁读锁再重新拿写锁构建缓存映射表
        locked_ptr.unlock();
        return data.with_lock([this, enum_name](auto& data) -> std::optional<enum_value_type> {
            if (!data.name_to_value) {
                init_name_to_value_map(data);
            }

            return get_value_from_cache(data, enum_name);
        });
    });
}

enum_metadata::enum_metadata(std::string_view name, enum_values values)
    : m_impl(std::make_unique<impl>()) {
    m_impl->name          = name;
    m_impl->values        = values.members;
    m_impl->count         = values.count;
    m_impl->is_continuous = is_continuous_enum(values.members, values.count);
}

enum_metadata::~enum_metadata() {
    m_impl->data.with_lock([](auto& data) {
        data.name_to_value.reset();
        data.value_to_name.reset();
    });
}

enum_value_type enum_metadata::get_value(std::string_view name) const {
    auto value = m_impl->get_value(name);
    if (!value) {
        throw_enum_value_not_found(m_impl->name, name);
    }

    return *value;
}

enum_value_type enum_metadata::get_value_from_variant(const mc::variant& var) const {
    auto value = try_get_value_from_variant(var);
    if (!value) {
        throw_enum_value_not_found(m_impl->name, var.to_string());
    }

    return *value;
}

std::string_view enum_metadata::get_name(enum_value_type value) const {
    auto name = m_impl->get_name(value);
    if (!name) {
        throw_enum_value_not_found(m_impl->name, value);
    }

    return *name;
}

std::optional<enum_value_type> enum_metadata::try_get_value(std::string_view name) const {
    return m_impl->get_value(name);
}

std::optional<enum_value_type> enum_metadata::try_get_value_from_variant(const mc::variant& var) const {
    if (var.is_integer()) {
        auto enum_value = static_cast<enum_value_type>(var.as_uint64());
        if (has_value(enum_value)) {
            return enum_value;
        }
    } else if (var.is_string()) {
        auto value = m_impl->get_value(var.get_string());
        if (value) {
            return value;
        }

        // 尝试将字符串转换成整数
        auto enum_value = var.try_as<enum_value_type>();
        if (enum_value && has_value(*enum_value)) {
            return enum_value;
        }
    }

    return std::nullopt;
}

std::optional<std::string_view> enum_metadata::try_get_name(enum_value_type value) const {
    return m_impl->get_name(value);
}

bool enum_metadata::has_value(std::string_view name) const {
    return m_impl->get_value(name).has_value();
}

bool enum_metadata::has_value(enum_value_type value) const {
    return m_impl->get_name(value).has_value();
}

std::vector<std::string_view> enum_metadata::get_names() const {
    std::vector<std::string_view> names;
    names.reserve(m_impl->count);
    for (size_t i = 0; i < m_impl->count; ++i) {
        names.push_back(m_impl->values[i].name);
    }

    return names;
}

enum_values enum_metadata::get_values() const {
    return enum_values{m_impl->values, m_impl->count};
}

} // namespace mc::reflect