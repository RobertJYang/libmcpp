# lua 模块构建文件
# Lua 绑定模块动态链接到 libmcpp.so，避免静态变量重复初始化问题

# 各子模块源文件定义
subdir('dbus')
subdir('lvalidate')
subdir('log')
subdir('utils')
subdir('dft')
subdir('shm_tree')
subdir('mdb')
subdir('json')
subdir('error')

if lua_dep.found()
  # 获取可选的 libsomp 依赖
  libsomp_dep = dependency('libsomp', required: false)
  
  # 基础依赖列表（仅用于编译时头文件）
  lua_base_deps = [dbus_dep, boost_dep, glib_dep, lua_dep]
  if libsomp_dep.found()
    lua_base_deps += [libsomp_dep]
  endif

  # 编译 lutils.so 动态库（Lua C 模块）
  # 动态链接到 libmcpp.so
  libmcpp_lua_utils_lib = shared_library(
    'lutils',
    lua_utils_sources,
    include_directories: [incdir, srcdir],
    dependencies: lua_base_deps,
    link_with: [libmcpp_lib],
    name_prefix: '',
    gnu_symbol_visibility: 'default',
    install: true,
    install_dir: install_dir,
    build_by_default: true,
    override_options: ['b_lundef=false'],
  )

  # 编译 ldft.so 动态库（Lua C 模块）
  lua_dft_lib = shared_library(
    'ldft',
    dft_sources,
    include_directories: [incdir, srcdir],
    dependencies: lua_base_deps,
    link_with: [libmcpp_lib, libmcpp_lua_utils_lib],
    name_prefix: '',
    gnu_symbol_visibility: 'default',
    install: true,
    install_dir: install_dir,
    build_by_default: true,
    override_options: ['b_lundef=false'],
  )

  # 编译 ldbus 统一 Lua 模块
  ldbus_module = shared_module(
    'ldbus',
    ldbus_src,
    name_prefix: '',
    name_suffix: 'so',
    dependencies: lua_base_deps,
    link_with: [libmcpp_lib, libmcpp_lua_utils_lib],
    include_directories: [incdir, srcdir],
    install: true,
    install_dir: install_dir,
    gnu_symbol_visibility: 'default',
    override_options: ['b_lundef=false'],
  )

  # 编译 lvalidate 模块（纯 Lua 验证，不依赖 libmcpp）
  lvalidate = shared_module(
    'lvalidate',
    lvalidate_src,
    include_directories: [incdir, srcdir],
    dependencies: [lua_dep, boost_dep, glib_dep],
    link_with: [libmcpp_lib],
    name_prefix: '',
    install: true,
    install_dir: install_dir,
    build_by_default: true,
    override_options: ['b_lundef=false'],
  )

  # 编译 lmdb Lua 模块
  lmdb_module = shared_module(
    'lmdb',
    lmdb_src,
    name_prefix: '',
    name_suffix: 'so',
    dependencies: lua_base_deps,
    link_with: [libmcpp_lib, libmcpp_engine_lib, libmcpp_lua_utils_lib],
    include_directories: [incdir, srcdir],
    install: true,
    install_dir: install_dir,
    gnu_symbol_visibility: 'default',
    override_options: ['b_lundef=false'],
  )

  # 编译 mc_log Lua 模块（依赖 lutils 以使用 lua_to_variant 解析 attrs table）
  mc_log_module = shared_module(
    'mc_log',
    mc_log_src,
    name_prefix: '',
    name_suffix: 'so',
    dependencies: lua_base_deps,
    link_with: [libmcpp_lib, libmcpp_lua_utils_lib],
    include_directories: [incdir, srcdir],
    install: true,
    install_dir: install_dir,
    gnu_symbol_visibility: 'default',
  )

  # 编译 lshm_tree Lua 模块
  lshm_tree_module = shared_module(
    'lshm_tree',
    shm_tree_src,
    name_prefix: '',
    name_suffix: 'so',
    dependencies: lua_base_deps,
    link_with: [libmcpp_lib, libmcpp_lua_utils_lib],
    include_directories: [incdir, srcdir],
    install: true,
    install_dir: install_dir,
    gnu_symbol_visibility: 'default',
  )
else
  warning('Lua not found, Lua modules will not be built')
endif

base_inc_dirs1 = [incdir, srcdir]
if not enable_conan_compile
  base_inc_dirs1 += [stubdir_global]
endif

if enable_conan_compile
  lua_base_deps += [json_dep]
endif

# 编译 ljson.so 动态库（Lua C 模块）
if lua_dep.found()
  lua_json_lib = shared_module(
    'ljson',
    json_src,
    name_prefix: '',
    name_suffix: 'so',
    dependencies: lua_base_deps,
    link_with: [libmcpp_lib, libmcpp_base_lib, libmcpp_lua_utils_lib],
    include_directories: base_inc_dirs1,
    install: true,
    install_dir: install_dir,
    gnu_symbol_visibility: 'default',
    override_options: ['b_lundef=false'],  # 允许未定义的 lua 符号
  )

  # 编译 mc_error Lua 模块
  error_module = shared_module(
    'mc_error',
    error_src,
    name_prefix: '',
    name_suffix: 'so',
    dependencies: lua_base_deps,
    link_with: [libmcpp_lib, libmcpp_base_lib, libmcpp_lua_utils_lib],
    include_directories: base_inc_dirs1,
    install: true,
    install_dir: install_dir,
    gnu_symbol_visibility: 'default',
    override_options: ['b_lundef=false'],
  )
endif
