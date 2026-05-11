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

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <mc/future.h>
#include <mc/runtime/thread_list.h>
#include <mc/singleton.h>
#include <new>
#include <type_traits>
#include <utility>

namespace {

struct scoped_payload_alloc_fail_after {
    explicit scoped_payload_alloc_fail_after(int fail_after) : m_fail_after(fail_after)
    {
        s_active     = true;
        s_fail_after = fail_after;
        s_call_count = 0;
    }

    ~scoped_payload_alloc_fail_after()
    {
        s_active = false;
    }

    int m_fail_after;

    inline static thread_local bool s_active     = false;
    inline static thread_local int  s_fail_after = -1;
    inline static thread_local int  s_call_count = 0;
};

struct oom_probe_payload {
    std::array<std::uint64_t, 40> data{};
};

static_assert(sizeof(std::optional<oom_probe_payload>) > mc::futures::state_base::s_inline_payload_bytes,
              "oom_probe_payload 必须走 external payload 路径");

} // namespace

namespace mc::futures::detail {

template <>
struct state_payload_allocator<std::optional<oom_probe_payload>> {
    using result_type = std::optional<oom_probe_payload>;

    static result_type* allocate()
    {
        if (scoped_payload_alloc_fail_after::s_active &&
            scoped_payload_alloc_fail_after::s_call_count++ >= scoped_payload_alloc_fail_after::s_fail_after) {
            throw std::bad_alloc();
        }
        return new result_type();
    }

    static void deallocate(result_type* ptr) noexcept
    {
        delete ptr;
    }
};

} // namespace mc::futures::detail

// clear_all_pools 仅测试使用（产品约定）。策略：
// - SetUpTestSuite：首次 instance + clear，得到干净起点；
// - 各用例 SetUp 不再 clear，避免与「上一用例 TearDown 尚未执行」的时序假设纠缠；
// - TearDown：仅主线程调用 clear，且要求用例内起的线程已全部 join（并发用例满足后再返回）。
class state_pool_test : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        mc::futures::state_pool::instance().clear_all_pools();
    }

    void TearDown() override
    {
        mc::futures::state_pool::instance().clear_all_pools();
    }

    mc::runtime::thread_pool io_context_{1, "state_pool_test"};
};

// 基本功能测试
TEST_F(state_pool_test, basic_pool_functionality)
{
    auto& pool = mc::futures::state_pool::instance();

    // 获取初始统计（统一控制块池在单例构造时已存在）
    auto initial_stats = pool.get_stats();
    EXPECT_EQ(initial_stats.total_global_states, 0);
    EXPECT_EQ(initial_stats.total_pools, 1);

    // 创建一些 promise/future 对
    std::vector<mc::promise<int>> promises;
    std::vector<mc::future<int>>  futures;

    for (int i = 0; i < 5; ++i) {
        auto promise = mc::make_promise<int>(io_context_);
        auto future  = promise.get_future();

        promises.push_back(std::move(promise));
        futures.push_back(std::move(future));
    }

    // 设置值并完成 future
    for (int i = 0; i < 5; ++i) {
        promises[i].set_value(i * 10);
        EXPECT_EQ(futures[i].get(), i * 10);
    }

    // 清理引用，让 State 回到池中
    promises.clear();
    futures.clear();

    // 检查池统计
    auto final_stats = pool.get_stats();
    EXPECT_EQ(final_stats.total_global_states, 5) << "应该有状态被缓存";
    EXPECT_GT(final_stats.total_pools, 0) << "应该创建了缓存池";
}

// 池重用测试
TEST_F(state_pool_test, pool_reuse)
{
    auto& pool = mc::futures::state_pool::instance();

    // 创建并完成一个 future
    {
        auto promise = mc::make_promise<mc::string>(io_context_);
        auto future  = promise.get_future();
        promise.set_value("test");
        EXPECT_EQ(future.get(), "test");
    }

    // 再次创建相同类型的 future，应该重用池中的 State
    {
        auto promise = mc::make_promise<mc::string>(io_context_);
        auto future  = promise.get_future();
        promise.set_value("reused");
        EXPECT_EQ(future.get(), "reused");
    }

    // 验证池中有缓存的 State
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.total_pools, 1) << "应该只有一个类型的池";
    EXPECT_EQ(stats.total_global_states, 1) << "应该有状态被缓存";
}

// 验证 State 从池中复用时，旧 payload 的销毁逻辑必须仍然生效（否则会泄漏）
//
// 复现路径：
//   1. 第一次使用 State<T>：storage_ops 绑定到当前 ResultType，对应 payload 已正常构造
//   2. 释放后 state_base::reset() 清空状态，State 进入统一控制块池
//   3. 第二次从池中复用：若未完整重建 State<T>，旧 payload 的销毁/构造路径可能丢失
//   4. 第二次释放：若 destroy_slot 未按当前类型执行，旧值会残留在 payload 中并造成泄漏
//
// 用 std::shared_ptr<int> 作为值类型，通过 weak_ptr::expired() 检测析构是否发生
TEST_F(state_pool_test, pool_reuse_no_value_leak)
{
    std::weak_ptr<int> weak1;
    std::weak_ptr<int> weak2;

    // 第一次使用：基线验证，值应被正确析构
    {
        auto promise = mc::make_promise<std::shared_ptr<int>>(io_context_);
        auto future  = promise.get_future();
        auto val     = std::make_shared<int>(42);
        weak1        = val;
        promise.set_value(std::move(val));
        EXPECT_EQ(*future.get(), 42);
        // 离开作用域：future/promise 析构 → state ref count 归零 → destroy_slot 路径清理 payload
    }
    // 第一次使用后值应该已经析构（基线验证，应该通过）
    EXPECT_TRUE(weak1.expired()) << "第一次使用后值应被正确析构（基线）";

    // 第二次使用：从池中复用 State，验证 payload 析构/重建逻辑仍然正确
    {
        auto promise = mc::make_promise<std::shared_ptr<int>>(io_context_);
        auto future  = promise.get_future();
        auto val     = std::make_shared<int>(100);
        weak2        = val;
        promise.set_value(std::move(val));
        EXPECT_EQ(*future.get(), 100);
        // 离开作用域：future/promise 析构 → state ref count 归零 → payload 应被完整清理
    }

    // 第二次使用后，值也应该被析构
    // 若池复用路径丢失 payload 销毁逻辑，这里 weak2 将不会过期
    EXPECT_TRUE(weak2.expired()) << "第二次（池复用）使用后值应被正确析构；"
                                    "若此断言失败，说明池复用后的 payload 销毁路径存在泄漏";
}

// 多次复用时值不泄漏：验证第3次、第4次复用同样正确
TEST_F(state_pool_test, pool_reuse_multiple_times_no_leak)
{
    constexpr int                   rounds = 5;
    std::vector<std::weak_ptr<int>> weaks(rounds);

    for (int i = 0; i < rounds; ++i) {
        auto promise = mc::make_promise<std::shared_ptr<int>>(io_context_);
        auto future  = promise.get_future();
        auto val     = std::make_shared<int>(i * 10);
        weaks[i]     = val;
        promise.set_value(std::move(val));
        EXPECT_EQ(*future.get(), i * 10);
        // 离开作用域后 state 回池
    }

    // 每一轮的值都应该已经被析构
    for (int i = 0; i < rounds; ++i) {
        EXPECT_TRUE(weaks[i].expired()) << "第 " << i + 1 << " 次复用后值应被正确析构";
    }
}

// 非平凡类型（mc::string）复用时不泄漏：string 在堆上有动态分配，valgrind 可检测
TEST_F(state_pool_test, pool_reuse_string_no_leak)
{
    // mc::string 是非平凡类型，payload 销毁路径必须正确恢复才能释放其堆内存
    for (int i = 0; i < 3; ++i) {
        auto promise = mc::make_promise<mc::string>(io_context_);
        auto future  = promise.get_future();
        // 超过 SSO 阈值，确保堆上有动态分配
        promise.set_value(mc::string(64, 'a' + i));
        EXPECT_EQ(future.get().size(), 64u);
    }
    // valgrind 应无 "definitely lost" 报告
}

// void 类型复用不受影响（monostate 是平凡类型，没有额外 payload 资源需要释放）
// 此用例验证修复不破坏 void future 的正常行为
TEST_F(state_pool_test, pool_reuse_void_no_regression)
{
    for (int i = 0; i < 3; ++i) {
        auto promise = mc::make_promise<void>(io_context_);
        auto future  = promise.get_future();
        promise.set_value();
        future.get(); // 不应抛异常
    }
}

// 平凡类型（int）复用不受影响（payload 无副作用析构，复用后也不应残留旧值）
// 此用例验证修复不破坏平凡类型 future 的正常行为
TEST_F(state_pool_test, pool_reuse_trivial_type_no_regression)
{
    for (int i = 0; i < 3; ++i) {
        auto promise = mc::make_promise<int>(io_context_);
        auto future  = promise.get_future();
        promise.set_value(i * 100);
        EXPECT_EQ(future.get(), i * 100);
    }
}

// 复用后 future 设置异常时值也不泄漏
TEST_F(state_pool_test, pool_reuse_exception_no_leak)
{
    // 第一次：正常完成，State 进池
    {
        auto promise = mc::make_promise<std::shared_ptr<int>>(io_context_);
        auto future  = promise.get_future();
        promise.set_value(std::make_shared<int>(1));
        future.get();
    }

    // 第二次（复用）：通过异常路径结束
    std::weak_ptr<int> weak;
    {
        auto promise = mc::make_promise<std::shared_ptr<int>>(io_context_);
        auto future  = promise.get_future();
        auto val     = std::make_shared<int>(2);
        weak         = val;
        // 以异常而非正常值结束，m_result 不会被 set_value 填入
        // 但要验证 State 正常析构时不会 crash
        promise.set_exception(std::runtime_error("test"));
        EXPECT_THROW(future.get(), std::runtime_error);
    }
    // 异常路径不会将值写入 m_result，val 的引用计数由 weak 和本地 val 控制
    // val 在上面的块中已经超出作用域，应该已经析构
    EXPECT_TRUE(weak.expired()) << "异常路径下 val 应在作用域结束时析构";
}

// 约束：外围代码应通过 State<T> 同名 facade 访问 typed value，而不是直接依赖 detail::state_payload_access
TEST_F(state_pool_test, state_typed_facade_keeps_legacy_names)
{
    auto  promise = mc::make_promise<int>(io_context_);
    auto  future  = promise.get_future();
    auto& state   = *future.get_state();

    mc::futures::State<int>::set_value(state, 123);
    state.set_ready();

    EXPECT_EQ(mc::futures::State<int>::get_value(state), 123);
}

TEST_F(state_pool_test, pool_reuse_preserves_dynamic_state_type)
{
    {
        auto promise = mc::make_promise<int>(io_context_);
        auto future  = promise.get_future();
        EXPECT_NE(dynamic_cast<mc::futures::State<int>*>(future.get_state().get()), nullptr);
        promise.set_value(1);
        EXPECT_EQ(future.get(), 1);
    }

    {
        auto promise = mc::make_promise<int>(io_context_);
        auto future  = promise.get_future();
        EXPECT_NE(dynamic_cast<mc::futures::State<int>*>(future.get_state().get()), nullptr);
        promise.set_value(2);
        EXPECT_EQ(future.get(), 2);
    }
}

// 不同大小类型测试
TEST_F(state_pool_test, different_size_types)
{
    auto& pool = mc::futures::state_pool::instance();

    // 创建不同大小的类型的 future
    {
        auto char_promise = mc::make_promise<char>(io_context_); // 1 字节
        auto char_future  = char_promise.get_future();
        char_promise.set_value('A');
        EXPECT_EQ(char_future.get(), 'A');
    }

    {
        auto int64_promise = mc::make_promise<std::int32_t>(io_context_); // 4 字节
        auto int64_future  = int64_promise.get_future();
        int64_promise.set_value(123456789L);
        EXPECT_EQ(int64_future.get(), 123456789L);
    }

    auto stats = pool.get_stats();
    EXPECT_EQ(stats.total_pools, 1) << "统一 state_base 控制块池下，不同 value size 应共享同一个池";
}

// 相同大小类型共享池测试
TEST_F(state_pool_test, same_size_types_share_pool)
{
    auto& pool = mc::futures::state_pool::instance();

    // 创建并销毁 int future
    {
        auto int_promise = mc::make_promise<int64_t>(io_context_);
        auto int_future  = int_promise.get_future();
        int_promise.set_value(42);
        EXPECT_EQ(int_future.get(), 42);
    }

    auto stats_after_int = pool.get_stats();
    EXPECT_GT(stats_after_int.total_global_states, 0) << "int future 应该创建状态";

    // 创建 double future，应该重用 int64_t 的缓存
    {
        auto double_promise = mc::make_promise<double>(io_context_);
        auto double_future  = double_promise.get_future();
        double_promise.set_value(3.14);
        EXPECT_FLOAT_EQ(double_future.get(), 3.14);
    }

    auto stats_after_float = pool.get_stats();
    EXPECT_EQ(stats_after_float.total_pools, 1) << "统一控制块池下，int64_t 和 double 应共享同一个池";
    EXPECT_EQ(stats_after_float.total_global_states, 1) << "统一控制块池下，两次释放后应回收到同一个缓存槽位";
}

TEST_F(state_pool_test, different_value_sizes_within_same_size_class_share_pool_under_pool_limit)
{
    auto& pool = mc::futures::state_pool::instance();

    struct state_size_17 {
        char data[16];
    };
    struct state_size_32 {
        char data[31];
    };

    auto promise1 = mc::make_promise<state_size_17>(io_context_);
    auto future1  = promise1.get_future();
    auto promise2 = mc::make_promise<state_size_32>(io_context_);
    auto future2  = promise2.get_future();

    {
        promise1.set_value(state_size_17{});
        future1.get();
        promise2.set_value(state_size_32{});
        future2.get();
    }

    promise1 = {};
    future1  = {};
    promise2 = {};
    future2  = {};

    auto stats = pool.get_stats();
    EXPECT_EQ(stats.total_pools, 1U) << "统一 state_base 控制块池下，不同 value size 应共享一个底层池";
    EXPECT_EQ(stats.total_global_states, 2U) << "两个 state 都应进入统一控制块池";
}

// 清理测试
TEST_F(state_pool_test, pool_clear)
{
    auto& pool = mc::futures::state_pool::instance();

    // 创建一些 future 来填充池
    {
        auto promise = mc::make_promise<int>(io_context_);
        auto future  = promise.get_future();
        promise.set_value(123);
        EXPECT_EQ(future.get(), 123);
    }

    // 清理池：销毁缓存并替换底层 fixed_size_pool，统计归零
    pool.clear_all_pools();

    auto stats = pool.get_stats();
    EXPECT_EQ(stats.total_global_states, 0);
    EXPECT_EQ(stats.total_pools, 1);
}

// 大 payload 走 external fallback，但控制块仍应进入统一池
TEST_F(state_pool_test, large_payload_still_reuses_pooled_state_base)
{
    auto& pool = mc::futures::state_pool::instance();

    struct large_state {
        std::array<std::uint64_t, 40> data{};
    };
    {
        auto        promise = mc::make_promise<large_state>(io_context_);
        auto        future  = promise.get_future();
        large_state ls{};
        promise.set_value(ls);
        future.get();
    }

    auto stats_after = pool.get_stats();
    EXPECT_EQ(stats_after.total_pools, 1U) << "大 payload 也应复用统一 state_base 控制块池";
    EXPECT_EQ(stats_after.total_global_states, 1U) << "大 payload 的控制块释放后应回到统一池";
}

// 大 payload 且含非平凡成员时，external 路径在多轮池复用后也必须正确析构堆对象
TEST_F(state_pool_test, large_nontrivial_external_payload_reuse_no_leak)
{
    struct large_tracked_state {
        std::array<std::uint64_t, 40> data{};
        std::shared_ptr<int>          owner;
    };

    constexpr int                   rounds = 4;
    std::vector<std::weak_ptr<int>> weaks(rounds);

    for (int i = 0; i < rounds; ++i) {
        auto promise = mc::make_promise<large_tracked_state>(io_context_);
        auto future  = promise.get_future();

        large_tracked_state value{};
        value.data[0] = static_cast<std::uint64_t>(i + 1);
        auto sp       = std::make_shared<int>(i + 1);
        weaks[i]      = sp;
        value.owner   = std::move(sp);

        promise.set_value(std::move(value));
        EXPECT_EQ(*future.get().owner, i + 1);
    }

    for (int i = 0; i < rounds; ++i) {
        EXPECT_TRUE(weaks[i].expired()) << "第 " << i + 1
                                        << " 轮 external payload 复用后，shared_ptr 持有的堆对象应已析构";
    }

    auto stats = mc::futures::state_pool::instance().get_stats();
    EXPECT_EQ(stats.total_global_states, 1U) << "同一大 payload 类型多轮复用后，应仍只回收到一个控制块槽位";
}

TEST_F(state_pool_test, external_payload_oom_does_not_terminate_or_lose_pool_slot)
{
    auto& pool = mc::futures::state_pool::instance();

    {
        auto promise = mc::make_promise<oom_probe_payload>(io_context_);
        auto future  = promise.get_future();
        promise.set_value(oom_probe_payload{});
        future.get();
    }

    const auto stats_before = pool.get_stats();
    EXPECT_EQ(stats_before.total_global_states, 1U) << "基线：应已有一个可复用的控制块槽位";

    {
        scoped_payload_alloc_fail_after fail_guard(0);
        EXPECT_THROW((void)mc::make_promise<oom_probe_payload>(io_context_), mc::bad_alloc_exception);
    }

    const auto stats_after_failure = pool.get_stats();
    EXPECT_EQ(stats_after_failure.total_global_states, 1U) << "构造 external payload 失败后，不应丢失池中的控制块槽位";

    {
        auto promise = mc::make_promise<oom_probe_payload>(io_context_);
        auto future  = promise.get_future();
        promise.set_value(oom_probe_payload{});
        future.get();
    }

    const auto stats_after_retry = pool.get_stats();
    EXPECT_EQ(stats_after_retry.total_global_states, 1U) << "OOM 后再次分配应仍可正常复用同一个控制块槽位";
}

// 泄漏验证辅助：先触发 external payload OOM fallback，再复用同一槽位并显式 clear_all_pools。
// 若 fallback state_base 没有在复用前先析构，leaks 会在进程退出时报告残留的控制块子对象。
TEST_F(state_pool_test, external_payload_oom_reuse_then_clear_pool_for_leak_check)
{
    auto& pool = mc::futures::state_pool::instance();

    {
        auto promise = mc::make_promise<oom_probe_payload>(io_context_);
        auto future  = promise.get_future();
        promise.set_value(oom_probe_payload{});
        future.get();
    }

    {
        scoped_payload_alloc_fail_after fail_guard(0);
        EXPECT_THROW((void)mc::make_promise<oom_probe_payload>(io_context_), mc::bad_alloc_exception);
    }

    {
        auto promise = mc::make_promise<oom_probe_payload>(io_context_);
        auto future  = promise.get_future();
        promise.set_value(oom_probe_payload{});
        future.get();
    }

    // 显式销毁缓存对象，便于把“复用前是否先析构 fallback 对象”的问题单独暴露给 leaks。
    pool.clear_all_pools();
    const auto stats = pool.get_stats();
    EXPECT_EQ(stats.total_global_states, 0U);
}

// 析构路径验证辅助：不主动 clear_all_pools，而是直接 reset state_pool 单例。
// 若 state_pool 析构前未先析构缓存 state，对应控制块内部资源会在进程退出时被 leaks 报出。
TEST(state_pool_shutdown_test, reset_singleton_with_cached_state_for_leak_check)
{
    mc::runtime::thread_pool io_context{1, "state_pool_shutdown_test"};

    auto& pool = mc::futures::state_pool::instance();
    pool.clear_all_pools();

    {
        auto promise = mc::make_promise<mc::string>(io_context);
        auto future  = promise.get_future();
        promise.set_value(mc::string(64, 'x'));
        EXPECT_EQ(future.get().size(), 64U);
    }

    const auto stats_before_reset = pool.get_stats();
    EXPECT_EQ(stats_before_reset.total_global_states, 1U);

    mc::singleton<mc::futures::state_pool>::reset_for_test();
}

// 运行期：含用户析构函数的类型，验证池复用时析构依然被调用
//
// tracked_struct 含 shared_ptr 成员和用户析构函数：
//   - is_trivially_destructible = false → destroy_slot 必须执行 payload 析构
//   - 池复用时必须完整重建 State<T>，否则旧 payload 可能残留
TEST_F(state_pool_test, get_destory_user_dtor_type_dtor_called_on_pool_reuse)
{
    struct tracked_struct {
        std::shared_ptr<int> owner; // 通过 weak_ptr 间接观察析构是否发生
        ~tracked_struct()
        {} // 用户析构函数 → 非平凡析构
    };
    std::weak_ptr<int> weak1;
    std::weak_ptr<int> weak2;

    // 第一轮（新分配）：基线验证
    {
        auto promise = mc::make_promise<tracked_struct>(io_context_);
        auto future  = promise.get_future();
        auto sp      = std::make_shared<int>(1);
        weak1        = sp;
        promise.set_value(tracked_struct{std::move(sp)});
        EXPECT_EQ(*future.get().owner, 1);
    }
    EXPECT_TRUE(weak1.expired()) << "首次使用后析构应被调用（基线）";

    // 第二轮（池复用）：关键验证——若池复用未完整重建类型对象，析构不会被调用
    {
        auto promise = mc::make_promise<tracked_struct>(io_context_);
        auto future  = promise.get_future();
        auto sp      = std::make_shared<int>(2);
        weak2        = sp;
        promise.set_value(tracked_struct{std::move(sp)});
        EXPECT_EQ(*future.get().owner, 2);
    }
    EXPECT_TRUE(weak2.expired()) << "池复用后，含用户析构函数的类型析构仍应被调用；"
                                    "若此断言失败，说明池复用重建路径丢失析构逻辑，destory_impl 被跳过";
}

// 运行期：vector<shared_ptr> 多次复用，确保每轮所有堆对象均被析构
// vector 是非平凡析构，destroy_slot 必须真正销毁 payload
TEST_F(state_pool_test, get_destory_vector_heap_objects_freed_on_reuse)
{
    constexpr int                   rounds    = 4;
    constexpr int                   per_round = 3;
    std::vector<std::weak_ptr<int>> weaks;

    for (int i = 0; i < rounds; ++i) {
        auto promise = mc::make_promise<std::vector<std::shared_ptr<int>>>(io_context_);
        auto future  = promise.get_future();

        std::vector<std::shared_ptr<int>> elems;
        for (int j = 0; j < per_round; ++j) {
            auto sp = std::make_shared<int>(i * 10 + j);
            weaks.push_back(sp);
            elems.push_back(std::move(sp));
        }
        promise.set_value(std::move(elems));
        ASSERT_EQ(static_cast<int>(future.get().size()), per_round);
    }

    for (int i = 0; i < rounds * per_round; ++i) {
        EXPECT_TRUE(weaks[i].expired()) << "第 " << i / per_round + 1 << " 轮第 " << i % per_round + 1
                                        << " 个元素应在 payload 销毁路径执行后析构";
    }
}

// 运行期：平凡析构类型（int）—— 不需要额外析构逻辑；
// 但因池复用会完整重建 State<T> 对象，旧值不会泄露给新一轮。
// 验证新一轮始终能获取到正确的新值，而非上一轮残留的旧值。
TEST_F(state_pool_test, get_destory_trivial_type_no_stale_value_after_reuse)
{
    for (int i = 0; i < 5; ++i) {
        auto promise = mc::make_promise<int>(io_context_);
        auto future  = promise.get_future();
        promise.set_value(i * 111);
        EXPECT_EQ(future.get(), i * 111) << "第 " << i + 1 << " 轮：复用 State 后应读到本轮写入的值 " << i * 111
                                         << "，而不是上一轮的残留值";
    }
}

// 多线程并发测试（子线程全部 join 后再析构用例对象，避免与 TearDown 中 clear_all_pools 交叉）
TEST_F(state_pool_test, concurrent_access)
{
    auto& pool = mc::futures::state_pool::instance();

    const int num_threads = 4;
    const int iterations  = 1000;

    mc::runtime::thread_list threads;
    std::atomic<int>         completed{0};

    // 多个线程并发访问池
    threads.start_threads(num_threads, [&completed]() {
        mc::runtime::thread_pool local_io(1, "state_pool_local");

        for (int i = 0; i < iterations; ++i) {
            auto promise = mc::make_promise<int>(local_io);
            auto future  = promise.get_future();
            promise.set_value(i);
            future.get();
        }

        completed++;
    });

    threads.join_all();

    EXPECT_EQ(completed.load(), num_threads);

    // 验证池状态
    auto stats = pool.get_stats();
    EXPECT_GT(stats.total_global_states, 0) << "应该有状态被缓存";
    EXPECT_GT(stats.total_pools, 0) << "应该创建了缓存池";
}

TEST_F(state_pool_test, mixed_size_types_under_contention)
{
    // 同上：threads.join_all() 完成后才进入 TearDown
    mc::runtime::thread_list threads;
    std::atomic<int>         completed{0};

    threads.start_threads(4, [&completed]() {
        mc::runtime::thread_pool local_io(1, "state_pool_mix");
        for (int i = 0; i < 500; ++i) {
            auto promise1 = mc::make_promise<int>(local_io);
            auto future1  = promise1.get_future();
            auto promise2 = mc::make_promise<mc::string>(local_io);
            auto future2  = promise2.get_future();

            promise1.set_value(i);
            promise2.set_value("payload");

            EXPECT_EQ(future1.get(), i);
            EXPECT_EQ(future2.get(), "payload");
        }
        ++completed;
    });

    threads.join_all();
    EXPECT_EQ(completed.load(), 4);
}
