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

#include <test_utilities/engine_test_base.h>

#if MCENGINE_USE_SHM

#include <mc/engine/engine.h>
#include <mc/protocol.h>
#include <mc/runtime.h>
#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/message_queue/mq_queue.h>

#include "../../../mcbase/src/shm/message_queue/mq_notifier.h"
#include "../../src/shm_runtime_provider.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace mc {
namespace test {

namespace {

// 内部约定的子进程异常退出码：body 抛出 C++ 异常时用这个码，避免和用户自己
// 约定的 1..109 冲突。
constexpr int kChildExceptionExitCode = 110;

mc::string make_unique_region_name()
{
    // pid + steady_clock 纳秒：同一 host 并发跑多个测试进程也不会撞名。
    std::ostringstream oss;
    oss << "mcengine.test." << static_cast<long>(::getpid()) << "."
        << std::chrono::steady_clock::now().time_since_epoch().count();
    return mc::string(oss.str().c_str());
}

} // namespace

void TestWithEngine::SetUp()
{
    // 链式执行 TestWithRuntime → TestBase 的 SetUp。
    TestWithRuntime::SetUp();

    // 进入本 case 前先把上一个 case 残留的 engine 单例（service_registry /
    // match_table / 全局 object_table）彻底清掉。单例可能持有上一个 case 已
    // unmap 的 SHM region 引用，必须在创建新 region 前先 reset。
    mc::engine::engine::reset_for_test();

    m_region_name = make_unique_region_name();
    mc::shm::detail::shared_memory_backend::remove(m_region_name);

    mc::shm::runtime_options opts;
    opts.region_name   = m_region_name;
    opts.region_size   = m_region_size;
    opts.root_capacity = m_root_capacity;
    m_runtime          = std::make_unique<mc::shm::shm_runtime>(opts);
    ASSERT_TRUE(m_runtime->is_valid()) << "mc::shm::shm_runtime 初始化失败";

    mc::engine::shm_runtime_provider::install_runtime(runtime_alias());
}

void TestWithEngine::TearDown()
{
    // 销毁 engine 单例必须先于 m_runtime.reset()：单例 lazy 创建的对象表 /
    // match_table / service_registry 都通过 provider 引用 m_runtime；reset
    // 顺序反了，单例析构会访问已 unmap 的 SHM 段错误。
    mc::engine::engine::reset_for_test();
    mc::engine::shm_runtime_provider::reset_for_test();

    if (m_runtime) {
        for (std::uint16_t endpoint_id = 1; endpoint_id <= mc::shm::shm_runtime::max_endpoints;
             ++endpoint_id) {
            auto info = m_runtime->get_endpoint(endpoint_id);
            if (!info.has_value()) {
                continue;
            }
            mc::shm::detail::mq_notifier::remove(info->notifier_name);
            mc::shm::detail::mq_notifier::remove(
                mc::shm::detail::mq_notifier::make_space_name(info->queue_name));
            mc::shm::detail::shared_memory_backend::remove(info->queue_name);
        }
    }
    m_runtime.reset();
    mc::shm::detail::shared_memory_backend::remove(m_region_name);
    m_region_name = mc::string();

    TestWithRuntime::TearDown();
}

std::shared_ptr<mc::shm::shm_runtime> TestWithEngine::runtime_alias() const
{
    // 空 deleter：所有权留在 fixture 的 m_runtime（unique_ptr）里，这个
    // shared_ptr 只是给 mq_channel::start 这种签名借用观察者用。TearDown
    // 前，所有从 alias 派生的 shared_ptr 必须释放（通常 channel.stop 触发）。
    return std::shared_ptr<mc::shm::shm_runtime>(m_runtime.get(),
                                                 [](mc::shm::shm_runtime*) {});
}

std::optional<mc::shm::endpoint> TestWithEngine::register_running_endpoint(mc::shm::shm_runtime& runtime,
                                                                           mc::string_view       name)
{
    auto ep = runtime.register_endpoint(name);
    if (!ep.has_value()) {
        return std::nullopt;
    }
    if (!runtime.mark_endpoint_running(*ep)) {
        return std::nullopt;
    }
    return ep;
}

std::unique_ptr<mc::shm::shm_runtime> TestWithEngine::open_child_shm_runtime() const
{
    mc::shm::runtime_options opts;
    opts.region_name   = m_region_name;
    opts.region_size   = m_region_size;
    opts.root_capacity = m_root_capacity;
    auto rt            = std::make_unique<mc::shm::shm_runtime>(opts);
    if (!rt->is_valid()) {
        return nullptr;
    }
    return rt;
}

void TestWithEngine::fork_child(const std::function<int()>& body) const
{
    pid_t pid = ::fork();
    ASSERT_NE(pid, -1) << "fork failed: " << std::strerror(errno);

    if (pid == 0) {
        int rc = 0;
        try {
            rc = body();
        } catch (...) {
            rc = kChildExceptionExitCode;
        }
        // _exit 跳过整条析构链，模拟"进程崩溃来不及清理"，同时避免
        // gtest fixture 析构在子进程里二次跑。
        _exit(rc);
    }

    int   status = 0;
    pid_t waited = -1;
    do {
        waited = ::waitpid(pid, &status, 0);
    } while (waited == -1 && errno == EINTR);
    ASSERT_EQ(waited, pid) << "waitpid failed: " << std::strerror(errno);
    ASSERT_TRUE(WIFEXITED(status)) << "child did not exit cleanly, raw status=" << status;
    ASSERT_EQ(WEXITSTATUS(status), 0) << "child reported failure code " << WEXITSTATUS(status);
}

// === mq_rx_pipeline ===

TestWithEngine::mq_rx_pipeline::mq_rx_pipeline()
{
    // 按 engine wire 协议栈自顶向下挂子节点：service_proto → mq_proto →
    // mq_transport_proto。channel 独立配合 proto 做 consume loop。
    proto.add_child(mq);
    mq.add_child(transport);
    channel.set_protocol(&proto);
}

TestWithEngine::mq_rx_pipeline::~mq_rx_pipeline()
{
    // 即便上层测试忘了 stop，这里兜底，避免 consumer thread 回调到已销毁的
    // proto 树造成 use-after-free。
    stop();
}

void TestWithEngine::mq_rx_pipeline::start(std::shared_ptr<mc::shm::shm_runtime> runtime,
                                           const mc::shm::endpoint& ep, std::uint32_t instance_id)
{
    channel.start(std::move(runtime), ep, instance_id);
    m_started = true;
}

void TestWithEngine::mq_rx_pipeline::stop()
{
    if (!m_started) {
        return;
    }
    channel.stop();
    m_started = false;
}

// === send_via_mq / to_endpoint ===

void TestWithEngine::send_via_mq(mc::shm::shm_runtime& runtime, const mc::shm::endpoint& writer,
                                 const mc::shm::endpoint& receiver, const mc::engine::message& msg)
{
    auto queue = runtime.open_queue(receiver);

    mc::engine::service_proto   proto_root;
    mc::shm::mq_proto           mq_layer;
    mc::shm::mq_transport_proto transport_layer;
    proto_root.add_child(mq_layer);
    mq_layer.add_child(transport_layer);
    mq_layer.configure(writer.instance_id, writer.endpoint_id);
    transport_layer.configure(queue, writer.endpoint_id, writer.instance_id);

    mc::proto::proto_request req;
    auto&                    ctx = req.ensure_context<mc::engine::service_proto::message_context>(&proto_root);
    ctx.msg                      = msg;
    // service_proto::on_push 不再统一 encode，由调用方负责构造 wire bytes
    auto payload = mc::engine::encode_message_bytes(msg);
    req.buffer().append_payload(payload.data(), payload.size());
    ASSERT_EQ(proto_root.push(req), mc::proto::execution_state::completed);
}

mc::shm::endpoint TestWithEngine::to_endpoint(const mc::shm::endpoint_info& info)
{
    mc::shm::endpoint ep;
    ep.endpoint_id   = info.endpoint_id;
    ep.instance_id   = info.instance_id;
    ep.state         = info.state;
    ep.slot_count    = info.slot_count;
    ep.endpoint_name = info.endpoint_name;
    ep.queue_name    = info.queue_name;
    ep.notifier_name = info.notifier_name;
    return ep;
}

} // namespace test
} // namespace mc

#endif // MCENGINE_USE_SHM
