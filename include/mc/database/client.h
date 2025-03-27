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

#ifndef MC_DATABASE_CLIENT_H
#define MC_DATABASE_CLIENT_H

#include <mc/database/common.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace mc {
namespace database {

// 前向声明
template<typename T>
class db_object;
class db_object_base;

class client {
public:
    // 回调函数类型
    using completion_handler = std::function<void(error_code)>;
    using string_list_handler = std::function<void(error_code, const std::vector<std::string>&)>;
    using boolean_handler = std::function<void(error_code, bool)>;

    // 构造和析构
    client();
    ~client();

    // 移动构造和赋值
    client(client&& other) noexcept;
    client& operator=(client&& other) noexcept;

    // 禁止复制
    client(const client&) = delete;
    client& operator=(const client&) = delete;

    // 连接方法
    error_code connect();
    error_code disconnect();
    bool is_connected() const;
    void async_connect(completion_handler handler);
    void async_disconnect(completion_handler handler);

    // 对象创建和查询
    template<typename T>
    error_code create(const path& object_path);

    template<typename T>
    std::shared_ptr<T> query(const path& object_path, error_code& ec);

    // 异步对象创建和查询
    template<typename T>
    void async_create(const path& object_path, completion_handler handler);

    template<typename T>
    void async_query(const path& object_path, 
                   std::function<void(error_code, std::shared_ptr<T>)> handler);

    // 对象删除和检查
    error_code remove(const path& object_path);
    bool exists(const path& object_path, error_code& ec);
    void async_remove(const path& object_path, completion_handler handler);
    void async_exists(const path& object_path, boolean_handler handler);

    // 子对象列表
    std::vector<std::string> list_children(const path& parent_path, error_code& ec);
    void async_list_children(const path& parent_path, string_list_handler handler);

private:
    // 实现类
    class impl {
    private:
        bool m_connected = false;
        std::unordered_map<std::string, std::shared_ptr<db_object_base>> m_objects;

    public:
        impl() = default;
        ~impl() = default;

        // 连接操作
        error_code connect() {
            m_connected = true;
            return error_code::success;
        }

        error_code disconnect() {
            if (!m_connected) {
                return error_code::not_initialized;
            }
            m_connected = false;
            return error_code::success;
        }

        bool is_connected() const {
            return m_connected;
        }

        // 对象管理
        template<typename T>
        error_code create_object(const path& object_path) {
            if (!is_connected()) {
                return error_code::not_initialized;
            }

            if (object_path.empty() || object_path[0] != '/') {
                return error_code::invalid_path;
            }

            std::string path_str = std::string(object_path);
            if (m_objects.find(path_str) != m_objects.end()) {
                return error_code::already_exists;
            }

            // 在实际实现中，这里应该与服务器通信
            auto obj = std::make_shared<T>();
            m_objects[path_str] = obj;
            
            return error_code::success;
        }

        template<typename T>
        std::shared_ptr<T> query_object(const path& object_path, error_code& ec) {
            if (!is_connected()) {
                ec = error_code::not_initialized;
                return nullptr;
            }

            if (object_path.empty() || object_path[0] != '/') {
                ec = error_code::invalid_path;
                return nullptr;
            }

            std::string path_str = std::string(object_path);
            auto it = m_objects.find(path_str);
            if (it == m_objects.end()) {
                ec = error_code::not_found;
                return nullptr;
            }

            // 在实际实现中，这里应该与服务器通信
            // 此处简单地假设类型转换一定成功
            ec = error_code::success;
            return std::static_pointer_cast<T>(it->second);
        }

        // 删除对象
        error_code remove_object(const path& object_path);

        // 检查对象是否存在
        bool exists_object(const path& object_path, error_code& ec);

        // 列出子对象
        std::vector<std::string> list_child_objects(const path& parent_path, error_code& ec);
    };
    
    std::unique_ptr<impl> m_impl;
};

// 模板方法实现
template<typename T>
error_code client::create(const path& object_path) {
    return m_impl->create_object<T>(object_path);
}

template<typename T>
std::shared_ptr<T> client::query(const path& object_path, error_code& ec) {
    return m_impl->query_object<T>(object_path, ec);
}

// 模板方法异步版本实现
template<typename T>
void client::async_create(const path& object_path, completion_handler handler) {
    error_code ec = create<T>(object_path);
    handler(ec);
}

template<typename T>
void client::async_query(const path& object_path, 
                       std::function<void(error_code, std::shared_ptr<T>)> handler) {
    error_code ec;
    auto obj = query<T>(object_path, ec);
    handler(ec, obj);
}

} // namespace database
} // namespace mc

#endif // MC_DATABASE_CLIENT_H 