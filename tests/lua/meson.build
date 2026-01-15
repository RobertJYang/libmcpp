# Lua 测试用例

lua_interpreter = find_program('lua', required: false)

if lua_interpreter.found()
    # 检查 luaunit 是否可用
    luaunit_check = run_command(
        lua_interpreter,
        '-e',
        'require("luaunit")',
        check: false
    )
    
    if luaunit_check.returncode() == 0
        message('Found luaunit, enabling Lua tests')
        
        # 获取系统 Lua 路径
        lua_path_cmd = run_command(
            lua_interpreter,
            '-e',
            'print(package.path)',
            check: true
        )
        system_lua_path = lua_path_cmd.stdout().strip()
        
        # 获取 ldbus_module 依赖（在 src/meson.build 中定义）
        # 确保测试运行前 ldbus.so 已经编译完成
        ldbus_module = get_variable('ldbus_module', disabler())
        
        # 获取 ldft 和 lutils 模块依赖（在 src/luaclib/meson.build 中定义）
        # 确保测试运行前 ldft.so 和 lutils.so 已经编译完成
        lua_dft_lib = get_variable('lua_dft_lib', disabler())
        libmcpp_lua_utils_lib = get_variable('libmcpp_lua_utils_lib', disabler())
        
        # 检查 dbus-run-session 或 dbus-launch
        dbus_runner = find_program('dbus-run-session', required: false)
        if not dbus_runner.found()
            dbus_runner = find_program('bash', required: true)
            wrapper_script = meson.current_source_dir() / 'run_with_dbus.sh'
            
            # 使用 wrapper 脚本启动 D-Bus session
            test('lua_tests',
                 dbus_runner,
                 args: [
                     wrapper_script,
                     lua_interpreter.full_path(),
                     meson.current_source_dir() / 'run_all_tests.lua'
                 ],
                 env: [
                    'LUA_CPATH=' + meson.project_build_root() / 'src' / '?.so;' +
                                  meson.project_build_root() / 'src' / 'luaclib' / '?.so;' +
                                  meson.project_build_root() / 'src' / 'luaclib' / '?' / '?.so',
                     'LUA_PATH=' + meson.current_source_dir() / '?.lua;' + 
                                  meson.current_source_dir() / 'dbus' / '?.lua;' +
                                  system_lua_path
                 ],
                 depends: [ldbus_module, lua_dft_lib, libmcpp_lua_utils_lib],
                 timeout: 60,
                 workdir: meson.current_source_dir())
            message('Lua D-Bus tests will run with temporary D-Bus session (via wrapper)')
        else
            # 使用 dbus-run-session 直接运行
            test('lua_dbus_tests',
                 dbus_runner,
                 args: [
                     '--',
                     lua_interpreter.full_path(),
                     meson.current_source_dir() / 'run_all_tests.lua'
                 ],
                 env: [
                    'LUA_CPATH=' + meson.project_build_root() / 'src' / '?.so;' +
                                  meson.project_build_root() / 'src' / 'luaclib' / '?.so;' +
                                  meson.project_build_root() / 'src' / 'luaclib' / '?' / '?.so',
                     'LUA_PATH=' + meson.current_source_dir() / '?.lua;' + 
                                  meson.current_source_dir() / 'dbus' / '?.lua;' +
                                  system_lua_path
                 ],
                 depends: [ldbus_module, lua_dft_lib, libmcpp_lua_utils_lib],
                 timeout: 60,
                 workdir: meson.current_source_dir())
            message('Lua D-Bus tests will run with temporary D-Bus session (via dbus-run-session)')
        endif
    else
        warning('luaunit not found, Lua tests will be skipped. Install with: luarocks install luaunit')
    endif
else
    warning('Lua interpreter not found, Lua tests will be skipped')
endif
