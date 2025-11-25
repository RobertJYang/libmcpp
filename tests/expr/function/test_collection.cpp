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

#include "mc/expr/function/call.h"
#include "mc/expr/function/collection.h"
#include <gtest/gtest.h>

using namespace mc::expr;

namespace {

class FuncCollectionFixture : public ::testing::Test {
protected:
    void SetUp() override {
        func_collection::get_instance().clear();
    }

    void TearDown() override {
        func_collection::get_instance().clear();
    }
};

func make_simple_func(const std::string& expression) {
    mc::dict args = {
        {"param", mc::variant(42)}};
    return func(expression, args);
}

TEST_F(FuncCollectionFixture, AddGetContainsAndSize) {
    mc::dict functions;
    functions.insert("$Func_Sample", mc::variant(make_simple_func("param")));

    func_collection::get_instance().add("pos1", nullptr, functions);

    EXPECT_TRUE(func_collection::get_instance().contains("pos1"));
    EXPECT_EQ(func_collection::get_instance().size(), 1U);
    EXPECT_FALSE(func_collection::get_instance().empty());

    auto stored = func_collection::get_instance().get("pos1", "$Func_Sample");
    EXPECT_FALSE(stored.is_null());
    EXPECT_EQ(stored.as<func>().get_args().size(), 1U);

    auto missing = func_collection::get_instance().get("pos1", "$Func_Unknown");
    EXPECT_TRUE(missing.is_null());
}

TEST_F(FuncCollectionFixture, RemoveClearsFunctionsAndServices) {
    mc::dict functions;
    functions.insert("$Func_Remove", mc::variant(make_simple_func("param")));
    func_collection::get_instance().add("pos-remove", nullptr, functions);

    auto removed = func_collection::get_instance().remove("pos-remove");
    EXPECT_EQ(removed.size(), 1U);
    EXPECT_FALSE(removed["$Func_Remove"].is_null());

    EXPECT_FALSE(func_collection::get_instance().contains("pos-remove"));
    EXPECT_EQ(func_collection::get_instance().size(), 0U);
    EXPECT_EQ(func_collection::get_instance().get_service("pos-remove"), nullptr);

    auto remove_missing = func_collection::get_instance().remove("not-exist");
    EXPECT_TRUE(remove_missing.empty());
}

TEST_F(FuncCollectionFixture, GetDictionaryByPositionReturnsCopy) {
    auto empty_dict = func_collection::get_instance().get("unknown");
    EXPECT_TRUE(empty_dict.empty());

    mc::dict functions;
    functions.insert("$Func_Get", mc::variant(make_simple_func("param")));
    func_collection::get_instance().add("pos-get", nullptr, functions);

    auto stored_dict = func_collection::get_instance().get("pos-get");
    EXPECT_EQ(stored_dict.size(), 1U);
    EXPECT_FALSE(stored_dict["$Func_Get"].is_null());
}

TEST_F(FuncCollectionFixture, EmptyReflectsInternalState) {
    EXPECT_TRUE(func_collection::get_instance().empty());

    mc::dict functions;
    functions.insert("$Func_A", mc::variant(make_simple_func("param")));
    func_collection::get_instance().add("pos-empty", nullptr, functions);

    EXPECT_FALSE(func_collection::get_instance().empty());

    func_collection::get_instance().clear();
    EXPECT_TRUE(func_collection::get_instance().empty());
    EXPECT_EQ(func_collection::get_instance().size(), 0U);
}

} // namespace

