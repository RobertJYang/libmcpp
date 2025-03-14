// 测试 mutable_dict 的 operator() 方法
TEST(MutableDictOperationsTest, OperatorParentheses) {
    mutable_dict md;

    // 测试 operator() 添加新键值对
    md("key1", 123);
    md("key2", "value");
    md("key3", true);

    // 验证添加的键值对
    EXPECT_EQ(md.size(), 3);
    EXPECT_EQ(md["key1"].as<int>(), 123);
    EXPECT_EQ(md["key2"].as<std::string>(), "value");
    EXPECT_EQ(md["key3"].as<bool>(), true);

    // 测试 operator() 修改现有键值对
    md("key1", 456);
    EXPECT_EQ(md["key1"].as<int>(), 456);
}

// 测试 mutable_dict 的 operator[] 方法
TEST(MutableDictOperationsTest, OperatorSquareBrackets) {
    mutable_dict md({{"key1", 123}, {"key2", "value"}, {"key3", true}});
}

// 测试 mutable_dict 的 at 方法
TEST(MutableDictOperationsTest, At) {
    mutable_dict md({{"key1", 123}, {"key2", "value"}, {"key3", true}});
}

// 测试 mutable_dict 的 contains 方法
TEST(MutableDictOperationsTest, Contains) {
    mutable_dict md({{"key1", 123}, {"key2", "value"}, {"key3", true}});
}

// 测试 mutable_dict 的 size 方法
TEST(MutableDictOperationsTest, Size) {
    mutable_dict md({{"key1", 123}, {"key2", "value"}, {"key3", true}});
}

// 测试 mutable_dict 的 empty 方法
TEST(MutableDictOperationsTest, Empty) {
    mutable_dict md1({{"key1", 123}, {"key2", "value"}, {"key3", true}});
}

// 测试 mutable_dict 的 keys 方法
TEST(MutableDictOperationsTest, Keys) {
    mutable_dict md({{"key1", 123}, {"key2", "value"}, {"key3", true}});
}

// 测试 mutable_dict 的 values 方法
TEST(MutableDictOperationsTest, Values) {
    mutable_dict md({{"key1", 123}, {"key2", "value"}, {"key3", true}});
}

// 测试 mutable_dict 的 items 方法
TEST(MutableDictOperationsTest, Items) {
    mutable_dict md({{"key1", 123}, {"key2", "value"}, {"key3", true}});
}

// 测试 mutable_dict 的 erase 方法
TEST(MutableDictOperationsTest, Erase) {
    mutable_dict md({{"key1", 123}, {"key2", "value"}, {"key3", true}});
}

// 测试 mutable_dict 的 clear 方法
TEST(MutableDictOperationsTest, Clear) {
    mutable_dict md({{"key1", 123}, {"key2", "value"}, {"key3", true}});
}