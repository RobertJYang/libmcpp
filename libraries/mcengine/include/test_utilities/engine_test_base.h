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

#ifndef MC_TEST_UTILITIES_ENGINE_TEST_BASE_H
#define MC_TEST_UTILITIES_ENGINE_TEST_BASE_H

#include <mc/engine.h>

#include <test_utilities/base.h>

#if MCENGINE_USE_SHM
#include <mc/common.h>
#include <mc/engine/message.h>
#include <mc/engine/service_proto.h>
#include <mc/shm/message_queue/mq_channel.h>
#include <mc/shm/message_queue/mq_proto.h>
#include <mc/shm/message_queue/mq_transport_proto.h>
#include <mc/shm/shm_runtime.h>
#include <mc/string.h>
#include <mc/string_view.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#endif

namespace mc {
namespace test {

// Engine 测试基座；开启 SHM 时每个 case 使用独占 region。
class MC_API TestWithEngine : public TestWithRuntime {
protected:
    static mc::engine::engine& get_engine()
    {
        return mc::engine::get_engine();
    }

    static void SetUpTestSuite()
    {
        mc::engine::engine::reset_for_test();
        TestWithRuntime::SetUpTestSuite();
    }

    static void TearDownTestSuite()
    {
        TestWithRuntime::TearDownTestSuite();
        mc::engine::engine::reset_for_test();
    }

#if MCENGINE_USE_SHM
    void SetUp() override;
    void TearDown() override;

    std::shared_ptr<mc::shm::shm_runtime> runtime_alias() const;

    static std::optional<mc::shm::endpoint> register_running_endpoint(mc::shm::shm_runtime& runtime,
                                                                      mc::string_view       name);

    std::unique_ptr<mc::shm::shm_runtime> open_child_shm_runtime() const;

    void fork_child(const std::function<int()>& body) const;

    struct MC_API mq_rx_pipeline {
        mq_rx_pipeline();
        ~mq_rx_pipeline();

        mq_rx_pipeline(const mq_rx_pipeline&)            = delete;
        mq_rx_pipeline& operator=(const mq_rx_pipeline&) = delete;

        void start(std::shared_ptr<mc::shm::shm_runtime> runtime, const mc::shm::endpoint& ep,
                   std::uint32_t instance_id);
        void stop();

        mc::engine::service_proto   proto;
        mc::shm::mq_proto           mq;
        mc::shm::mq_transport_proto transport;
        mc::shm::mq_channel         channel;

    private:
        bool m_started{false};
    };

    static void send_via_mq(mc::shm::shm_runtime& runtime, const mc::shm::endpoint& writer,
                            const mc::shm::endpoint& receiver, const mc::engine::message& msg);

    static mc::shm::endpoint to_endpoint(const mc::shm::endpoint_info& info);

    std::size_t m_region_size{2U * 1024U * 1024U};
    std::size_t m_root_capacity{64U};

    mc::string                            m_region_name;
    std::unique_ptr<mc::shm::shm_runtime> m_runtime;
#endif // MCENGINE_USE_SHM
};

} // namespace test
} // namespace mc

#endif // MC_TEST_UTILITIES_ENGINE_TEST_BASE_H
