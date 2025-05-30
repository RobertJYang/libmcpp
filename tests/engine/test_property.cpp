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

#include <cmath>
#include <functional>
#include <gtest/gtest.h>
#include <mc/engine/interface.h>
#include <mc/engine/object.h>
#include <mc/engine/property.h>
#include <sstream>

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

// 测试同步属性
TEST(PropertyTest, SingleSyncProperty) {
    // 测试单个同步源属性
    property<int, test_observer> source(10);
    property<double, test_observer> sync(
        [](const int& src) { return src * 2.0; },
        source
    );
    EXPECT_DOUBLE_EQ(sync.value(), 20.0);
    EXPECT_DOUBLE_EQ(sync.value(true), 20.0);

    // 更新源属性值
    source = 20;
    EXPECT_DOUBLE_EQ(sync.value(), 40.0);
    EXPECT_DOUBLE_EQ(sync.value(true), 40.0);

    // 更新属性值
    sync = 30.0;
    EXPECT_EQ(source.value(), 20);
    EXPECT_EQ(source.value(true), 20);
    EXPECT_DOUBLE_EQ(sync.value(), 30.0);
    EXPECT_DOUBLE_EQ(sync.value(true), 40.0);
}

TEST(PropertyTest, MultiSyncProperty) {
    // 测试多个同步源属性
    property<Point, test_observer> p1(Point{1, 2});
    property<Point, test_observer> p2(Point{4, 6});
    property<double, test_observer> distance(
        [](const Point& p1, const Point& p2) {
            auto dx = p2.x - p1.x;
            auto dy = p2.y - p1.y;
            return std::sqrt(dx * dx + dy * dy);
        },
        p1, p2
    );
    EXPECT_DOUBLE_EQ(distance.value(), 5.0);
    EXPECT_DOUBLE_EQ(distance.value(true), 5.0);

    // 更新源属性值
    p1 = Point{2, 3};
    EXPECT_DOUBLE_EQ(distance.value(), std::sqrt(13.0));
    EXPECT_DOUBLE_EQ(distance.value(true), std::sqrt(13.0));

    distance = 10.0;
    EXPECT_EQ(p1.value().x, 2);
    EXPECT_EQ(p1.value().y, 3);
    EXPECT_EQ(p2.value().x, 4);
    EXPECT_EQ(p2.value().y, 6);
    EXPECT_DOUBLE_EQ(distance.value(), 10.0);
    EXPECT_DOUBLE_EQ(distance.value(true), std::sqrt(13.0));
}

TEST(PropertyTest, MultiDiffTypeSyncProperty) {
    property<int, test_observer> p1(2);
    property<Point, test_observer> p2(Point{5, 6});
    property<double, test_observer> distance(
        [](const int& p1, const Point& p2) {
            return std::sqrt((p2.x - p1) * (p2.x - p1) + (p2.y - p1) * (p2.y - p1));
        },
        p1, p2
    );
    EXPECT_DOUBLE_EQ(distance.value(), 5.0);
    EXPECT_DOUBLE_EQ(distance.value(true), 5.0);

    p2 = Point{8, 10};
    EXPECT_DOUBLE_EQ(distance.value(), 10.0);
    EXPECT_DOUBLE_EQ(distance.value(true), 10.0);

    distance = 20.0;
    EXPECT_EQ(p1.value(), 2);
    EXPECT_EQ(p2.value().x, 8);
    EXPECT_EQ(p2.value().y, 10);
    EXPECT_DOUBLE_EQ(distance.value(), 20.0);
    EXPECT_DOUBLE_EQ(distance.value(true), 10.0);
}

// 测试引用属性
TEST(PropertyTest, SingleRefProperty) {
    // 测试单个引用源属性
    property<int, test_observer>    source(10);
    property<double, test_observer> ref(
        std::function<double(const property<int, test_observer>&)>(
            [](const property<int, test_observer>& src) -> double {
                return src.value() * 2.0;
            }),
        std::function<void(const double&, property<int, test_observer>&)>(
            [](const double& value, property<int, test_observer>& src) -> void {
                src = static_cast<int>(value / 2);
            }),
        source);
    EXPECT_EQ(source.value(), 10);
    EXPECT_EQ(source.value(true), 10);
    EXPECT_DOUBLE_EQ(ref.value(), 20.0);
    EXPECT_DOUBLE_EQ(ref.value(true), 20.0);

    // 通过引用属性设置源属性值
    ref = 30.0;
    EXPECT_EQ(source.value(), 15);
    EXPECT_EQ(source.value(true), 15);
    EXPECT_DOUBLE_EQ(ref.value(), 30.0);
    EXPECT_DOUBLE_EQ(ref.value(true), 30.0);
}

// 测试多个引用源属性
TEST(PropertyTest, MultiRefProperty) {
    property<Point, test_observer>  p1(Point{1, 2});
    property<Point, test_observer>  p2(Point{4, 6});
    property<double, test_observer> distance(
        std::function<double(const property<Point, test_observer>&,
                             const property<Point, test_observer>&)>(
            [](const property<Point, test_observer>& p1,
               const property<Point, test_observer>& p2) -> double {
                auto dx = p2.value().x - p1.value().x;
                auto dy = p2.value().y - p1.value().y;
                return std::sqrt(dx * dx + dy * dy);
            }),
        std::function<void(const double&, property<Point, test_observer>&,
                           property<Point, test_observer>&)>(
            [](const double& value, property<Point, test_observer>& p1,
               property<Point, test_observer>& p2) -> void {
                // 根据距离更新两个点的位置
                auto angle = std::atan2(p2.value().y - p1.value().y, p2.value().x - p1.value().x);
                p1         = Point{0, 0};
                p2         = Point{static_cast<int>(std::floor(value * std::cos(angle))),
                           static_cast<int>(std::ceil(value * std::sin(angle)))};
            }),
        p1, p2);
    EXPECT_DOUBLE_EQ(distance.value(), 5.0);
    EXPECT_DOUBLE_EQ(distance.value(true), 5.0);

    p2 = Point{7, 10};
    EXPECT_DOUBLE_EQ(distance.value(), 5.0);
    EXPECT_DOUBLE_EQ(distance.value(true), 10.0);

    // 通过引用属性设置源属性值
    distance = 10.0;
    EXPECT_EQ(p1.value().x, 0);
    EXPECT_EQ(p1.value().y, 0);
    EXPECT_EQ(p2.value().x, 6);
    EXPECT_EQ(p2.value().y, 8);
    EXPECT_DOUBLE_EQ(distance.value(), 10.0);
    EXPECT_DOUBLE_EQ(distance.value(true), 10.0);
}

// 测试多个不同类型引用源属性
TEST(PropertyTest, MultiDiffTypeRefProperty) {
    property<uint32_t, test_observer> count(10);
    property<uint8_t, test_observer> element(10);
    property<std::vector<uint8_t>, test_observer> container(
        [](const property<uint32_t, test_observer>& count,
           const property<uint8_t, test_observer>& element) -> std::vector<uint8_t> {
            std::vector<uint8_t> result(count.value(), element.value());
            return result;
        },
        [](const std::vector<uint8_t>& value,
           property<uint32_t, test_observer>& count,
           property<uint8_t, test_observer>& element) -> void {
            count = value.size();
            element = value[0];
        },
        count, element);

    EXPECT_EQ(container.value().size(), 10);
    for (uint32_t i = 0; i < container.value().size(); i++) {
        EXPECT_EQ(container.value()[i], 10);
    }

    count = 20;
    EXPECT_EQ(container.value().size(), 10);
    for (uint32_t i = 0; i < container.value().size(); i++) {
        EXPECT_EQ(container.value()[i], 10);
    }
    auto v = container.value(true);
    EXPECT_EQ(v.size(), 20);
    for (uint32_t i = 0; i < v.size(); i++) {
        EXPECT_EQ(v[i], 10);
    }

    element = 20;
    EXPECT_EQ(container.value().size(), 20);
    auto v2 = container.value(true);
    EXPECT_EQ(v2.size(), 20);
    for (uint32_t i = 0; i < v2.size(); i++) {
        EXPECT_EQ(v2[i], 20);
    }
}

// 测试单个只读引用属性
TEST(PropertyTest, ReadOnlySingleRefProperty) {
    property<int, test_observer> source(10);
    property<double, test_observer> ref(
        [](const property<int, test_observer>& src) -> double {
            return src.value() * 2.0;
        },
        nullptr,
        source);

    EXPECT_DOUBLE_EQ(ref.value(), 20.0);
    EXPECT_DOUBLE_EQ(ref.value(true), 20.0);

    // 尝试设置只读引用属性值
    ref = 30.0;
    EXPECT_EQ(source.value(), 10); // 源属性值不应改变
    EXPECT_DOUBLE_EQ(ref.value(), 30.0);
    EXPECT_DOUBLE_EQ(ref.value(true), 20.0);
}

// 测试多个只读引用属性
TEST(PropertyTest, ReadOnlyMultiRefProperty) {
    property<int, test_observer> source1(10);
    property<int, test_observer> source2(20);
    property<double, test_observer> ref(
        [](const property<int, test_observer>& src1,
           const property<int, test_observer>& src2) -> double {
            return src1.value() * 2.0 + src2.value() * 3.0;
        },
        nullptr,
        source1, source2);

    EXPECT_DOUBLE_EQ(ref.value(), 80.0);
    EXPECT_DOUBLE_EQ(ref.value(true), 80.0);

    source1 = 30;
    EXPECT_DOUBLE_EQ(ref.value(), 80.0);
    EXPECT_DOUBLE_EQ(ref.value(true), 120.0);

    source2 = 40;
    EXPECT_DOUBLE_EQ(ref.value(), 120.0);
    EXPECT_DOUBLE_EQ(ref.value(true), 180.0);

    ref = 50.0;
    EXPECT_EQ(source1.value(), 30);
    EXPECT_EQ(source2.value(), 40);
}

// 测试多个不同类型只读引用属性
TEST(PropertyTest, ReadOnlyMultiDiffTypeRefProperty) {
    property<uint32_t, test_observer> hour(10);
    property<uint8_t, test_observer> minute(20);
    property<std::string, test_observer> ref(
        [](const property<uint32_t, test_observer>& src1,
           const property<uint8_t, test_observer>& src2) -> std::string {
            std::ostringstream oss;
            oss << std::dec << src1.value() << ":" << std::dec
                << static_cast<uint32_t>(src2.value());
            return oss.str();
        },
        nullptr,
        hour, minute);

    EXPECT_EQ(ref.value(), "10:20");
    EXPECT_EQ(ref.value(true), "10:20");

    hour = 30;
    EXPECT_EQ(ref.value(), "10:20");
    EXPECT_EQ(ref.value(true), "30:20");

    minute = 40;
    EXPECT_EQ(ref.value(), "30:20");
    EXPECT_EQ(ref.value(true), "30:40");
}