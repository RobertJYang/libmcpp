// 测试 variant 的默认构造函数
TEST(VariantConstructionTest, DefaultConstructor) {
    variant v;
    EXPECT_EQ(v.type(), variant_type::null);
}

// 测试 variant 的 bool 构造函数
TEST(VariantConstructionTest, BoolConstructor) {
    variant v = true;
    EXPECT_EQ(v.type(), variant_type::boolean);
    EXPECT_EQ(v.as<bool>(), true);
}

// 测试 variant 的 int 构造函数
TEST(VariantConstructionTest, IntConstructor) {
    variant v = 123;
    EXPECT_EQ(v.type(), variant_type::integer);
    EXPECT_EQ(v.as<int>(), 123);
}

// 测试 variant 的 double 构造函数
TEST(VariantConstructionTest, DoubleConstructor) {
    variant v = 123.456;
    EXPECT_EQ(v.type(), variant_type::floating);
    EXPECT_DOUBLE_EQ(v.as<double>(), 123.456);
}

// 测试 variant 的 const char* 构造函数
TEST(VariantConstructionTest, ConstCharPtrConstructor) {
    variant v = "hello";
    EXPECT_EQ(v.type(), variant_type::string);
    EXPECT_EQ(v.as<std::string>(), "hello");
}

// 测试 variant 的 std::string 构造函数
TEST(VariantConstructionTest, StringConstructor) {
    std::string s = "hello";
    variant     v = s;
    EXPECT_EQ(v.type(), variant_type::string);
    EXPECT_EQ(v.as<std::string>(), "hello");
}

// 测试 variant 的 std::string_view 构造函数
TEST(VariantConstructionTest, StringViewConstructor) {
    std::string_view sv = "hello";
    variant          v  = sv;
    EXPECT_EQ(v.type(), variant_type::string);
    EXPECT_EQ(v.as<std::string>(), "hello");
}

// 测试 variant 的 blob 构造函数
TEST(VariantConstructionTest, BlobConstructor) {
    blob    b = {1, 2, 3, 4, 5};
    variant v = b;
    EXPECT_EQ(v.type(), variant_type::blob);
    EXPECT_EQ(v.as<blob>(), b);
}

// 测试 variant 的 dict 构造函数
TEST(VariantConstructionTest, DictConstructor) {
    dict    d({{"key1", 123}, {"key2", "value"}, {"key3", true}});
    variant v = d;
    EXPECT_EQ(v.type(), variant_type::dict);
    EXPECT_EQ(v.as<dict>(), d);
}

// 测试 variant 的 mutable_dict 构造函数
TEST(VariantConstructionTest, MutableDictConstructor) {
    mutable_dict md({{"key1", 123}, {"key2", "value"}, {"key3", true}});
    variant      v = md;
    EXPECT_EQ(v.type(), variant_type::dict);
    EXPECT_EQ(v.as<dict>(), dict(md));
}

// 测试 variant 的 variants 构造函数
TEST(VariantConstructionTest, VariantsConstructor) {
    variants vs = {123, "value", true};
    variant  v  = vs;
    EXPECT_EQ(v.type(), variant_type::array);
    EXPECT_EQ(v.as<variants>(), vs);
}

// 测试 variant 的拷贝构造函数
TEST(VariantConstructionTest, CopyConstructor) {
    variant v1 = 123;
    variant v2 = v1;
    EXPECT_EQ(v2.type(), variant_type::integer);
    EXPECT_EQ(v2.as<int>(), 123);
}

// 测试 variant 的移动构造函数
TEST(VariantConstructionTest, MoveConstructor) {
    variant v1 = 123;
    variant v2 = std::move(v1);
    EXPECT_EQ(v2.type(), variant_type::integer);
    EXPECT_EQ(v2.as<int>(), 123);
    EXPECT_EQ(v1.type(), variant_type::null);
}

// 测试 variant 的拷贝赋值运算符
TEST(VariantConstructionTest, CopyAssignmentOperator) {
    variant v1 = 123;
    variant v2;
    v2 = v1;
    EXPECT_EQ(v2.type(), variant_type::integer);
    EXPECT_EQ(v2.as<int>(), 123);
}

// 测试 variant 的移动赋值运算符
TEST(VariantConstructionTest, MoveAssignmentOperator) {
    variant v1 = 123;
    variant v2;
    v2 = std::move(v1);
    EXPECT_EQ(v2.type(), variant_type::integer);
    EXPECT_EQ(v2.as<int>(), 123);
    EXPECT_EQ(v1.type(), variant_type::null);
}

// 测试 variant 的 bool 赋值运算符
TEST(VariantConstructionTest, BoolAssignmentOperator) {
    variant v;
    v = true;
    EXPECT_EQ(v.type(), variant_type::boolean);
    EXPECT_EQ(v.as<bool>(), true);
}

// 测试 variant 的 int 赋值运算符
TEST(VariantConstructionTest, IntAssignmentOperator) {
    variant v;
    v = 123;
    EXPECT_EQ(v.type(), variant_type::integer);
    EXPECT_EQ(v.as<int>(), 123);
}

// 测试 variant 的 double 赋值运算符
TEST(VariantConstructionTest, DoubleAssignmentOperator) {
    variant v;
    v = 123.456;
    EXPECT_EQ(v.type(), variant_type::floating);
    EXPECT_DOUBLE_EQ(v.as<double>(), 123.456);
}

// 测试 variant 的 const char* 赋值运算符
TEST(VariantConstructionTest, ConstCharPtrAssignmentOperator) {
    variant v;
    v = "hello";
    EXPECT_EQ(v.type(), variant_type::string);
    EXPECT_EQ(v.as<std::string>(), "hello");
}

// 测试 variant 的 std::string 赋值运算符
TEST(VariantConstructionTest, StringAssignmentOperator) {
    variant     v;
    std::string s = "hello";
    v             = s;
    EXPECT_EQ(v.type(), variant_type::string);
    EXPECT_EQ(v.as<std::string>(), "hello");
}

// 测试 variant 的 std::string_view 赋值运算符
TEST(VariantConstructionTest, StringViewAssignmentOperator) {
    variant          v;
    std::string_view sv = "hello";
    v                   = sv;
    EXPECT_EQ(v.type(), variant_type::string);
    EXPECT_EQ(v.as<std::string>(), "hello");
}

// 测试 variant 的 blob 赋值运算符
TEST(VariantConstructionTest, BlobAssignmentOperator) {
    variant v;
    blob    b = {1, 2, 3, 4, 5};
    v         = b;
    EXPECT_EQ(v.type(), variant_type::blob);
    EXPECT_EQ(v.as<blob>(), b);
}

// 测试 variant 的 dict 赋值运算符
TEST(VariantConstructionTest, DictAssignmentOperator) {
    variant v;
    dict    d({{"key1", 123}, {"key2", "value"}, {"key3", true}});
    v = d;
    EXPECT_EQ(v.type(), variant_type::dict);
    EXPECT_EQ(v.as<dict>(), d);
}

// 测试 variant 的 mutable_dict 赋值运算符
TEST(VariantConstructionTest, MutableDictAssignmentOperator) {
    variant      v;
    mutable_dict md({{"key1", 123}, {"key2", "value"}, {"key3", true}});
    v = md;
    EXPECT_EQ(v.type(), variant_type::dict);
    EXPECT_EQ(v.as<dict>(), dict(md));
}

// 测试 variant 的 variants 赋值运算符
TEST(VariantConstructionTest, VariantsAssignmentOperator) {
    variant  v;
    variants vs = {123, "value", true};
    v           = vs;
    EXPECT_EQ(v.type(), variant_type::array);
    EXPECT_EQ(v.as<variants>(), vs);
}