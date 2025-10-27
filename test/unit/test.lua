package.path = './temp/opt/bmc/lualib/?.lua;' .. package.path

-- 动态设置 LD_LIBRARY_PATH，确保能够加载库文件
local function setup_library_path()
    local lib_dirs = {}
    
    -- 查找 libmcpp.so 的位置
    local handle = io.popen("find temp/root/.conan2/p/b -name 'libmcpp.so' -type f 2>/dev/null | head -1")
    if handle then
        local lib_path = handle:read("*line")
        handle:close()
        if lib_path then
            local lib_dir = lib_path:match("(.*/)")
            table.insert(lib_dirs, lib_dir)
        end
    end
    
    -- 添加其他可能的库目录
    table.insert(lib_dirs, "temp/usr/lib64")
    table.insert(lib_dirs, "temp/root/.conan2/p/b/*/b/builddir/usr/lib64")
    
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

TestLibmcpp ={}
function TestLibmcpp:setupClass()
end
function TestLibmcpp:teardwonClass()
end
function TestLibmcpp:test()
    -- 获取库路径配置
    local new_ld_path = setup_library_path()
    
    -- 设置环境变量并运行测试
    local cmd = string.format("LD_LIBRARY_PATH='%s' temp/bin/libmcpp_test", new_ld_path)
    local result = os.execute(cmd)
    if result == true then
        result = 0
    end
    lu.assertEquals(result, 0)
end

os.exit(lu.run())
