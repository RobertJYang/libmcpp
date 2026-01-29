/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <mc/validate/validator.h>
#include <string>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace mc {
namespace validate {
namespace lua {

// 辅助函数：将 C++ 异常转换为 lua 错误
static int handle_validation_exception(lua_State* L, const validation_exception& e) {
    return luaL_error(L, "%s", e.what());
}

// check_integer(name, val, min, max, need_convert)
static int l_check_integer(lua_State* L) {
    // 参数顺序：name val min max need_convert
    const int name_idx         = 1;
    const int val_idx          = 2;
    const int min_idx          = 3;
    const int max_idx          = 4;
    const int need_convert_idx = 5;

    // 检查类型是否为数字
    if (lua_type(L, val_idx) != LUA_TNUMBER) {
        const char* name         = luaL_checkstring(L, name_idx);
        int         need_convert = lua_toboolean(L, need_convert_idx);
        luaL_tolstring(L, val_idx, nullptr);
        const char* val_str = lua_tostring(L, -1);
        auto [final_name, final_val] =
            validator::format_name_and_value(name, val_str ? val_str : "", need_convert);
        return luaL_error(L,
                          "PropertyValueTypeError: The value %s for the property %s is of a different type than the "
                          "property can accept.",
                          final_val.c_str(), final_name.c_str());
    }

    // 检查min max是否为数字类型
    if (lua_type(L, min_idx) != LUA_TNUMBER || lua_type(L, max_idx) != LUA_TNUMBER) {
        const char* name         = luaL_checkstring(L, name_idx);
        int         need_convert = lua_toboolean(L, need_convert_idx);
        luaL_tolstring(L, val_idx, nullptr);
        const char* val_str = lua_tostring(L, -1);
        auto [final_name, final_val] =
            validator::format_name_and_value(name, val_str ? val_str : "", need_convert);
        return luaL_error(L,
                          "PropertyValueOutOfRange: The value '%s' for the property %s is not in the supported range "
                          "of acceptable values.",
                          final_val.c_str(), final_name.c_str());
    }

    try {
        const char* name         = luaL_checkstring(L, name_idx);
        lua_Number  val          = lua_tonumber(L, val_idx);
        lua_Number  min          = lua_tonumber(L, min_idx);
        lua_Number  max          = lua_tonumber(L, max_idx);
        int         need_convert = lua_toboolean(L, need_convert_idx);

        validator::check_integer(name, val, min, max, need_convert != 0);
        return 0;
    } catch (const property_value_type_error& e) {
        return handle_validation_exception(L, e);
    } catch (const property_value_out_of_range& e) {
        return handle_validation_exception(L, e);
    }
}

// 公共数值范围校验实现
static int ranges_impl(lua_State* L, bool allow_nil) {
    const int name_idx         = 1;
    const int val_idx          = 2;
    const int min_idx          = 3;
    const int max_idx          = 4;
    const int need_convert_idx = 5;

    if (allow_nil && lua_isnil(L, val_idx)) {
        return 0; // 允许为 nil，跳过检查
    }

    // 检查类型是否为数字
    if (lua_type(L, val_idx) != LUA_TNUMBER) {
        const char* name         = luaL_checkstring(L, name_idx);
        int         need_convert = lua_toboolean(L, need_convert_idx);
        luaL_tolstring(L, val_idx, nullptr);
        const char* val_str = lua_tostring(L, -1);
        auto [final_name, final_val] =
            validator::format_name_and_value(name, val_str ? val_str : "", need_convert);
        return luaL_error(L,
                          "PropertyValueTypeError: The value %s for the property %s is of a different type than the "
                          "property can accept.",
                          final_val.c_str(), final_name.c_str());
    }

    // 检查min max是否为数字类型
    if (lua_type(L, min_idx) != LUA_TNUMBER || lua_type(L, max_idx) != LUA_TNUMBER) {
        const char* name         = luaL_checkstring(L, name_idx);
        int         need_convert = lua_toboolean(L, need_convert_idx);
        luaL_tolstring(L, val_idx, nullptr);
        const char* val_str = lua_tostring(L, -1);
        auto [final_name, final_val] =
            validator::format_name_and_value(name, val_str ? val_str : "", need_convert);
        return luaL_error(L,
                          "PropertyValueOutOfRange: The value '%s' for the property %s is not in the supported range "
                          "of acceptable values.",
                          final_val.c_str(), final_name.c_str());
    }

    try {
        const char* name         = luaL_checkstring(L, name_idx);
        lua_Number  val          = lua_tonumber(L, val_idx);
        lua_Number  min          = lua_tonumber(L, min_idx);
        lua_Number  max          = lua_tonumber(L, max_idx);
        int         need_convert = lua_toboolean(L, need_convert_idx);

        validator::ranges(name, val, min, max, need_convert != 0, allow_nil);
        return 0;
    } catch (const property_value_type_error& e) {
        return handle_validation_exception(L, e);
    } catch (const property_value_out_of_range& e) {
        return handle_validation_exception(L, e);
    }
}

// ranges(name, val, min, max, need_convert)
static int l_ranges(lua_State* L) {
    return ranges_impl(L, false);
}

// range_or_none(name, val, min, max, need_convert)
static int l_range_or_none(lua_State* L) {
    return ranges_impl(L, true);
}

// 公共字符串长度校验实现
static int lens_impl(lua_State* L, bool allow_nil) {
    const int name_idx         = 1;
    const int val_idx          = 2;
    const int min_idx          = 3;
    const int max_idx          = 4;
    const int need_convert_idx = 5;

    if (allow_nil && lua_isnil(L, val_idx)) {
        return 0; // 允许为 nil，跳过长度检查
    }

    // 检查类型是否为字符串
    if (lua_type(L, val_idx) != LUA_TSTRING) {
        const char* name         = luaL_checkstring(L, name_idx);
        int         need_convert = lua_toboolean(L, need_convert_idx);
        luaL_tolstring(L, val_idx, nullptr);
        const char* val_str = lua_tostring(L, -1);
        auto [final_name, final_val] =
            validator::format_name_and_value(name, val_str ? val_str : "", need_convert);
        return luaL_error(L,
                          "PropertyValueTypeError: The value %s for the property %s is of a different type than the "
                          "property can accept.",
                          final_val.c_str(), final_name.c_str());
    }

    // 检查min max是否为数字类型
    if (lua_type(L, min_idx) != LUA_TNUMBER || lua_type(L, max_idx) != LUA_TNUMBER) {
        const char* name         = luaL_checkstring(L, name_idx);
        int         need_convert = lua_toboolean(L, need_convert_idx);
        luaL_tolstring(L, val_idx, nullptr);
        const char* val_str = lua_tostring(L, -1);
        auto [final_name, final_val] =
            validator::format_name_and_value(name, val_str ? val_str : "", need_convert);
        return luaL_error(L,
                          "PropertyValueOutOfRange: The value '%s' for the property %s is not in the supported range "
                          "of acceptable values.",
                          final_val.c_str(), final_name.c_str());
    }

    try {
        const char* name         = luaL_checkstring(L, name_idx);
        size_t      val_len      = 0;
        const char* val_str      = lua_tolstring(L, val_idx, &val_len);
        lua_Number  min          = lua_tonumber(L, min_idx);
        lua_Number  max          = lua_tonumber(L, max_idx);
        int         need_convert = lua_toboolean(L, need_convert_idx);

        validator::lens(name, std::string_view(val_str, val_len), static_cast<int>(min), static_cast<int>(max),
                        need_convert != 0, allow_nil);
        return 0;
    } catch (const property_value_type_error& e) {
        return handle_validation_exception(L, e);
    } catch (const property_value_out_of_range& e) {
        return handle_validation_exception(L, e);
    } catch (const string_length_error& e) {
        return handle_validation_exception(L, e);
    }
}

// lens(name, val, min, max, need_convert)
static int l_lens(lua_State* L) {
    return lens_impl(L, false);
}

// len_or_none(name, val, min, max, need_convert)
static int l_len_or_none(lua_State* L) {
    return lens_impl(L, true);
}

// 公共正则校验实现
static int regex_impl(lua_State* L, bool allow_nil) {
    const int name_idx         = 1;
    const int val_idx          = 2;
    const int pattern_idx      = 3;
    const int need_convert_idx = 4;

    if (allow_nil && lua_isnil(L, val_idx)) {
        return 0; // 允许为 nil
    }

    // 检查类型是否为字符串
    if (lua_type(L, val_idx) != LUA_TSTRING) {
        const char* name         = luaL_checkstring(L, name_idx);
        int         need_convert = lua_toboolean(L, need_convert_idx);
        luaL_tolstring(L, val_idx, nullptr);
        const char* val_str = lua_tostring(L, -1);
        auto [final_name, final_val] =
            validator::format_name_and_value(name, val_str ? val_str : "", need_convert);
        return luaL_error(L,
                          "PropertyValueTypeError: The value %s for the property %s is of a different type than the "
                          "property can accept.",
                          final_val.c_str(), final_name.c_str());
    }

    try {
        const char* name         = luaL_checkstring(L, name_idx);
        size_t      val_len      = 0;
        const char* val_str      = lua_tolstring(L, val_idx, &val_len);
        const char* pattern      = luaL_checkstring(L, pattern_idx);
        int         need_convert = lua_toboolean(L, need_convert_idx);

        validator::regex(name, std::string_view(val_str, val_len), pattern, need_convert != 0, allow_nil);
        return 0;
    } catch (const property_value_type_error& e) {
        return handle_validation_exception(L, e);
    } catch (const format_error& e) {
        return handle_validation_exception(L, e);
    }
}

// regex(name, val, pattern, need_convert)
static int l_regex(lua_State* L) {
    return regex_impl(L, false);
}

// regex_or_none(name, val, pattern, need_convert)
static int l_regex_or_none(lua_State* L) {
    return regex_impl(L, true);
}

// Json(val)
static int l_json(lua_State* L) {
    const int val_idx = 1;

    if (lua_type(L, val_idx) != LUA_TSTRING) {
        return luaL_error(L, "MalformedJSON: The request body submitted was malformed JSON and could not be parsed by "
                             "the receiving service.");
    }

    try {
        size_t      len     = 0;
        const char* jsonstr = lua_tolstring(L, val_idx, &len);
        validator::json(std::string_view(jsonstr, len));
        return 0;
    } catch (const json_error& e) {
        return handle_validation_exception(L, e);
    }
}

static const luaL_Reg methods[] = {{"check_integer", l_check_integer},
                                   {"ranges", l_ranges},
                                   {"range_or_none", l_range_or_none},
                                   {"lens", l_lens},
                                   {"len_or_none", l_len_or_none},
                                   {"regex", l_regex},
                                   {"regex_or_none", l_regex_or_none},
                                   {"Json", l_json},
                                   {nullptr, nullptr}};

} // namespace lua
} // namespace validate
} // namespace mc

extern "C" {

__attribute__((visibility("default"))) int luaopen_lvalidate(lua_State* L) {
    luaL_checkversion(L);
    luaL_newlib(L, mc::validate::lua::methods);
    return 1;
}
}