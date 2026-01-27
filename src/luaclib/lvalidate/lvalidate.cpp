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
#include <cctype>
#include <glib.h>
#include <mc/json.h>
#include <mc/variant.h>
#include <string>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace mc {
namespace validate {
namespace lua {

static int raise_property_value_type_error(lua_State* L, int name_idx, int val_idx, int need_convert_idx) {
    const char* name         = luaL_checkstring(L, name_idx);
    int         need_convert = lua_toboolean(L, need_convert_idx);

    luaL_tolstring(L, val_idx, nullptr); // 将 val 转为字符串压栈
    const char* val_str = lua_tostring(L, -1);

    std::string final_name = name;
    std::string final_val  = val_str ? val_str : "";

    if (need_convert) {
        final_name = "%" + std::string(name);
        final_val  = "%" + std::string(name) + ":" + final_val;
    }

    return luaL_error(
        L,
        "PropertyValueTypeError: The value %s for the property %s is of a different type than the property can accept.",
        final_val.c_str(), final_name.c_str());
}

static int raise_property_value_out_of_range(lua_State* L, int name_idx, int val_idx, int need_convert_idx) {
    const char* name         = luaL_checkstring(L, name_idx);
    int         need_convert = lua_toboolean(L, need_convert_idx);

    luaL_tolstring(L, val_idx, nullptr);
    const char* val_str = lua_tostring(L, -1);

    std::string final_name = name;
    std::string final_val  = val_str ? val_str : "";

    if (need_convert) {
        final_name = "%" + std::string(name);
        final_val  = "%" + std::string(name) + ":" + final_val;
    }

    return luaL_error(L,
                      "PropertyValueOutOfRange: The value '%s' for the property %s is not in the supported range of "
                      "acceptable values.",
                      final_val.c_str(), final_name.c_str());
}

static int l_check_integer(lua_State* L) {
    // 参数顺序：name val min max need_convert
    const int name_idx         = 1;
    const int val_idx          = 2;
    const int min_idx          = 3;
    const int max_idx          = 4;
    const int need_convert_idx = 5;
    // 先检查类型是否为数字
    if (lua_type(L, val_idx) != LUA_TNUMBER) {
        raise_property_value_type_error(L, name_idx, val_idx, need_convert_idx);
    }

    // 如果是数字，检查是否为整数
    if (!lua_isinteger(L, val_idx)) {
        // 是数字但不是整数类型，检查是否有小数部分
        lua_Number  val_num = lua_tonumber(L, val_idx);
        lua_Integer val_int = (lua_Integer)val_num;

        // 检查是否是整数（排除NaN）
        if (val_num != val_num || val_num != (lua_Number)val_int) {
            raise_property_value_type_error(L, name_idx, val_idx, need_convert_idx);
        }
    }

    // 检查min max是否为数字类型
    if (lua_type(L, 3) != LUA_TNUMBER || lua_type(L, 4) != LUA_TNUMBER) {
        raise_property_value_out_of_range(L, name_idx, val_idx, need_convert_idx);
    }

    // 获取参数值
    lua_Number val = lua_tonumber(L, val_idx);
    lua_Number min = lua_tonumber(L, min_idx);
    lua_Number max = lua_tonumber(L, max_idx);

    // 检查val是否在[min, max]范围内
    if (val < min || val > max) {
        raise_property_value_out_of_range(L, name_idx, val_idx, need_convert_idx);
    }

    return 0; // 成功，返回0个值
}

// 辅助函数：构造字符串过短/过长错误
static int raise_string_length_error(lua_State* L, int name_idx, int val_idx, int need_convert_idx, int threshold,
                                     bool too_short) {
    const char* name         = luaL_checkstring(L, name_idx);
    int         need_convert = lua_toboolean(L, need_convert_idx);

    luaL_tolstring(L, val_idx, nullptr);
    const char* val_str = lua_tostring(L, -1);

    std::string final_name = name;
    std::string final_val  = val_str ? val_str : "";

    if (need_convert) {
        final_name = "%" + std::string(name);
        final_val  = "%" + std::string(name) + ":" + final_val;
    }
    if (too_short) {
        return luaL_error(L, "StringValueTooShort: The string '%s' was under the minimum required length %d.",
                          final_val.c_str(), threshold);
    } else {
        return luaL_error(L, "StringValueTooLong: The string '%s' exceeds the length limit %d.", final_val.c_str(),
                          threshold);
    }
}

// 辅助函数：构造格式错误
static int raise_format_error(lua_State* L, int name_idx, int val_idx, int need_convert_idx) {
    const char* name         = luaL_checkstring(L, name_idx);
    int         need_convert = lua_toboolean(L, need_convert_idx);

    luaL_tolstring(L, val_idx, nullptr);
    const char* val_str = lua_tostring(L, -1);

    std::string final_name = name;
    std::string final_val  = val_str ? val_str : "";

    if (need_convert) {
        final_name = "%" + std::string(name);
        final_val  = "%" + std::string(name) + ":" + final_val;
    }

    return luaL_error(L,
                      "PropertyValueFormatError: The value %s for the property %s is of a different format than the "
                      "property can accept.",
                      final_val.c_str(), final_name.c_str());
}

// 辅助函数：构造 JSON 错误
static int raise_json_error(lua_State* L, int name_idx, int val_idx, int need_convert_idx) {
    const char* name         = luaL_checkstring(L, name_idx);
    int         need_convert = lua_toboolean(L, need_convert_idx);

    luaL_tolstring(L, val_idx, nullptr);
    const char* val_str = lua_tostring(L, -1);

    std::string final_name = name;
    std::string final_val  = val_str ? val_str : "";

    if (need_convert) {
        final_name = "%" + std::string(name);
        final_val  = "%" + std::string(name) + ":" + final_val;
    }

    return luaL_error(L, "MalformedJSON: The request body submitted was malformed JSON and could not be parsed by the "
                         "receiving service.");
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

    if (lua_type(L, val_idx) != LUA_TNUMBER) {
        return raise_property_value_type_error(L, name_idx, val_idx, need_convert_idx);
    }

    if (lua_type(L, min_idx) != LUA_TNUMBER || lua_type(L, max_idx) != LUA_TNUMBER) {
        return raise_property_value_out_of_range(L, name_idx, val_idx, need_convert_idx);
    }

    lua_Number val = lua_tonumber(L, val_idx);
    lua_Number min = lua_tonumber(L, min_idx);
    lua_Number max = lua_tonumber(L, max_idx);

    if (val < min || val > max) {
        return raise_property_value_out_of_range(L, name_idx, val_idx, need_convert_idx);
    }

    return 0;
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

    if (lua_type(L, val_idx) != LUA_TSTRING) {
        return raise_property_value_type_error(L, name_idx, val_idx, need_convert_idx);
    }

    if (lua_type(L, min_idx) != LUA_TNUMBER || lua_type(L, max_idx) != LUA_TNUMBER) {
        return raise_property_value_out_of_range(L, name_idx, val_idx, need_convert_idx);
    }

    size_t     len = lua_rawlen(L, val_idx);
    lua_Number min = lua_tonumber(L, min_idx);
    lua_Number max = lua_tonumber(L, max_idx);

    if (len < static_cast<size_t>(min)) {
        return raise_string_length_error(L, name_idx, val_idx, need_convert_idx, static_cast<int>(min), true);
    }
    if (len > static_cast<size_t>(max)) {
        return raise_string_length_error(L, name_idx, val_idx, need_convert_idx, static_cast<int>(max), false);
    }

    return 0;
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

    if (lua_type(L, val_idx) != LUA_TSTRING) {
        return raise_property_value_type_error(L, name_idx, val_idx, need_convert_idx);
    }

    luaL_checkstring(L, pattern_idx);

    // 获取待匹配的字符串和正则模式
    size_t      val_len = 0;
    const char* val_str = lua_tolstring(L, val_idx, &val_len);
    const char* pattern = lua_tostring(L, pattern_idx);
    GError*     error   = nullptr;

    // 编译标准正则表达式
    GRegex* regex = g_regex_new(pattern, static_cast<GRegexCompileFlags>(0), static_cast<GRegexMatchFlags>(0), &error);
    if (regex == nullptr) {
        if (error) {
            g_error_free(error);
        }
        return raise_format_error(L, name_idx, val_idx, need_convert_idx);
    }

    gboolean matched = g_regex_match(regex, val_str, static_cast<GRegexMatchFlags>(0), nullptr);
    g_regex_unref(regex);

    if (!matched) {
        return raise_format_error(L, name_idx, val_idx, need_convert_idx);
    }

    return 0;
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
    const int val_idx  = 1;
    bool      is_valid = false;
    if (lua_type(L, val_idx) == LUA_TSTRING) {
        size_t      len     = 0;
        const char* jsonstr = lua_tolstring(L, val_idx, &len);
        std::string s(jsonstr ? jsonstr : "", len);
        try {
            mc::json::json_decode(s);
            is_valid = true;
        } catch (...) {
            is_valid = false;
        }
    }
    if (!is_valid) {
        return luaL_error(L, "MalformedJSON: The request body submitted was malformed JSON and could not be parsed by "
                             "the receiving service.");
    }
    return 0;
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