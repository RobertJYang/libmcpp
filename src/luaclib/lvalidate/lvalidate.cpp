#include <string>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace mc {
namespace validate {
namespace lua {

static int raise_type_error(lua_State* L) {    
    // 参数位置: 1=name, 2=val, 3=min, 4=max, 5=need_convert
    const char* name = luaL_checkstring(L, 1);
    int need_convert = lua_toboolean(L, 5);
    
    // 获取val的字符串表示
    luaL_tolstring(L, 2, NULL);  // 将val转换为字符串并压入栈顶
    const char* val_str = lua_tostring(L, -1);
    
    std::string final_name = name;
    std::string final_val = val_str;
    
    // 如果need_convert为真，格式化val和name
    if (need_convert) {
        // 格式化name: %Prop1
        final_name = "%" + std::string(name);
        
        // 格式化val: %Prop1:129
        final_val = "%" + std::string(name) + ":" + val_str;
    }
    
    // 构建错误信息
    std::string error_msg = "PropertyValueTypeError: The value '" + 
                           final_val + "' for the property " + 
                           final_name + " is of a different type than the property can accept.";
    
    // 抛出异常
    return luaL_error(L, "%s", error_msg.c_str());
}

static int raise_range_error(lua_State* L) {   
    // 参数位置: 1=name, 2=val, 3=min, 4=max, 5=need_convert
    const char* name = luaL_checkstring(L, 1);
    int need_convert = lua_toboolean(L, 5);
    
    // 获取val的字符串表示
    luaL_tolstring(L, 2, NULL);  // 将val转换为字符串并压入栈顶
    const char* val_str = lua_tostring(L, -1);
    
    std::string final_name = name;
    std::string final_val = val_str;

    if (need_convert) {
        final_name = "%" + std::string(name);
        final_val = "%" + std::string(name) + ":" + val_str;
    }

    // 构建错误信息
    std::string error_msg = "PropertyValueOutOfRange: The value '" + 
                           final_val + "' for the property " + 
                           final_name + " is not in the supported range of acceptable values.";
    
    // 抛出异常
    return luaL_error(L, "%s", error_msg.c_str());
}

static int l_check_integer(lua_State* L) {    
    // 参数顺序：name val min max need_convert
    // 先检查类型是否为数字
    if (lua_type(L, 2) != LUA_TNUMBER) {
        raise_type_error(L);
    }
    
    // 如果是数字，检查是否为整数
    if (!lua_isinteger(L, 2)) {
        // 是数字但不是整数类型，检查是否有小数部分
        lua_Number val_num = lua_tonumber(L, 2);
        lua_Integer val_int = (lua_Integer)val_num;
        
        // 检查是否是整数（排除NaN）
        if (val_num != val_num || val_num != (lua_Number)val_int) {
            raise_type_error(L);
        }
    }

    // 检查min max是否为数字类型
    if (lua_type(L, 3) != LUA_TNUMBER || 
        lua_type(L, 4) != LUA_TNUMBER) {
        raise_range_error(L);
    }
    
    // 获取参数值
    lua_Number val = lua_tonumber(L, 2);
    lua_Number min = lua_tonumber(L, 3);
    lua_Number max = lua_tonumber(L, 4);
    
    // 检查val是否在[min, max]范围内
    if (val < min || val > max) { 
        raise_range_error(L);
    }

    return 0;  // 成功，返回0个值
}

static const luaL_Reg methods[] = {
    {"check_integer", l_check_integer},
    {nullptr, nullptr}
};

} // namespace lua
} // namespace validate
} // namespace mc

extern "C" {

__attribute__((visibility("default")))
int luaopen_lvalidate(lua_State* L) {
    luaL_checkversion(L);
    luaL_newlib(L, mc::validate::lua::methods);
    return 1;
}

}