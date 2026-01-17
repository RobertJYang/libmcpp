#include "mc/validate.h"
#include <gtest/gtest.h>
#include <string>

class ValidatorTest : public ::testing::Test {
protected:
    mc::validate::Validator validator;
};

TEST(ValidatorTest, ComprehensiveIntegerValidation) {
    mc::validate::Validator validator;
    
    // 1. 整数转整数 - 左值，不触发异常
    {
        int32_t src_int32 = 100;
        int32_t dst_int32 = src_int32;
        EXPECT_NO_THROW(validator.check<int32_t>(dst_int32));
    }
    
    // 2. 整数转整数 - 右值，不触发异常
    EXPECT_NO_THROW(validator.check<int32_t>(200));
    
    // 4. 整数转整数 - 触发范围错误（超出范围）
    {
        int64_t large_value = 9223372036854775807LL; // LLONG_MAX
        EXPECT_THROW(validator.check<int32_t>(large_value), mc::validate::RangeError);
    }
    
    // 5. 浮点数转整数 - 触发类型错误
    {
        float float_value = 123.45f;
        EXPECT_THROW(validator.check<int32_t>(float_value), mc::validate::TypeError);
    }
    
    // 6. 对象转整数 - 触发类型错误
    {
        std::string str_value = "123";
        EXPECT_THROW(validator.check<int32_t>(str_value), mc::validate::TypeError);
    }
    
    // 7. 无符号到有符号 - 范围正常
    {
        uint16_t uint16_value = 32767;
        EXPECT_NO_THROW(validator.check<int32_t>(uint16_value));
    }
    
    // 8. 有符号到无符号 - 范围正常（正值）
    {
        int32_t positive_int = 100;
        EXPECT_NO_THROW(validator.check<uint32_t>(positive_int));
    }
    
    // 9. 有符号到无符号 - 触发范围错误（负值）
    {
        int32_t negative_int = -100;
        EXPECT_THROW(validator.check<uint32_t>(negative_int), mc::validate::RangeError);
    }
    
    // 10. 大范围整数到小范围整数 - 触发范围错误
    {
        int64_t large_int64 = 9223372036854775807LL; // LLONG_MAX
        EXPECT_THROW(validator.check<int16_t>(large_int64), mc::validate::RangeError);
    }
    
    // 11. 同类型边界值检查 - 最小值
    {
        int32_t min_int32 = std::numeric_limits<int32_t>::min();
        EXPECT_NO_THROW(validator.check<int32_t>(min_int32));
    }
    
    // 12. 同类型边界值检查 - 最大值
    {
        int32_t max_int32 = std::numeric_limits<int32_t>::max();
        EXPECT_NO_THROW(validator.check<int32_t>(max_int32));
    }

    {
    // 13. 不支持的校验类型，抛出InternalError
        EXPECT_THROW(validator.check<float>(100), mc::validate::InternalError);
    }
}