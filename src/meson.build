# 包含目录（相对于 src/meson.build）
incdir = include_directories('../include')
srcdir = include_directories('.')

# mcbase 的 include 目录从主项目获取（通过 subproject）
# 注意：mcbase_dep 已经包含了 include 目录，但某些地方需要单独使用

# 包含子目录的 meson.build 文件
subdir('crypto')
subdir('dbus')
subdir('debounce')
subdir('interprocess')
subdir('log')

# 获取 enable_conan_compile 选项
enable_conan_compile = get_option('enable_conan_compile')

base_src_files = [
  # 根目录源文件
  debounce_src_files,
  crypto_src_files,
  interprocess_src_files,
  log_appenders_src_files,
]

if enable_conan_compile
  base_src_files += 'json_wrapper.cpp'
endif

# 源文件列表
libmcpp_src_files = ['variant_c_api.cpp']

# 依赖 mcbase 库
dep_arr = [mcbase_dep, boost_dep, boost_program_options_dep, dbus_dep, glib_dep]
mcpp_deps = []

# enable_conan_compile 已由主项目定义，这里直接使用
if enable_conan_compile
  message('使用conan构建')
  libsomp_dep = dependency('libsomp', method: 'pkg-config', required: true)
  liblogger_dep = dependency('liblogger', method: 'pkg-config', required: true)
  json_dep = dependency('json', method: 'pkg-config', required: true)
  dep_arr += [libsomp_dep, liblogger_dep, json_dep]
endif

if meson_build
  install_dir = meson.project_build_root() / 'usr/lib64'
  run_command('mkdir', '-p', install_dir, check: false)
else
  install_dir = 'usr/lib64'
endif

# 构建 include_directories 列表（使用根目录定义的 stubdir_global）
base_inc_dirs = [incdir, srcdir]
if not enable_conan_compile
  base_inc_dirs += [stubdir_global]
endif

# 创建基础库
# 注意：mcbase_dep 已经包含了 include 目录，不需要单独指定
libmcpp_base_lib = library(
  'mcpp_base',
  base_src_files,
  include_directories: [incdir, srcdir],
  dependencies: dep_arr,
  install: true,
  install_dir: install_dir,
  build_by_default: true,
)
libmcpp_base_dep = declare_dependency(
  include_directories: [incdir, srcdir],
  link_with: libmcpp_base_lib,
  dependencies: dep_arr,
)
mcpp_deps += [libmcpp_base_dep]

# Lua 依赖查找（需要在 luaclib 子目录之前）
if enable_conan_compile
  # Conan 编译时使用 skynet 依赖
  lua_dep = dependency('skynet', required: false)
  if not lua_dep.found()
    warning('skynet not found in conan mode, trying system lua')
    lua_dep = dependency('lua5.3', required: false)
    if not lua_dep.found()
      lua_dep = dependency('lua', required: false)
    endif
  endif
else
  # 非 Conan 编译时使用系统 lua 依赖
  lua_dep = dependency('lua5.3', required: false)
  if not lua_dep.found()
    lua_dep = dependency('lua', required: false)
  endif
endif

# Lua 模块构建（需要在 base 库创建之后，因为 lua 模块依赖 libmcpp_base_dep）
subdir('luaclib')

subdir('core')
subdir('engine')
subdir('expr')

# 创建静态库
libmcpp_static_lib = static_library(
  'mcpp',
  libmcpp_src_files,
  include_directories: [incdir, srcdir],
  dependencies: dep_arr,
  link_whole: [
    libmcpp_base_lib,
    libmcpp_core_lib,
    libmcpp_engine_lib,
    libmcpp_expr_lib,
  ],
)

# 创建动态库，将整个静态库的符号包含进去
libmcpp_shared_lib = shared_library(
  'mcpp',
  [],
  include_directories: [incdir, srcdir],
  dependencies: dep_arr,
  link_whole: [
    libmcpp_static_lib,
  ],
  install: true,
  install_dir: install_dir,
  build_by_default: true,
  version: '1.0.0',
  soversion: '1',
)

# 导出库依赖（给外部程序使用）
# 使用动态库，符号的可见性由 MC_API 控制
# 只有标记为 MC_API 的符号才会被导出，其他符号是隐藏的（non-external）
# 注意：mcbase_dep 已经包含了 include 目录，不需要单独指定
libmcpp_dep = declare_dependency(
  include_directories: [incdir],
  link_with: libmcpp_shared_lib,
  dependencies: dep_arr,
)

# 为项目内部测试程序
# 测试程序使用静态库，可以访问所有符号（包括隐藏的）
libmcpp_dep_with_link = declare_dependency(
  include_directories: incdir,
  link_with: libmcpp_static_lib,
  dependencies: dep_arr,
)

# 编译 ldbus 统一 Lua 模块
if lua_dep.found()
  # 获取可选的 libsomp 依赖
  libsomp_dep = dependency('libsomp', required: false)
  
  # 构建依赖列表
  ldbus_deps = [lua_dep, dbus_dep, boost_dep]
  if libsomp_dep.found()
    ldbus_deps += [libsomp_dep]
  endif

  # 编译 ldbus 统一 Lua 模块
  # 使用 shared_module 而不是 shared_library，这样可以允许未定义的 lua 符号
  # Lua 符号会在运行时由 lua 解释器提供
  ldbus_module = shared_module(
    'ldbus',
    ldbus_src,
    name_prefix: '',  # 不要 lib 前缀
    name_suffix: 'so',
    dependencies: ldbus_deps + [libmcpp_dep],
    include_directories: [incdir, srcdir],
    install: true,
    install_dir: install_dir,  # 使用与 libmcpp.so 相同的目录
    gnu_symbol_visibility: 'default',  # 导出所有符号
  )
  message('ldbus module will be built with dependency: ' + lua_dep.name())

  lvalidate = shared_module(
    'lvalidate',
    lvalidate_src,
    include_directories: [incdir, srcdir],
    dependencies: lua_dep,
    name_prefix: '',
    install: true,
    install_dir: install_dir,
    build_by_default: true,
    override_options: ['b_lundef=false'],  # 允许未定义的符号
  )

  # 编译 log Lua 模块
  # 注意：file_appender.cpp 中使用了 engine::context::get_current_context_ptr()，
  # 所以需要链接 engine 库
  mc_log_module = shared_module(
    'mc_log',
    mc_log_src,
    name_prefix: '',  # 不要 lib 前缀
    name_suffix: 'so',
    dependencies: [lua_dep, dbus_dep, boost_dep, mcbase_dep],
    link_with: [libmcpp_base_lib, libmcpp_engine_lib],  
    include_directories: [incdir, srcdir],
    install: true,
    install_dir: install_dir,  # 使用与 libmcpp.so 相同的目录
    gnu_symbol_visibility: 'default',  # 导出所有符号
  )
  message('mc_log module will be built with dependency: ' + lua_dep.name())
else
  warning('Lua not found, ldbus module will not be built')
endif