# lua 模块构建文件
# 包含子目录的 meson.build 文件

# 各子模块源文件定义
subdir('dbus')
subdir('lvalidate')
subdir('log')
subdir('utils')
subdir('dft')
subdir('shm_tree')

# 编译 lutils.so 动态库（Lua C 模块）
# 注意: Lua C 模块的 lua 符号由运行时的 Lua 解释器提供,不需要在编译时链接
if lua_dep.found()
  libmcpp_lua_utils_lib = shared_library(
    'lutils',
    lua_utils_sources,
    include_directories: [incdir, srcdir],
    dependencies: [libmcpp_base_dep, lua_dep],
    name_prefix: '',
    gnu_symbol_visibility: 'default',
    install: true,
    install_dir: install_dir,
    build_by_default: true,
    override_options: ['b_lundef=false'],  # 允许未定义的 lua 符号
  )

  # 导出 lua_utils 库依赖，供其他 Lua 模块使用
  libmcpp_lua_utils_dep = declare_dependency(
    include_directories: [incdir, srcdir],
    link_with: libmcpp_lua_utils_lib,
    dependencies: [libmcpp_base_dep, lua_dep],
  )
else
  libmcpp_lua_utils_dep = disabler()
endif

# 编译 ldft.so 动态库（Lua C 模块）
# 注意: Lua C 模块的 lua 符号由运行时的 Lua 解释器提供,不需要在编译时链接
if lua_dep.found() and libmcpp_lua_utils_dep.found()
  lua_dft_lib = shared_library(
    'ldft',
    dft_sources,
    include_directories: [incdir, srcdir],
    dependencies: [libmcpp_base_dep, libmcpp_lua_utils_dep, lua_dep],
    name_prefix: '',
    gnu_symbol_visibility: 'default',
    install: true,
    install_dir: install_dir,
    build_by_default: true,
    override_options: ['b_lundef=false'],  # 允许未定义的 lua 符号
  )
endif


