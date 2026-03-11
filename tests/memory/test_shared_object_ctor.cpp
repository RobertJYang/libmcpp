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

#include <array>
#include <gtest/gtest.h>
#include <mc/exception.h>
#include <mc/memory.h>
#include <memory>

// 测试用的复杂对象
class ctor_test_object : public mc::enable_shared_from_this<ctor_test_object> {
public:
    ctor_test_object()
        : m_id(s_next_id++), m_data(std::make_unique<int>(42))
    {
        ++s_construct_count;
    }

    explicit ctor_test_object(int value)
        : m_id(s_next_id++), m_data(std::make_unique<int>(value))
    {
        ++s_construct_count;
    }

    ~ctor_test_object()
    {
        ++s_destruct_count;
        m_data.reset(); // 清空资源
    }

    int get_id() const
    {
        return m_id;
    }
    int get_data() const
    {
        return m_data ? *m_data : -1;
    }

    void set_data(int value)
    {
        if (m_data) {
            *m_data = value;
        }
    }

    static void reset_counters()
    {
        s_construct_count = 0;
        s_destruct_count  = 0;
        s_next_id         = 1;
    }

    static int get_construct_count()
    {
        return s_construct_count;
    }
    static int get_destruct_count()
    {
        return s_destruct_count;
    }

private:
    int                  m_id;
    std::unique_ptr<int> m_data;
    static int           s_construct_count;
    static int           s_destruct_count;
    static int           s_next_id;
};

int ctor_test_object::s_construct_count = 0;
int ctor_test_object::s_destruct_count  = 0;
int ctor_test_object::s_next_id         = 1;

// 异常安全测试对象
class exception_test_object : public mc::enable_shared_from_this<exception_test_object> {
public:
    explicit exception_test_object(bool throw_in_constructor = false)
    {
        if (throw_in_constructor) {
            throw std::runtime_error("构造函数中的测试异常");
        }
        ++s_construct_count;
    }

    ~exception_test_object()
    {
        ++s_destruct_count;
    }

    void may_throw()
    {
        if (m_should_throw) {
            throw std::runtime_error("测试异常");
        }
    }

    void set_throw_flag(bool should_throw)
    {
        m_should_throw = should_throw;
    }

    static void reset_counters()
    {
        s_construct_count = 0;
        s_destruct_count  = 0;
    }

    static int get_construct_count()
    {
        return s_construct_count;
    }
    static int get_destruct_count()
    {
        return s_destruct_count;
    }

private:
    bool       m_should_throw = false;
    static int s_construct_count;
    static int s_destruct_count;
};

int exception_test_object::s_construct_count = 0;
int exception_test_object::s_destruct_count  = 0;

class SharedPtrConstructTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ctor_test_object::reset_counters();
        exception_test_object::reset_counters();
    }

    void TearDown() override
    {
        // 确保所有对象都被正确析构
        EXPECT_EQ(ctor_test_object::get_construct_count(), ctor_test_object::get_destruct_count());
        EXPECT_EQ(exception_test_object::get_construct_count(), exception_test_object::get_destruct_count());
    }
};

// 测试未初始化状态下的操作
TEST_F(SharedPtrConstructTest, UninitializedStateOperations)
{
    // 测试对象创建但未被 shared_ptr 管理的状态
    auto* raw_obj = new ctor_test_object(100);

    // 对象应该处于未管理状态
    EXPECT_FALSE(raw_obj->is_managed());
    EXPECT_FALSE(raw_obj->is_destroyed());
    EXPECT_EQ(raw_obj->ref_count(), std::numeric_limits<uint32_t>::max());

    // 现在创建 shared_ptr 来管理它
    mc::shared_ptr<ctor_test_object> ptr(raw_obj);

    // 对象现在应该被管理
    EXPECT_TRUE(raw_obj->is_managed());
    EXPECT_FALSE(raw_obj->is_destroyed());
    EXPECT_EQ(raw_obj->ref_count(), 1);
    EXPECT_EQ(ptr.use_count(), 1);
}

TEST_F(SharedPtrConstructTest, FromRawWithoutAddRefThrowsOnReset)
{
    auto* raw_obj = new ctor_test_object(150);
    EXPECT_FALSE(raw_obj->is_managed());

    EXPECT_THROW(ctor_test_object::from_raw(raw_obj), mc::invalid_op_exception);
    delete raw_obj;
}

TEST_F(SharedPtrConstructTest, ResetThrowsWhenRefCountAlreadyDestroyed)
{
    auto*                            raw_obj = new ctor_test_object(160);
    mc::shared_ptr<ctor_test_object> ptr(raw_obj);

    EXPECT_TRUE(raw_obj->is_managed());
    EXPECT_EQ(ptr.use_count(), 1);

    bool should_delete = raw_obj->release_ref();
    EXPECT_TRUE(should_delete);
    EXPECT_TRUE(raw_obj->is_destroyed());

    try {
        ptr.reset();
        FAIL() << "reset 应该抛出 mc::invalid_op_exception";
    } catch (const mc::invalid_op_exception&) {
    }

    EXPECT_FALSE(ptr);
    delete raw_obj;
}

// 测试 shared_from_this 在不同状态下的行为
TEST_F(SharedPtrConstructTest, SharedFromThisStates)
{
    auto obj = mc::make_shared<ctor_test_object>(200);

    // 正常情况下的 shared_from_this
    auto shared1 = obj->shared_from_this();
    EXPECT_EQ(shared1.get(), obj.get());
    EXPECT_EQ(obj.use_count(), 2);

    // weak_from_this
    auto weak = obj->weak_from_this();
    EXPECT_FALSE(weak.expired());
    EXPECT_EQ(weak.get(), obj.get());

    // 对象销毁后 weak_ptr 的行为
    obj.reset();
    shared1.reset();

    EXPECT_TRUE(weak.expired());
    EXPECT_FALSE(weak.lock());
}

// 测试异常安全性
TEST_F(SharedPtrConstructTest, ExceptionSafety)
{
    // 测试构造函数抛异常的情况
    EXPECT_THROW({
        mc::make_shared<exception_test_object>(true);
    },
                 std::runtime_error);

    // 确保没有内存泄漏
    EXPECT_EQ(exception_test_object::get_construct_count(), 0);
    EXPECT_EQ(exception_test_object::get_destruct_count(), 0);

    // 测试正常构造但方法抛异常
    auto obj = mc::make_shared<exception_test_object>(false);
    EXPECT_EQ(exception_test_object::get_construct_count(), 1);

    obj->set_throw_flag(true);
    EXPECT_THROW(obj->may_throw(), std::runtime_error);

    // 对象应该仍然有效
    EXPECT_TRUE(obj);
    EXPECT_EQ(obj.use_count(), 1);

    // 清理
    obj.reset();
    EXPECT_EQ(exception_test_object::get_destruct_count(), 1);
}

// 测试自赋值和自交换
TEST_F(SharedPtrConstructTest, SelfSwap)
{
    auto  obj            = mc::make_shared<ctor_test_object>(300);
    auto* original_ptr   = obj.get();
    auto  original_count = obj.use_count();

    // 自交换
    obj.swap(obj);
    EXPECT_EQ(obj.get(), original_ptr);
    EXPECT_EQ(obj.use_count(), original_count);

    // 确保对象数据完整
    EXPECT_EQ(obj->get_data(), 300);
}

// 测试空指针和 nullptr 操作
TEST_F(SharedPtrConstructTest, NullptrOperations)
{
    mc::shared_ptr<ctor_test_object> null_ptr;
    mc::shared_ptr<ctor_test_object> nullptr_ptr(nullptr);

    // 比较操作
    EXPECT_TRUE(null_ptr == nullptr_ptr);
    EXPECT_TRUE(null_ptr == nullptr);
    EXPECT_TRUE(nullptr == null_ptr);
    EXPECT_FALSE(null_ptr != nullptr_ptr);

    // 赋值操作
    auto obj = mc::make_shared<ctor_test_object>(400);
    null_ptr = obj;
    EXPECT_TRUE(null_ptr);
    EXPECT_EQ(null_ptr.get(), obj.get());

    null_ptr = nullptr;
    EXPECT_FALSE(null_ptr);
    EXPECT_EQ(null_ptr.get(), nullptr);

    // swap 和 reset 操作
    null_ptr.swap(nullptr_ptr);
    EXPECT_FALSE(null_ptr);
    EXPECT_FALSE(nullptr_ptr);

    null_ptr.reset();
    EXPECT_FALSE(null_ptr);
}

// 测试引用计数溢出边界
TEST_F(SharedPtrConstructTest, ReferenceCountBoundaries)
{
    auto obj = mc::make_shared<ctor_test_object>(500);

    // 测试大量引用
    std::vector<mc::shared_ptr<ctor_test_object>> ptrs;
    ptrs.reserve(10000);

    for (int i = 0; i < 10000; ++i) {
        ptrs.push_back(obj);
    }

    EXPECT_EQ(obj.use_count(), 10001); // obj + 10000 copies

    // 逐步释放
    for (int i = 0; i < 5000; ++i) {
        ptrs.pop_back();
    }

    EXPECT_EQ(obj.use_count(), 5001);

    // 清空剩余
    ptrs.clear();
    EXPECT_EQ(obj.use_count(), 1);
    EXPECT_TRUE(obj.unique());
}

// 测试 weak_ptr 相关的边界情况
TEST_F(SharedPtrConstructTest, WeakPtrEdgeCases)
{
    mc::weak_ptr<ctor_test_object> weak;

    // 空 weak_ptr 的操作
    EXPECT_TRUE(weak.expired());
    EXPECT_FALSE(weak.lock());
    EXPECT_EQ(weak.use_count(), 0);

    {
        auto obj = mc::make_shared<ctor_test_object>(600);
        weak     = obj;

        EXPECT_FALSE(weak.expired());
        EXPECT_EQ(weak.use_count(), 1);

        // 创建多个从同一 weak_ptr 的 lock
        auto lock1 = weak.lock();
        auto lock2 = weak.lock();
        auto lock3 = weak.lock();

        EXPECT_TRUE(lock1);
        EXPECT_TRUE(lock2);
        EXPECT_TRUE(lock3);
        EXPECT_EQ(weak.use_count(), 4); // obj + 3 locks
        EXPECT_EQ(lock1.get(), lock2.get());
        EXPECT_EQ(lock2.get(), lock3.get());
    }

    // 所有 shared_ptr 都超出作用域
    EXPECT_TRUE(weak.expired());
    EXPECT_FALSE(weak.lock());
    EXPECT_EQ(weak.use_count(), 0);
}

// 测试类型转换的边界情况
class edge_base : public mc::enable_shared_from_this<edge_base> {
public:
    virtual ~edge_base() = default;
    virtual int get_type() const
    {
        return 1;
    }
};

class edge_derived : public edge_base {
public:
    int get_type() const override
    {
        return 2;
    }
    int get_derived_value() const
    {
        return 42;
    }
};

class edge_other_derived : public edge_base {
public:
    int get_type() const override
    {
        return 3;
    }
    int get_other_value() const
    {
        return 84;
    }
};

TEST_F(SharedPtrConstructTest, TypeConversionEdgeCases)
{
    // 成功的向上转换
    auto                      derived = mc::make_shared<edge_derived>();
    mc::shared_ptr<edge_base> base    = derived;

    EXPECT_EQ(base.get(), derived.get());
    EXPECT_EQ(base->get_type(), 2);
    EXPECT_EQ(derived.use_count(), 2);

    // 成功的动态向下转换
    auto back_to_derived = base.template dynamic_pointer_cast<edge_derived>();
    EXPECT_TRUE(back_to_derived);
    EXPECT_EQ(back_to_derived.get(), derived.get());
    EXPECT_EQ(back_to_derived->get_derived_value(), 42);

    // 失败的动态转换
    auto failed_cast = base.template dynamic_pointer_cast<edge_other_derived>();
    EXPECT_FALSE(failed_cast);
    EXPECT_EQ(failed_cast.get(), nullptr);
    EXPECT_EQ(failed_cast.use_count(), 0);

    // 静态转换（总是成功，但可能不安全）
    auto static_cast_result = base.template static_pointer_cast<edge_derived>();
    EXPECT_TRUE(static_cast_result);
    EXPECT_EQ(static_cast_result.get(), derived.get());

    // 空指针的类型转换
    mc::shared_ptr<edge_base> null_base;
    auto                      null_derived = null_base.template dynamic_pointer_cast<edge_derived>();
    EXPECT_FALSE(null_derived);
    EXPECT_EQ(null_derived.get(), nullptr);
}

// 测试 from_raw 的边界情况
TEST_F(SharedPtrConstructTest, FromRawEdgeCases)
{
    // 使用 nullptr
    auto null_ptr = ctor_test_object::from_raw(nullptr);
    EXPECT_FALSE(null_ptr);
    EXPECT_EQ(null_ptr.get(), nullptr);

    // 使用未管理的对象，增加引用
    auto* raw_obj = new ctor_test_object(700);
    auto  ptr1    = ctor_test_object::from_raw(raw_obj, true);

    EXPECT_TRUE(ptr1);
    EXPECT_EQ(ptr1.get(), raw_obj);
    EXPECT_EQ(ptr1.use_count(), 1);
    EXPECT_TRUE(raw_obj->is_managed());

    // 从已管理对象再创建一个 shared_ptr，也要增加引用
    auto ptr2 = ctor_test_object::from_raw(raw_obj, true);
    EXPECT_TRUE(ptr2);
    EXPECT_EQ(ptr2.get(), raw_obj);
    EXPECT_EQ(ptr1.use_count(), 2); // 现在有两个 shared_ptr
    EXPECT_EQ(ptr2.use_count(), 2);

    // 验证对象数据
    EXPECT_EQ(ptr1->get_data(), 700);
    EXPECT_EQ(ptr2->get_data(), 700);
}

// 测试 detach 操作
TEST_F(SharedPtrConstructTest, DetachOperation)
{
    auto  obj     = mc::make_shared<ctor_test_object>(800);
    auto* raw_ptr = obj.get();

    EXPECT_EQ(obj.use_count(), 1);

    // detach 指针 - 这只是解除智能指针与裸指针的关系，不改变引用计数
    auto* detached_ptr = obj.detach();

    EXPECT_FALSE(obj);
    EXPECT_EQ(obj.get(), nullptr);
    EXPECT_EQ(obj.use_count(), 0);
    EXPECT_EQ(detached_ptr, raw_ptr);

    // 验证 detached 指针仍然有效，但现在需要手动管理
    EXPECT_EQ(detached_ptr->get_data(), 800);
    EXPECT_EQ(detached_ptr->ref_count(), 1); // 引用计数仍然是1

    // 由于 detach 后智能指针不再管理对象，需要手动释放引用和删除对象
    bool should_delete = detached_ptr->release_ref();
    EXPECT_TRUE(should_delete); // 引用计数变为 DESTROYED_VALUE，应该删除对象

    // 需要手动销毁对象
    delete detached_ptr;

    // 对象应该被销毁
    EXPECT_EQ(ctor_test_object::get_construct_count(), 1);
    EXPECT_EQ(ctor_test_object::get_destruct_count(), 1);
}

// 测试析构过程中的异常安全
TEST_F(SharedPtrConstructTest, DestructorExceptionSafety)
{
    // 这个测试主要验证析构过程不会因为异常而导致问题

    {
        std::vector<mc::shared_ptr<ctor_test_object>> ptrs;

        // 创建一些对象
        for (int i = 0; i < 10; ++i) {
            ptrs.push_back(mc::make_shared<ctor_test_object>(i));
        }

        // 创建一些额外的引用
        for (size_t i = 0; i < ptrs.size() && i < 5; ++i) {
            auto copy = ptrs[i];
            ptrs.push_back(copy);
        }

        // 清理一些指针
        for (size_t i = 0; i < ptrs.size() && i < 5; i += 2) {
            ptrs[i].reset();
        }

        // ptrs 超出作用域时，所有对象应该被正确清理
    }

    // 验证清理是否正确
    EXPECT_GT(ctor_test_object::get_construct_count(), 0);
    EXPECT_EQ(ctor_test_object::get_construct_count(), ctor_test_object::get_destruct_count());
}

// 测试 hash 功能（如果实现了的话）
TEST_F(SharedPtrConstructTest, HashFunctionality)
{
    auto obj1      = mc::make_shared<ctor_test_object>(900);
    auto obj2      = mc::make_shared<ctor_test_object>(901);
    auto obj1_copy = obj1;

    std::hash<mc::shared_ptr<ctor_test_object>> hasher;

    // 相同对象的 hash 应该相同
    EXPECT_EQ(hasher(obj1), hasher(obj1_copy));

    // 不同对象的 hash 通常应该不同（虽然不是绝对保证）
    auto hash1 = hasher(obj1);
    auto hash2 = hasher(obj2);

    // 空指针的 hash
    mc::shared_ptr<ctor_test_object> null_ptr;
    auto                             null_hash = hasher(null_ptr);

    // 验证 hash 的一致性
    EXPECT_EQ(hasher(obj1), hash1);
    EXPECT_EQ(hasher(obj2), hash2);
    EXPECT_EQ(hasher(null_ptr), null_hash);
}
