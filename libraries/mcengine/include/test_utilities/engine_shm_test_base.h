/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_TEST_UTILITIES_ENGINE_SHM_TEST_BASE_H
#define MC_TEST_UTILITIES_ENGINE_SHM_TEST_BASE_H

#include <mc/common.h>
#include <mc/engine/message.h>
#include <mc/engine/service_proto.h>
#include <mc/shm/message_queue/mq_channel.h>
#include <mc/shm/message_queue/mq_proto.h>
#include <mc/shm/message_queue/mq_transport_proto.h>
#include <mc/shm/shm_runtime.h>
#include <mc/string.h>
#include <mc/string_view.h>

#include <test_utilities/engine_test_base.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace mc {
namespace test {

// SHM 测试基座。每个 TEST_F 独占一块 SHM region。
// 继承链：TestWithRuntime → TestWithEngine → TestWithEngineShmRegion。
class MC_API TestWithEngineShmRegion : public TestWithEngine {
protected:
    void SetUp() override;
    void TearDown() override;

    // === 单进程 helper ===

    // m_runtime 的 shared_ptr 别名（无 deleter），供 mq_channel::start 等接口使用。
    std::shared_ptr<mc::shm::shm_runtime> runtime_alias() const;

    // register_endpoint + mark_endpoint_running 组合。失败返回 nullopt。
    static std::optional<mc::shm::endpoint> register_running_endpoint(mc::shm::shm_runtime& runtime,
                                                                      mc::string_view       name);

    // === 跨进程 helper ===

    // attach 同一 region 造独立 shm_runtime，给 fork 子进程用。
    std::unique_ptr<mc::shm::shm_runtime> open_child_shm_runtime() const;

    // fork 子进程执行 body；父进程 waitpid 并断言退出码 0。
    // body 返回 int 作退出码；抛异常兜底为 110。
    void fork_child(const std::function<int()>& body) const;

    // RX pipeline：service_proto + mq_proto + mq_transport_proto + mq_channel。
    // 析构时自动 stop channel。
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

    // publisher 侧：经完整协议栈把 msg 投进 receiver 的 SHM 队列。
    static void send_via_mq(mc::shm::shm_runtime& runtime, const mc::shm::endpoint& writer,
                            const mc::shm::endpoint& receiver, const mc::engine::message& msg);

    // endpoint_info → endpoint 转换。
    static mc::shm::endpoint to_endpoint(const mc::shm::endpoint_info& info);

    // 可在派生类 ctor 中覆写。
    std::size_t m_region_size{2U * 1024U * 1024U};
    std::size_t m_root_capacity{64U};

    mc::string                            m_region_name;
    std::unique_ptr<mc::shm::shm_runtime> m_runtime;
};

} // namespace test
} // namespace mc

#endif // MC_TEST_UTILITIES_ENGINE_SHM_TEST_BASE_H
