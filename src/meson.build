# 包含目录（相对于 src/meson.build）
incdir = include_directories('../include')
srcdir = include_directories('.')

# 包含子目录的 meson.build 文件
subdir('crypto')
subdir('dbus')
subdir('mdb')
subdir('debounce')
subdir('interprocess')
subdir('log')
subdir('validate')

compress_src_files = [
  'compress.cpp',
]

# 获取 enable_conan_compile 选项
enable_conan_compile = get_option('enable_conan_compile')

base_src_files = [
  # 根目录源文件
  'error_message_parser.cpp',
  'error_message_converter.cpp',

  compress_src_files,
  debounce_src_files,
  log_src_files,
  validate_src_files,
  crypto_src_files,
  interprocess_src_files,
]

if enable_conan_compile
  base_src_files += 'json_wrapper.cpp'
endif

# 源文件列表
libmcpp_src_files = ['variant_c_api.cpp']

zlib_dep = dependency('zlib', required: true)
dep_arr = [mcbase_dep, boost_dep, boost_program_options_dep, dbus_dep, glib_dep]
base_dep_arr = dep_arr + [zlib_dep]
mcpp_deps = []

enable_conan_compile = get_option('enable_conan_compile')
add_project_arguments(
  '-DENABLE_CONAN_COMPILE=' + enable_conan_compile.to_int().to_string(),
  language: 'cpp',
)

if enable_conan_compile
  message('使用conan构建')
  base_dep_arr += [openssl_dep]
  libsomp_dep = dependency('libsomp', method: 'pkg-config', required: true)
  liblogger_dep = dependency('liblogger', method: 'pkg-config', required: true)
  json_dep = dependency('json', method: 'pkg-config', required: true)
  base_dep_arr += [libsomp_dep, liblogger_dep, json_dep]
  dep_arr += [libsomp_dep]
endif

if meson_build
  install_dir = meson.project_build_root() / 'usr/lib64'
  run_command('mkdir', '-p', install_dir, check: false)
else
  install_dir = 'usr/lib64'
endif

base_inc_dirs = [incdir, srcdir]

# 创建基础库
libmcpp_base_lib = library(
  'mcpp_base',
  base_src_files,
  include_directories: base_inc_dirs,
  dependencies: base_dep_arr,
  install: true,
  install_dir: install_dir,
  build_by_default: true,
)
libmcpp_base_dep = declare_dependency(
  include_directories: base_inc_dirs,
  link_with: libmcpp_base_lib,
  dependencies: base_dep_arr,
)
libmcpp_base_internal_dep = declare_dependency(
  include_directories: base_inc_dirs,
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

# 先构建核心库（core, engine, expr），因为这些库被 luaclib 依赖
subdir('core')
subdir('engine')
subdir('expr')

# 创建静态库
libmcpp_static_lib = static_library(
  'mcpp',
  libmcpp_src_files,
  include_directories: base_inc_dirs,
  dependencies: [mcbase_dep, glib_dep],
  link_whole: [
    libmcpp_base_lib,
    libmcpp_core_lib,
    libmcpp_engine_lib,
    libmcpp_expr_lib,
  ],
)

# 创建动态库，将整个静态库的符号包含进去
# 必须显式声明 dependencies：libmcpp_base 等单元调用的 securec_* 符号由 libmcbase 提供，
# link_whole 不会自动把 mcbase_dep 链进本 .so，否则链接阶段会报 memcpy_s 等未定义引用
libmcpp_shared_lib = shared_library(
  'mcpp',
  [],
  include_directories: base_inc_dirs,
  dependencies: base_dep_arr,
  link_whole: [
    libmcpp_static_lib,
  ],
  install: true,
  install_dir: install_dir,
  build_by_default: true,
  version: '1.0.0',
  soversion: '1',
)

# 导出 libmcpp_lib 别名
libmcpp_lib = libmcpp_shared_lib

# 导出库依赖（给外部程序使用）
# 使用动态库，符号的可见性由 MC_API 控制
# 只有标记为 MC_API 的符号才会被导出，其他符号是隐藏的（non-external）
# 注意：mcbase_dep 已经包含了 include 目录，不需要单独指定
libmcpp_dep = declare_dependency(
  include_directories: [incdir],
  link_with: libmcpp_shared_lib,
  dependencies: base_dep_arr,
)

# 为项目内部测试程序
# 测试程序使用静态库，可以访问所有符号（包括隐藏的）
libmcpp_dep_with_link = declare_dependency(
  include_directories: incdir,
  link_whole: libmcpp_static_lib,
  dependencies: dep_arr,
)

# Lua 模块构建（需要在所有库创建之后）
subdir('luaclib')
