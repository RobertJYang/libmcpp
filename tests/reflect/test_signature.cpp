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

#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/reflect/signature.h>
#include <mc/variant.h>

using mc::reflect::signature;
using mc::reflect::signature_iterator;

TEST(signature_test, construct_and_to_string)
{
    signature sig("(isd)");
    EXPECT_EQ(sig.str(), "(isd)");

    signature sig2("(ay)");
    EXPECT_EQ(sig2.str(), "(ay)");
    EXPECT_FALSE(sig2.is_empty());
}

TEST(signature_test, invalid_signature_handling)
{
    signature invalid("(");
    EXPECT_FALSE(invalid.is_valid());
    EXPECT_THROW(signature::validate("("), mc::invalid_arg_exception);

    signature assigned;
    assigned = std::string_view("(i");
    EXPECT_FALSE(assigned.is_valid());
    EXPECT_THROW(signature::validate(assigned.str()), mc::invalid_arg_exception);

    EXPECT_THROW(signature_iterator("(i"), mc::invalid_arg_exception);
}

TEST(signature_test, get_complete_types_and_iteration)
{
    signature sig("a{si}");
    auto      types = sig.get_complete_types();
    ASSERT_EQ(types.size(), 1U);
    EXPECT_EQ(types[0].str(), "a{si}");

    signature_iterator array_it(sig);
    ASSERT_TRUE(array_it.is_valid());
    EXPECT_TRUE(array_it.is_container());
    auto entry_it = array_it.get_content_iterator();
    EXPECT_TRUE(entry_it.is_container());
    EXPECT_EQ(entry_it.current_type(), "{si}");
    auto key_it = entry_it.get_dict_key_iterator();
    auto val_it = entry_it.get_dict_value_iterator();
    EXPECT_EQ(key_it.current_type_char(), 's');
    EXPECT_EQ(val_it.current_type_char(), 'i');
}

TEST(signature_test, static_checks)
{
    EXPECT_TRUE(signature::is_basic_type('s'));
    EXPECT_FALSE(signature::is_basic_type('('));
    EXPECT_TRUE(signature::is_container_type('a'));
    EXPECT_FALSE(signature::is_container_type('i'));

    EXPECT_TRUE(signature::is_single_complete_type("s"));
    EXPECT_FALSE(signature::is_single_complete_type("si"));

    EXPECT_EQ(signature::get_complete_type_length("(is)", 0), 4U);
    EXPECT_TRUE(signature::is_valid("(is)"));
    EXPECT_FALSE(signature::is_valid("(i"));
}

TEST(signature_test, first_type_empty_string)
{
    std::string empty_sig;
    char        first = mc::reflect::first_type(empty_sig);
    EXPECT_EQ(first, '\0');
}

TEST(signature_test, assign_string_view)
{
    signature        sig("(is)");
    std::string_view sv("(isd)");
    sig = sv;
    EXPECT_EQ(sig.str(), "(isd)");
}

TEST(signature_test, assign_string)
{
    signature   sig("(is)");
    std::string str("(isb)");
    sig = str;
    EXPECT_EQ(sig.str(), "(isb)");
}

TEST(signature_test, from_variant)
{
    mc::variant var("(isb)");
    signature   sig;
    mc::from_variant(var, sig);
    EXPECT_EQ(sig.str(), "(isb)");
}
