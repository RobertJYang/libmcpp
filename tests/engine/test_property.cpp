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

#include <gtest/gtest.h>
#include <mc/engine/interface.h>
#include <mc/engine/object.h>
#include <mc/engine/property.h>

namespace {
struct Point {
    int x{0};
    int y{0};

    Point() = default;
    Point(int x, int y) : x(x), y(y) {
    }

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

struct test_observer {
    int         m_count{0};
    mc::variant m_last_value;

    void notify(mc::variant value, mc::engine::property_base& property) {
        m_count++;
        m_last_value = value;
    }
};
template <typename T, typename Observer = mc::engine::detail::empty_observer>
using property = mc::engine::property<T, Observer>;
} // namespace

MC_REFLECT(Point, (x)(y))

// 测试基础类型的 property
TEST(PropertyTest, BasicType) {
    // 测试默认构造函数
    property<int> p1;
    EXPECT_EQ(p1.value(), 0);

    // 测试带参数构造函数
    property<int> p2(42);
    EXPECT_EQ(p2.value(), 42);

    // 测试赋值操作符
    p1 = 100;
    EXPECT_EQ(p1.value(), 100);

    // 测试值获取
    EXPECT_EQ(*p1, 100);
    EXPECT_EQ(static_cast<int>(p1), 100);

    // 测试 modify 方法
    p1.modify([](int& val) {
        val += 10;
        return true;
    });
    EXPECT_EQ(p1.value(), 110);
}

// 测试自定义类型的 property
TEST(PropertyTest, CustomType) {
    // 测试默认构造函数
    property<Point> p1;
    EXPECT_EQ(p1.value().x, 0);
    EXPECT_EQ(p1.value().y, 0);

    // 测试带参数构造函数
    Point           pt{10, 20};
    property<Point> p2(pt);
    EXPECT_EQ(p2.value().x, 10);
    EXPECT_EQ(p2.value().y, 20);

    // 测试移动构造
    property<Point> p3(Point{30, 40});
    EXPECT_EQ(p3.value().x, 30);
    EXPECT_EQ(p3.value().y, 40);

    // 测试赋值操作符
    p1 = Point{50, 60};
    EXPECT_EQ(p1.value().x, 50);
    EXPECT_EQ(p1.value().y, 60);

    // 测试修改值
    p1.modify([](Point& p) {
        p.x += 5;
        p.y += 5;
        return true;
    });
    EXPECT_EQ(p1.value().x, 55);
    EXPECT_EQ(p1.value().y, 65);

    EXPECT_TRUE(p1 == Point(55, 65));
    EXPECT_FALSE(p1 != Point(55, 65));

    EXPECT_TRUE(Point(55, 65) == p1);
    EXPECT_FALSE(Point(55, 65) != p1);
}

// 测试比较操作符
TEST(PropertyTest, Comparison) {
    property<int> p1(10);
    property<int> p2(10);
    property<int> p3(20);

    // 测试相等比较
    EXPECT_TRUE(p1 == p2);
    EXPECT_FALSE(p1 == p3);
    EXPECT_TRUE(p1 == 10);
    EXPECT_TRUE(10 == p1);
    EXPECT_TRUE(mc::variant(10) == p1);
    EXPECT_TRUE(p1 == mc::variant(10));

    // 测试不等比较
    EXPECT_FALSE(p1 != p2);
    EXPECT_TRUE(p1 != p3);
    EXPECT_FALSE(p1 != 10);
    EXPECT_TRUE(p1 != 20);
    EXPECT_FALSE(10L != p1);
    EXPECT_TRUE(20L != p1);
    EXPECT_FALSE(mc::variant(10) != p1);
    EXPECT_TRUE(mc::variant(20) != p1);
    EXPECT_FALSE(p1 != mc::variant(10));
    EXPECT_TRUE(p1 != mc::variant(20));
}

// 测试值变更不触发通知的情况
TEST(PropertyTest, NoNotify) {
    property<int> p(10);

    // 设置相同的值不应该触发通知
    p.set_value(10);
    EXPECT_EQ(p.value(), 10);

    // modify 返回 false 不应该触发通知
    p.modify([](int& val) {
        val = 20;
        return false;
    });
    EXPECT_EQ(p.value(), 20);
}

// 测试 variant 转换
TEST(PropertyTest, VariantConversion) {
    property<int> p(42);

    // 测试 to_variant
    mc::variant v;
    to_variant(p, v);
    EXPECT_EQ(v.as<int>(), 42);

    // 测试 from_variant
    mc::variant v2 = 100;
    from_variant(v2, p);
    EXPECT_EQ(p.value(), 100);
}

// 测试通知机制
TEST(PropertyTest, Notify) {
    property<int, test_observer> p(10);
    EXPECT_EQ(p.observer().m_count, 0);

    // 设置新值，应该触发通知
    p.set_value(20);
    EXPECT_EQ(p.observer().m_count, 1);
    EXPECT_EQ(p.observer().m_last_value, 20);

    // 再次设置同样的值，不应该触发通知
    p.set_value(20);
    EXPECT_EQ(p.observer().m_count, 1);

    // 使用 modify 修改并返回 true，应该触发通知
    p.modify([](int& val) {
        val = 30;
        return true;
    });
    EXPECT_EQ(p.observer().m_count, 2);
    EXPECT_EQ(p.observer().m_last_value, 30);

    // 直接赋值新值，应该触发通知
    p = 40;
    EXPECT_EQ(p.observer().m_count, 3);
    EXPECT_EQ(p.observer().m_last_value, 40);
}