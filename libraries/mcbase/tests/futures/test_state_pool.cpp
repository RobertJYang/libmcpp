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

#include <mc/future.h>
#include <mc/runtime/thread_list.h>

class state_pool_test : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 重置池配置
        mc::futures::state_pool_config config;
        config.max_count_per_pool = 10;
        config.max_pool_count     = 5;
        config.max_cacheable_size = 256;

        mc::futures::state_pool::instance().set_config(config);
    }

    void TearDown() override
    {
        // 清理池
        mc::futures::state_pool::instance().clear_all_pools();
    }

    mc::runtime::thread_pool io_context_{1, "state_pool_test"};
};

// 基本功能测试
TEST_F(state_pool_test, basic_pool_functionality)
{
    auto& pool = mc::futures::state_pool::instance();

    // 获取初始统计
    auto initial_stats = pool.get_stats();
    EXPECT_EQ(initial_stats.total_global_states, 0);
    EXPECT_EQ(initial_stats.total_pools, 0);

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

// 验证 State 从池中复用时，旧值的析构函数必须被调用（否则产生内存泄漏）
//
// 复现路径：
//   1. 第一次使用 State<T>：构造时 m_destory = get_destory_()（非平凡类型不为 nullptr）
//   2. 释放后 state_base::reset() 将 m_destory 置为 nullptr，State 进池
//   3. 第二次从池中复用：State<T>::reuse() 未恢复 m_destory
//   4. 第二次释放：destory() 因 m_destory==nullptr 跳过 destory_impl，
//      m_result 中的值（如 DBusMessage* 引用计数）永远不会被析构 → 内存泄漏
//
// 用 std::shared_ptr<int> 作为值类型，通过 weak_ptr::expired() 检测析构是否发生
TEST_F(state_pool_test, pool_reuse_no_value_leak)
{
    std::weak_ptr<int> weak1;
    std::weak_ptr<int> weak2;

    // 第一次使用：State 第一次创建，m_destory 正常，值应该被正确析构
    {
        auto promise = mc::make_promise<std::shared_ptr<int>>(io_context_);
        auto future  = promise.get_future();
        auto val     = std::make_shared<int>(42);
        weak1        = val;
        promise.set_value(std::move(val));
        EXPECT_EQ(*future.get(), 42);
        // 离开作用域：future/promise 析构 → state ref count 归零 → destroy() 被调用
        // m_destory 非 nullptr → destory_impl 被调用 → m_result 被清空 → val 引用计数归零
    }
    // 第一次使用后值应该已经析构（基线验证，应该通过）
    EXPECT_TRUE(weak1.expired()) << "第一次使用后值应被正确析构（基线）";

    // 第二次使用：从池中复用 State，此时存在 m_destory == nullptr 的 bug
    {
        auto promise = mc::make_promise<std::shared_ptr<int>>(io_context_);
        auto future  = promise.get_future();
        auto val     = std::make_shared<int>(100);
        weak2        = val;
        promise.set_value(std::move(val));
        EXPECT_EQ(*future.get(), 100);
        // 离开作用域：future/promise 析构 → state ref count 归零 → destroy() 被调用
        // BUG：m_destory == nullptr → destory_impl 未调用 → m_result 未被清空
        // val 的引用计数保持为 1（m_result 持有），shared_ptr 不会析构 → 内存泄漏
    }

    // 第二次使用后，值也应该被析构
    // BUG 存在时：weak2 未过期（m_result 仍然持有值），valgrind 会报告堆内存泄漏
    // BUG 修复后：weak2 过期（m_result 已被清空），valgrind 无泄漏
    EXPECT_TRUE(weak2.expired()) << "第二次（池复用）使用后值应被正确析构；"
                                    "若此断言失败，valgrind 将报告 State<T>::reuse() 未恢复 m_destory 导致的内存泄漏";
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
    // mc::string 是非平凡类型，m_destory 必须被正确恢复才能释放其堆内存
    for (int i = 0; i < 3; ++i) {
        auto promise = mc::make_promise<mc::string>(io_context_);
        auto future  = promise.get_future();
        // 超过 SSO 阈值，确保堆上有动态分配
        promise.set_value(mc::string(64, 'a' + i));
        EXPECT_EQ(future.get().size(), 64u);
    }
    // valgrind 应无 "definitely lost" 报告
}

// void 类型复用不受 bug 影响（monostate 是平凡类型，m_destory 本来就是 nullptr）
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

// 平凡类型（int）复用不受 bug 影响（get_destory_() 本就为 nullptr，修复是幂等的）
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

// 池大小限制测试
TEST_F(state_pool_test, pool_size_limit)
{
    auto& pool = mc::futures::state_pool::instance();

    // 设置较小的池大小
    mc::futures::state_pool_config config;
    config.max_count_per_pool = 2;
    pool.set_config(config);

    // 创建超过池大小的 future
    std::vector<mc::promise<int>> promises;
    std::vector<mc::future<int>>  futures;

    for (int i = 0; i < 5; ++i) {
        auto promise = mc::make_promise<int>(io_context_);
        auto future  = promise.get_future();

        promises.push_back(std::move(promise));
        futures.push_back(std::move(future));
    }

    // 完成所有 future
    for (int i = 0; i < 5; ++i) {
        promises[i].set_value(i);
        EXPECT_EQ(futures[i].get(), i);
    }

    // 清理引用
    promises.clear();
    futures.clear();

    // 检查池大小不超过限制
    auto stats = pool.get_stats();
    EXPECT_LE(stats.total_global_states, 2) << "缓存的状态数不应超过池大小限制";
}

// 不同大小类型测试
TEST_F(state_pool_test, different_size_types)
{
    auto& pool = mc::futures::state_pool::instance();

    // 清理初始状态
    pool.clear_all_pools();

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
    EXPECT_EQ(stats.total_pools, 1) << "缓存池按8字节对齐，应该只有一个";
}

// 相同大小类型共享池测试
TEST_F(state_pool_test, same_size_types_share_pool)
{
    auto& pool = mc::futures::state_pool::instance();

    auto config = pool.get_config();
    pool.set_config(config);

    // 清理初始状态
    pool.clear_all_pools();

    static_assert(sizeof(int64_t) == sizeof(double), "int64_t 和 double 必须同样大小才能测试");

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
    EXPECT_EQ(stats_after_float.total_pools, 1) << "int64_t 和 double 应该共享同一个池";
    EXPECT_EQ(stats_after_float.total_global_states, 1) << "int64_t 和 double 应该共享同一个状态";
}

// 配置测试
TEST_F(state_pool_test, pool_config)
{
    auto& pool = mc::futures::state_pool::instance();

    mc::futures::state_pool_config config;
    config.max_count_per_pool = 500;
    config.max_pool_count     = 20;
    config.max_cacheable_size = 1024;

    pool.set_config(config);

    const auto& retrieved_config = pool.get_config();
    EXPECT_EQ(retrieved_config.max_count_per_pool, 500);
    EXPECT_EQ(retrieved_config.max_pool_count, 20);
    EXPECT_EQ(retrieved_config.max_cacheable_size, 1024);
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

    // 清理池
    pool.clear_all_pools();

    // 检查池被清空
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.total_global_states, 0);
    EXPECT_EQ(stats.total_pools, 0);
}

// 大size状态不缓存测试
TEST_F(state_pool_test, large_state_not_cached)
{
    auto& pool = mc::futures::state_pool::instance();

    // 设置较小的max_cacheable_size
    mc::futures::state_pool_config config;
    config.max_cacheable_size = 32;
    pool.set_config(config);

    // 创建一个大的结构体
    struct large_state {
        char data[64]; // 大于max_cacheable_size
    };

    // 创建并完成一个large_state future
    {
        auto        promise = mc::make_promise<large_state>(io_context_);
        auto        future  = promise.get_future();
        large_state ls{};
        promise.set_value(ls);
        future.get();
    }

    // 检查池统计，不应该有缓存的状态
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.total_global_states, 0) << "大size的状态不应该被缓存";
    EXPECT_EQ(stats.total_pools, 0) << "不应该为大size状态创建池";
}

// 池数量限制测试
TEST_F(state_pool_test, pool_count_limit)
{
    auto& pool = mc::futures::state_pool::instance();

    // 设置较小的max_pool_count
    mc::futures::state_pool_config config;
    config.max_pool_count = 2;
    pool.set_config(config);

    // 创建不同大小的状态来触发新池创建
    struct state_size_1 {
        char data[16];
    };
    struct state_size_2 {
        char data[32];
    };
    struct state_size_3 {
        char data[48];
    };

    // 创建并完成future
    {
        auto promise1 = mc::make_promise<state_size_1>(io_context_);
        auto future1  = promise1.get_future();
        promise1.set_value(state_size_1{});
        future1.get();
    }

    {
        auto promise2 = mc::make_promise<state_size_2>(io_context_);
        auto future2  = promise2.get_future();
        promise2.set_value(state_size_2{});
        future2.get();
    }

    {
        auto promise3 = mc::make_promise<state_size_3>(io_context_);
        auto future3  = promise3.get_future();
        promise3.set_value(state_size_3{});
        future3.get();
    }

    // 检查池数量不超过限制
    auto stats = pool.get_stats();
    EXPECT_LE(stats.total_pools, 2) << "池数量不应超过限制";
}

// =============================================================================
// 验证 get_destory_() 的类型分类行为
// =============================================================================

namespace {

// ---- 编译期断言：哪些类型属于平凡析构（get_destory_() = nullptr） ----
//
// std::optional<T> 是平凡析构 当且仅当 T 是平凡析构；
// get_destory_() 以 is_trivially_destructible_v<optional<T>> 作为判据。

// 平凡析构类型：不会调用 destory_impl，也不需要调用（无副作用析构）
static_assert(std::is_trivially_destructible_v<std::optional<int>>,
              "State<int>: get_destory_() 返回 nullptr，destory_impl 不被调用");
static_assert(std::is_trivially_destructible_v<std::optional<double>>, "State<double>: get_destory_() 返回 nullptr");
static_assert(std::is_trivially_destructible_v<std::optional<std::monostate>>,
              "State<void>: monostate 是平凡析构，get_destory_() 返回 nullptr");

struct trivial_pod {
    int   x;
    float y;
};
static_assert(std::is_trivially_destructible_v<std::optional<trivial_pod>>,
              "纯 POD 结构体：平凡析构，get_destory_() 返回 nullptr");

// 非平凡析构类型：必须调用 destory_impl 才能释放资源
static_assert(!std::is_trivially_destructible_v<std::optional<std::shared_ptr<int>>>,
              "State<shared_ptr>: get_destory_() 必须返回 &destory_impl");
static_assert(!std::is_trivially_destructible_v<std::optional<mc::string>>,
              "State<string>: get_destory_() 必须返回 &destory_impl");
static_assert(!std::is_trivially_destructible_v<std::optional<std::vector<int>>>,
              "State<vector>: get_destory_() 必须返回 &destory_impl");

// 含用户提供析构函数的类型：即使析构体为空，也是非平凡析构
struct user_dtor_empty {
    int x;
    ~user_dtor_empty()
    {} // 空析构函数 → 非平凡析构
};
static_assert(!std::is_trivially_destructible_v<std::optional<user_dtor_empty>>,
              "含用户析构函数（即使为空）的类型是非平凡析构，"
              "get_destory_() 返回 &destory_impl");

// 含原始指针的"伪资源"类型：由于无用户析构函数，属于平凡析构；
// m_destory = nullptr，state_pool 不会释放指针——指针生命周期由调用者负责。
struct raw_ptr_holder {
    int* p; // 持有裸指针但无析构函数
};
static_assert(std::is_trivially_destructible_v<std::optional<raw_ptr_holder>>,
              "裸指针包装体没有用户析构函数，属于平凡析构；"
              "get_destory_() 返回 nullptr，调用者必须自行管理指针生命周期");

} // anonymous namespace

// 运行期：含用户析构函数的类型，验证池复用时析构依然被调用
//
// tracked_struct 含 shared_ptr 成员和用户析构函数：
//   - is_trivially_destructible = false → get_destory_() 返回 &destory_impl
//   - 池复用时 State<T>::reuse() 必须恢复 m_destory，否则析构不会被调用
TEST_F(state_pool_test, get_destory_user_dtor_type_dtor_called_on_pool_reuse)
{
    struct tracked_struct {
        std::shared_ptr<int> owner; // 通过 weak_ptr 间接观察析构是否发生
        ~tracked_struct()
        {} // 用户析构函数 → 非平凡析构
    };
    static_assert(!std::is_trivially_destructible_v<std::optional<tracked_struct>>);

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

    // 第二轮（池复用）：关键验证——若 reuse() 未恢复 m_destory，析构不会被调用
    {
        auto promise = mc::make_promise<tracked_struct>(io_context_);
        auto future  = promise.get_future();
        auto sp      = std::make_shared<int>(2);
        weak2        = sp;
        promise.set_value(tracked_struct{std::move(sp)});
        EXPECT_EQ(*future.get().owner, 2);
    }
    EXPECT_TRUE(weak2.expired()) << "池复用后，含用户析构函数的类型析构仍应被调用；"
                                    "若此断言失败，说明 reuse() 未恢复 m_destory，destory_impl 被跳过";
}

// 运行期：vector<shared_ptr> 多次复用，确保每轮所有堆对象均被析构
// vector 是非平凡析构，get_destory_() 返回 &destory_impl
TEST_F(state_pool_test, get_destory_vector_heap_objects_freed_on_reuse)
{
    static_assert(!std::is_trivially_destructible_v<std::optional<std::vector<std::shared_ptr<int>>>>);

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
                                        << " 个元素应在 destory_impl 调用后析构";
    }
}

// 运行期：平凡析构类型（int）—— get_destory_() 返回 nullptr，destory_impl 不被调用；
// 但因 reuse() 中有显式 m_result.~result_type()/placement-new，旧值不会泄露给新一轮。
// 验证新一轮始终能获取到正确的新值，而非上一轮残留的旧值。
TEST_F(state_pool_test, get_destory_trivial_type_no_stale_value_after_reuse)
{
    static_assert(std::is_trivially_destructible_v<std::optional<int>>, "int 是平凡析构：destory_impl 不被调用，"
                                                                        "reuse() 的显式 placement-new 负责清除旧值");

    for (int i = 0; i < 5; ++i) {
        auto promise = mc::make_promise<int>(io_context_);
        auto future  = promise.get_future();
        promise.set_value(i * 111);
        EXPECT_EQ(future.get(), i * 111) << "第 " << i + 1 << " 轮：复用 State 后应读到本轮写入的值 " << i * 111
                                         << "，而不是上一轮的残留值";
    }
}

// 多线程并发测试
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
