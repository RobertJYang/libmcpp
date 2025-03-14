#include "mc/event.h"
#include <gtest/gtest.h>

using namespace mc;

// 自定义事件类型
class test_event : public event {
public:
    test_event(int value) : value_(value) {
    }
    int value() const {
        return value_;
    }

private:
    int value_;
};

// 自定义事件处理器
class test_handler : public event_handler {
public:
    test_handler() : handled_count_(0), last_value_(0) {
    }

    bool handle_event(event& e) override {
        if (auto* te = dynamic_cast<test_event*>(&e)) {
            handled_count_++;
            last_value_ = te->value();
            te->accept();
            return true;
        }
        return false;
    }

    int handled_count() const {
        return handled_count_;
    }
    int last_value() const {
        return last_value_;
    }

private:
    int handled_count_;
    int last_value_;
};

// 自定义事件过滤器
class test_filter : public event_filter {
public:
    test_filter() : filtered_count_(0) {
    }

    bool filter_event(event& e, event_handler* target) override {
        if (auto* te = dynamic_cast<test_event*>(&e)) {
            filtered_count_++;
            // 如果值为负数，则过滤掉事件
            if (te->value() < 0) {
                return true; // 事件被过滤器处理
            }
            // 否则让事件继续传递给目标处理器
        }
        return false;
    }

    int filtered_count() const {
        return filtered_count_;
    }

private:
    int filtered_count_;
};

// 测试事件基本功能
TEST(EventTest, BasicEvent) {
    test_event e(42);
    EXPECT_FALSE(e.is_accepted());

    e.accept();
    EXPECT_TRUE(e.is_accepted());

    e.ignore();
    EXPECT_FALSE(e.is_accepted());
}

// 测试事件处理器
TEST(EventTest, EventHandler) {
    test_handler handler;
    test_event   e1(10);
    test_event   e2(20);

    // 处理事件
    EXPECT_TRUE(handler.handle_event(e1));
    EXPECT_EQ(1, handler.handled_count());
    EXPECT_EQ(10, handler.last_value());
    EXPECT_TRUE(e1.is_accepted());

    // 处理另一个事件
    EXPECT_TRUE(handler.handle_event(e2));
    EXPECT_EQ(2, handler.handled_count());
    EXPECT_EQ(20, handler.last_value());
    EXPECT_TRUE(e2.is_accepted());
}

// 测试事件过滤器
TEST(EventTest, EventFilter) {
    test_handler handler;
    test_filter  filter;
    test_event   e1(10); // 正值，应该传递给处理器
    test_event   e2(-5); // 负值，应该被过滤器拦截

    // 测试正值事件
    EXPECT_FALSE(filter.filter_event(e1, &handler));
    EXPECT_EQ(1, filter.filtered_count());
    EXPECT_TRUE(handler.handle_event(e1));
    EXPECT_EQ(1, handler.handled_count());

    // 测试负值事件
    EXPECT_TRUE(filter.filter_event(e2, &handler));
    EXPECT_EQ(2, filter.filtered_count());
    // 事件被过滤器拦截，不应该到达处理器
    EXPECT_EQ(1, handler.handled_count());
}