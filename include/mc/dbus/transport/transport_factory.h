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

#ifndef MC_DBUS_TRANSPORT_FACTORY_H
#define MC_DBUS_TRANSPORT_FACTORY_H

#include <mc/dbus/address.h>
#include <mc/dbus/transport/transport.h>
#include <mc/singleton.h>

namespace mc::dbus {

/**
 * @brief 传输层工厂类
 *
 * 用于根据地址创建 transport 对象
 */
class transport_factory {
public:
    using io_context_type = transport::io_context_type;
    using creator_type    = std::function<transport_ptr(io_context_type&, address_entry_ptr)>;

    transport_factory();
    ~transport_factory();

    static transport_factory& get_instance() {
        return mc::singleton<transport_factory>::instance();
    }

    /**
     * @brief 注册传输层创建器
     * @param method 传输层方法
     * @param creator 创建器
     */
    bool register_creator(std::string method, creator_type creator);

    /**
     * @brief 创建传输层对象
     * @param io_context IO上下文
     * @param address_str 地址字符串
     * @return 传输层对象
     * @throws mc::bad_value_exception 如果地址字符串格式错误或不支持的传输类型
     */
    transport_ptr create(io_context_type& io_context, std::string_view address_str);

    /**
     * @brief 根据地址条目创建传输层对象
     * @param io_context IO上下文
     * @param entry 地址条目
     * @return 传输层对象，如果不支持的传输类型则返回nullptr
     * @throws mc::bad_value_exception 如果地址条目格式错误或缺少必要参数
     */
    transport_ptr create(io_context_type& io_context, address_entry_ptr entry);

private:
    std::map<std::string, creator_type> m_creators;
};

} // namespace mc::dbus

#endif // MC_DBUS_TRANSPORT_FACTORY_H