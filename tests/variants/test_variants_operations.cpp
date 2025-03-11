// 测试 variants 的 push_back 方法
TEST(VariantsOperationsTest, PushBack) {
    variants vs;
    vs.push_back(123);
    vs.push_back("hello");
    vs.push_back(true);
    
    EXPECT_EQ(vs.size(), 3);
    EXPECT_EQ(vs[0].as<int>(), 123);
    EXPECT_EQ(vs[1].as<std::string>(), "hello");
    EXPECT_EQ(vs[2].as<bool>(), true);
}

// 测试 variants 的 emplace_back 方法
TEST(VariantsOperationsTest, EmplaceBack) {
    variants vs;
    vs.emplace_back(123);
    vs.emplace_back("hello");
    vs.emplace_back(true);
    
    EXPECT_EQ(vs.size(), 3);
    EXPECT_EQ(vs[0].as<int>(), 123);
    EXPECT_EQ(vs[1].as<std::string>(), "hello");
    EXPECT_EQ(vs[2].as<bool>(), true);
}

// 测试 variants 的 operator[] 方法
TEST(VariantsOperationsTest, OperatorSquareBrackets) {
    variants vs = {123, "hello", true};
    
    EXPECT_EQ(vs[0].as<int>(), 123);
    EXPECT_EQ(vs[1].as<std::string>(), "hello");
    EXPECT_EQ(vs[2].as<bool>(), true);
    
    vs[0] = 456;
    vs[1] = "world";
    vs[2] = false;
    
    EXPECT_EQ(vs[0].as<int>(), 456);
    EXPECT_EQ(vs[1].as<std::string>(), "world");
    EXPECT_EQ(vs[2].as<bool>(), false);
}

// 测试 variants 的 at 方法
TEST(VariantsOperationsTest, At) {
    variants vs = {123, "hello", true};
    
    EXPECT_EQ(vs.at(0).as<int>(), 123);
    EXPECT_EQ(vs.at(1).as<std::string>(), "hello");
    EXPECT_EQ(vs.at(2).as<bool>(), true);
    
    vs.at(0) = 456;
    vs.at(1) = "world";
    vs.at(2) = false;
    
    EXPECT_EQ(vs.at(0).as<int>(), 456);
    EXPECT_EQ(vs.at(1).as<std::string>(), "world");
    EXPECT_EQ(vs.at(2).as<bool>(), false);
    
    EXPECT_THROW(vs.at(3), std::out_of_range);
}

// 测试 variants 的 front 方法
TEST(VariantsOperationsTest, Front) {
    variants vs = {123, "hello", true};
    
    EXPECT_EQ(vs.front().as<int>(), 123);
    
    vs.front() = 456;
    
    EXPECT_EQ(vs.front().as<int>(), 456);
}

// 测试 variants 的 back 方法
TEST(VariantsOperationsTest, Back) {
    variants vs = {123, "hello", true};
    
    EXPECT_EQ(vs.back().as<bool>(), true);
    
    vs.back() = false;
    
    EXPECT_EQ(vs.back().as<bool>(), false);
}

// 测试 variants 的 size 方法
TEST(VariantsOperationsTest, Size) {
    variants vs = {123, "hello", true};
    
    EXPECT_EQ(vs.size(), 3);
    
    vs.push_back(123.456);
    
    EXPECT_EQ(vs.size(), 4);
}

// 测试 variants 的 empty 方法
TEST(VariantsOperationsTest, Empty) {
    variants vs1;
    variants vs2 = {123, "hello", true};
    
    EXPECT_TRUE(vs1.empty());
    EXPECT_FALSE(vs2.empty());
}

// 测试 variants 的 clear 方法
TEST(VariantsOperationsTest, Clear) {
    variants vs = {123, "hello", true};
    
    EXPECT_EQ(vs.size(), 3);
    
    vs.clear();
    
    EXPECT_EQ(vs.size(), 0);
    EXPECT_TRUE(vs.empty());
}

// 测试 variants 的 erase 方法
TEST(VariantsOperationsTest, Erase) {
    variants vs = {123, "hello", true};
    
    EXPECT_EQ(vs.size(), 3);
    
    vs.erase(1);
    
    EXPECT_EQ(vs.size(), 2);
    EXPECT_EQ(vs[0].as<int>(), 123);
    EXPECT_EQ(vs[1].as<bool>(), true);
}

// 测试 variants 的 insert 方法
TEST(VariantsOperationsTest, Insert) {
    variants vs = {123, "hello", true};
    
    EXPECT_EQ(vs.size(), 3);
    
    vs.insert(1, 123.456);
    
    EXPECT_EQ(vs.size(), 4);
    EXPECT_EQ(vs[0].as<int>(), 123);
    EXPECT_DOUBLE_EQ(vs[1].as<double>(), 123.456);
    EXPECT_EQ(vs[2].as<std::string>(), "hello");
    EXPECT_EQ(vs[3].as<bool>(), true);
}

// 测试 variants 的 operator== 方法
TEST(VariantsOperationsTest, OperatorEqual) {
    variants vs1 = {123, "hello", true};
    variants vs2 = {123, "hello", true};
    variants vs3 = {123, "world", true};
    
    EXPECT_TRUE(vs1 == vs2);
    EXPECT_FALSE(vs1 == vs3);
}

// 测试 variants 的 operator!= 方法
TEST(VariantsOperationsTest, OperatorNotEqual) {
    variants vs1 = {123, "hello", true};
    variants vs2 = {123, "hello", true};
    variants vs3 = {123, "world", true};
    
    EXPECT_FALSE(vs1 != vs2);
    EXPECT_TRUE(vs1 != vs3);
} 