# 包含子目录的 meson.build 文件
subdir('crypto')
subdir('db')
subdir('dbus')
subdir('mdb')
subdir('debounce')
subdir('dict')
subdir('fmt')
subdir('futures')
subdir('interprocess')
subdir('io')
subdir('log')
subdir('memory')
subdir('reflect')
subdir('runtime')
subdir('string')
subdir('variant')
subdir('validate')

compress_src_files = [
  'compress.cpp',
]

base_src_files = [
  # 根目录源文件
  'common.cpp',
  'exception.cpp',
  'filesystem.cpp',
  'json.cpp',
  'json_wrapper.cpp',
  'time.cpp',
  'singleton.cpp',
  'error.cpp',
  'error_engine.cpp',
  'error_message_parser.cpp',
  'error_message_converter.cpp',
  'result.cpp',
  'array.cpp',

  compress_src_files,
  debounce_src_files,
  dict_src_files,
  fmt_src_files,
  log_src_files,
  memory_src_files,
  reflect_src_files,
  runtime_src_files,
  string_src_files,
  variant_src_files,
  validate_src_files,
  crypto_src_files,
  db_src_files,
  interprocess_src_files,
  io_src_files,
  futures_src_files,
]

# 源文件列表
libmcpp_src_files = ['variant_c_api.cpp']

zlib_dep = dependency('zlib', required: true)
dep_arr = [boost_dep, boost_program_options_dep, dbus_dep, glib_dep, zlib_dep, openssl_dep]
mcpp_deps = []

enable_conan_compile = get_option('enable_conan_compile')
add_project_arguments(
  '-DENABLE_CONAN_COMPILE=' + enable_conan_compile.to_int().to_string(),
  language: 'cpp',
)
if enable_conan_compile
  message('使用conan构建')
  libsomp_dep = dependency('libsomp', method: 'pkg-config', required: true)
  liblogger_dep = dependency('liblogger', method: 'pkg-config', required: true)
  json_dep = dependency('json', method: 'pkg-config', required: true)
  securec_dep = dependency('securec', method: 'pkg-config', required: true)
  dep_arr += [libsomp_dep, liblogger_dep, json_dep, securec_dep]
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
libmcpp_base_lib = library(
  'mcpp_base',
  base_src_files,
  include_directories: base_inc_dirs,
  dependencies: dep_arr,
  install: true,
  install_dir: install_dir,
  build_by_default: true,
)
libmcpp_base_dep = declare_dependency(
  include_directories: incdir,
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

subdir('core')
subdir('engine')
subdir('expr')
subdir('module')

if get_option('default_library') == 'static'
  libmcpp_lib = shared_library(
    'mcpp',
    libmcpp_src_files,
    include_directories: base_inc_dirs,
    dependencies: dep_arr,
    install: true,
    install_dir: install_dir,
    link_whole: [
      libmcpp_base_lib,
      libmcpp_core_lib,
      libmcpp_engine_lib,
      libmcpp_expr_lib,
      libmcpp_module_lib,
    ],
  )
else
  dep_arr += mcpp_deps
  libmcpp_lib = shared_library(
    'mcpp',
    libmcpp_src_files,
    include_directories: base_inc_dirs,
    dependencies: dep_arr,
    install: true,
    install_dir: install_dir,
    build_by_default: true,
  )
endif

# Lua 模块构建（需要在 libmcpp_lib 创建之后，以支持动态链接）
subdir('luaclib')

# 导出库依赖
# 在静态库模式下，虽然使用link_whole包含了所有子库，但为了确保依赖关系正确传递，
# 需要在dependencies中显式包含所有子库的依赖
if get_option('default_library') == 'static'
  libmcpp_dep = declare_dependency(
    include_directories: incdir,
    link_with: libmcpp_lib,
    dependencies: [
      dep_arr,
      libmcpp_base_dep,
      libmcpp_core_dep,
      libmcpp_engine_dep,
      libmcpp_expr_dep,
      libmcpp_module_dep,
    ],
  )
else
  libmcpp_dep = declare_dependency(
    include_directories: incdir,
    link_with: libmcpp_lib,
    dependencies: dep_arr,
  )
endif
