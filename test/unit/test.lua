-- 确保 D-Bus 环境存在
local dbus_address_file = "./test/.dbus"
local dbus_daemon_started = false

if not os.getenv("DBUS_SESSION_BUS_ADDRESS") then
    print("未检测到 D-Bus session，启动临时 D-Bus daemon...")

    -- 启动 dbus-daemon 并获取地址
    local handle = io.popen("dbus-daemon --session --fork --print-address 2>&1")
    if not handle then
        print("错误: 无法启动 dbus-daemon")
        os.exit(1)
    end

    local dbus_address = handle:read("*line")
    handle:close()

    if not dbus_address or dbus_address == "" or dbus_address:match("error") then
        print("错误: 无法获取 D-Bus 地址: " .. (dbus_address or ""))
        os.exit(1)
    end

    -- 将地址写入 test/.dbus 文件
    os.execute("mkdir -p test")
    local file = io.open(dbus_address_file, "w")
    if file then
        file:write(dbus_address)
        file:close()
        print("D-Bus daemon 已启动，地址已写入: " .. dbus_address_file)
        print("D-Bus 地址: " .. dbus_address)
        dbus_daemon_started = true
    else
        print("错误: 无法写入 D-Bus 地址文件")
        os.exit(1)
    end
end

package.path = './temp/opt/bmc/lualib/?.lua;' .. package.path

-- 动态设置 LD_LIBRARY_PATH，确保能够加载库文件
local function setup_library_path()
    -- 获取当前工作目录
    local pwd_handle = io.popen("pwd")
    local current_dir = pwd_handle:read("*line")
    pwd_handle:close()

    local lib_dirs = {}

    -- 查找 libmcpp.so 的位置（使用绝对路径）
    -- 同时搜索 temp（bingo test）和 builddir（直接运行）
    local search_cmd = string.format(
                           "find %s/temp/root/.conan2/p/b %s/builddir -name 'libmcpp.so' -type f 2>/dev/null | head -1",
                           current_dir, current_dir)
    local handle = io.popen(search_cmd)
    if handle then
        local lib_path = handle:read("*line")
        handle:close()
        if lib_path then
            local lib_dir = lib_path:match("(.*/)")
            table.insert(lib_dirs, lib_dir)
        end
    end

    -- 添加其他可能的库目录（使用绝对路径）
    table.insert(lib_dirs, current_dir .. "/temp/usr/lib64")
    table.insert(lib_dirs, current_dir .. "/builddir/usr/lib64")

    -- 查找所有 conan 构建目录中的 lib64 目录
    local conan_lib_search = string.format(
                                 "find %s/temp/root/.conan2/p/b -type d -name lib64 2>/dev/null",
                                 current_dir)
    local conan_handle = io.popen(conan_lib_search)
    if conan_handle then
        for line in conan_handle:lines() do
            if line ~= "" then table.insert(lib_dirs, line) end
        end
        conan_handle:close()
    end

    local current_ld_path = os.getenv("LD_LIBRARY_PATH") or ""
    for _, dir in ipairs(lib_dirs) do
        if current_ld_path == "" then
            current_ld_path = dir
        else
            current_ld_path = dir .. ":" .. current_ld_path
        end
    end

    -- 导出环境变量（但 os.execute 创建的进程无法继承这个变量，需要在命令中设置）
    return current_ld_path
end

local lu = require("luaunit")

TestLibmcpp = {}
function TestLibmcpp:setupClass() end

function TestLibmcpp:teardownClass() end
function TestLibmcpp:test_cpp()
    print("\n========== 开始 C++ 单元测试 ==========")
    -- 获取库路径配置
    local new_ld_path = setup_library_path()

    -- 查找测试可执行文件（temp 或 builddir）
    local test_exe = nil
    if io.open("temp/bin/libmcpp_test", "r") then
        test_exe = "temp/bin/libmcpp_test"
    elseif io.open("builddir/tests/libmcpp_test", "r") then
        test_exe = "builddir/tests/libmcpp_test"
    else
        print("警告: 未找到 C++ 测试可执行文件，跳过 C++ 测试")
        return
    end

    -- 设置环境变量并运行 C++ 测试
    local cmd = string.format("LD_LIBRARY_PATH='%s' %s", new_ld_path, test_exe)
    print("运行 C++ 测试: " .. cmd)
    os.execute(cmd)
    print("========== C++ 单元测试完成 ==========\n")
end

function TestLibmcpp:test_lua_dbus()
    print("\n========== 开始 Lua D-Bus 测试 ==========")
    -- 获取 D-Bus 地址（从环境变量或文件）
    local dbus_address = os.getenv("DBUS_SESSION_BUS_ADDRESS")
    if not dbus_address then
        -- 尝试从文件读取
        local file = io.open(dbus_address_file, "r")
        if file then
            dbus_address = file:read("*line")
            file:close()
        end
    end

    if not dbus_address or dbus_address == "" then
        print(
            "警告: 未检测到 D-Bus session 环境，跳过 Lua D-Bus 测试")
        return
    end

    print("运行 Lua D-Bus 测试 (D-Bus 地址: " .. dbus_address .. ")")

    -- 获取当前工作目录
    local pwd_handle = io.popen("pwd")
    local current_dir = pwd_handle:read("*line")
    pwd_handle:close()

    -- 查找 ldbus.so 的位置（同时搜索 temp 和 builddir）
    local ldbus_path = nil
    local search_cmd = string.format(
                           "find %s/temp/root/.conan2/p/b %s/temp/usr/lib64 %s/builddir/usr/lib64 -name 'ldbus.so' -type f 2>/dev/null | head -1",
                           current_dir, current_dir, current_dir)
    print("查找 ldbus.so: " .. search_cmd)
    local handle = io.popen(search_cmd)
    if handle then
        ldbus_path = handle:read("*line")
        handle:close()
    end

    if not ldbus_path or ldbus_path == "" then
        print("警告: 未找到 ldbus.so，跳过 Lua D-Bus 测试")
        return
    end

    print("找到 ldbus.so: " .. ldbus_path)

    -- 验证文件是否真的存在
    local test_file = io.open(ldbus_path, "r")
    if not test_file then
        print("警告: ldbus.so 不存在或无法读取: " .. ldbus_path)
        return
    end
    test_file:close()

    -- 设置路径
    local ldbus_dir = ldbus_path:match("(.*/)")
    local lua_cpath = ldbus_dir .. "?.so;" .. package.cpath
    local lua_path = current_dir .. "/tests/lua/?.lua;" .. current_dir ..
                         "/tests/lua/dbus/?.lua;" .. package.path

    -- 构建 LD_LIBRARY_PATH
    local new_ld_path = setup_library_path()

    -- 创建临时测试脚本（避免引号转义问题）
    local temp_script = current_dir .. "/test/.lua_dbus_test.lua"
    local script = io.open(temp_script, "w")
    if not script then
        print("错误: 无法创建临时测试脚本")
        return
    end

    script:write(string.format([[
package.cpath = '%s'
package.path = '%s'
local lu = require('luaunit')
require('dbus.test_blocking')
require('dbus.test_nonblock')
require('dbus.test_error')
require('dbus.test_message')
require('lvalidate.test_integer')
require('lvalidate.test_validate')
require('json.test_json')
os.exit(lu.LuaUnit.run())
]], lua_cpath, lua_path))
    script:close()

    print("临时脚本已创建: " .. temp_script)
    print("LD_LIBRARY_PATH: " .. new_ld_path)
    print("DBUS_SESSION_BUS_ADDRESS: " .. dbus_address)

    -- 检查 lua 是否存在
    local check_lua = io.popen("which lua 2>/dev/null")
    local lua_path = nil
    if check_lua then
        lua_path = check_lua:read("*line")
        check_lua:close()
    end

    if not lua_path or lua_path == "" then
        print("警告: 未找到 lua 解释器，跳过 Lua D-Bus 测试")
        os.remove(temp_script)
        return
    end

    print("找到 lua 解释器: " .. lua_path)

    -- 在子进程中运行测试
    local cmd = string.format(
                    "LD_LIBRARY_PATH='%s' DBUS_SESSION_BUS_ADDRESS='%s' lua %s",
                    new_ld_path, dbus_address, temp_script)

    print("运行命令: " .. cmd)

    local success, _, exit_code = os.execute(cmd)

    -- 清理临时脚本
    os.remove(temp_script)

    -- 处理 Lua 5.3+ 的返回值格式
    -- success: true/nil, exit_type: "exit"/"signal", exit_code: 退出码
    local actual_exit_code
    if type(success) == "number" then
        -- Lua 5.1/5.2: os.execute() 直接返回退出码
        actual_exit_code = success
    else
        -- Lua 5.3+: os.execute() 返回 (success, type, code)
        actual_exit_code = exit_code or 0
        if not success then actual_exit_code = exit_code or 1 end
    end

    print("Lua D-Bus 子进程退出码: " .. tostring(actual_exit_code))

    -- 直接断言退出码为 0，失败时会显示详细错误信息
    lu.assertEquals(actual_exit_code, 0,
                    "Lua DBus 测试失败，退出码: " ..
                        tostring(actual_exit_code))

    print("========== Lua D-Bus 测试完成 ==========\n")
end

-- 运行测试（只运行 TestLibmcpp 类的测试）
print("\n========================================")
print("开始运行 TestLibmcpp 测试套件")
print("========================================\n")

local result = lu.LuaUnit.run()

print("\n========================================")
print("测试套件执行完成，退出码: " .. tostring(result))
print("========================================\n")

-- 清理 D-Bus daemon
if dbus_daemon_started then
    print("清理 D-Bus daemon...")
    os.execute("pkill -f 'dbus-daemon --session'")
    if io.open(dbus_address_file, "r") then os.remove(dbus_address_file) end
end

os.exit(result)
