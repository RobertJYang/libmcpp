/*
* Copyright (c) 2023 Huawei Technologies Co., Ltd.
* openUBMC is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*         http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

#ifndef MC_DATABASE_OBJECT_H
#define MC_DATABASE_OBJECT_H

#include <mc/database/common.h>
#include <mc/database/property.h>
#include <mc/reflect/reflect.h>
#include <iostream>

namespace mc::database {

// 对象基类
class db_object_base {
protected:
    path m_path;
    void* m_shared_data{nullptr};
    
    virtual void sync_from_shared_memory() = 0;
    
public:
    db_object_base() = default;
    virtual ~db_object_base() = default;
    
    path_view path() const { return m_path; }
    
    // 设置对象路径
    void set_path(const database::path& p) { m_path = p; }
};

// 统一的对象模板
template<typename T>
class db_object : public db_object_base {
public:
    using self_type = db_object<T>;
    
    // 构造函数
    db_object() = default;
    
    // 析构函数
    ~db_object() override = default;
    
    // 刷新对象（从共享内存获取最新数据）
    void refresh() {
        if (m_shared_data) {
            sync_from_shared_memory();
        }
    }
    
    // 从共享内存同步
    void sync_from_shared_memory() override {
        std::cout << "同步数据: " << m_path << std::endl;
        // 实际实现会从共享内存读取数据，这里只是简单实现
    }
    
    // 更新属性
    void update_property(const char* name, const mc::variant& value) {
        std::cout << "更新属性: " << name << " = " << "[variant值]" << std::endl;
        // 实际实现会将更改写入共享内存
    }
    
    // 注销对象
    error_code unregister() {
        std::cout << "注销对象: " << m_path << std::endl;
        return error_code::success;
    }
    
    // 访问属性
    template<typename PropType>
    PropType& get_property(const char* name) {
        // 在实际实现中，这里应该查找属性表
        // 此处仅为示例实现
        return *(PropType*)nullptr;
    }
    
    // 初始化属性，将属性与对象关联
    template<typename PropType>
    void init_property(property<PropType>& prop, const char* name) {
        // 设置属性名称和父对象引用
        prop.m_name = name;
        prop.m_parent = this;
    }
    
    // 初始化所有属性，通过反射自动关联
    void init_properties() {
        // 由于我们不确定是否有反射支持，简单地使用一个静态断言来检查
        // 实际项目中可以使用更复杂的反射检测机制
        auto& self = static_cast<T&>(*this);
        
        // 如果有反射支持，使用反射来初始化属性
        mc::reflect::reflector<T>::visit([&](const char* name, auto getter, auto) {
            using PropType = std::decay_t<decltype(getter(self))>;
            if constexpr (is_property<PropType>::value) {
                init_property(getter(self), name);
            }
        });
    }

private:
    // 检测是否为property类型的辅助模板
    template<typename U>
    struct is_property : std::false_type {};
    
    template<typename U>
    struct is_property<property<U>> : std::true_type {};
    
    // 其他私有成员
    // ... 
    
    friend class reflect::reflector<T>;
};

} // namespace mc::database

#endif // MC_DATABASE_OBJECT_H 