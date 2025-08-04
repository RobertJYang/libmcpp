# 包含子目录的 meson.build 文件
subdir('crypto')
subdir('db')
subdir('dbus')
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

base_src_files = [
  # 根目录源文件
  'common.cpp',
  'exception.cpp',
  'filesystem.cpp',
  'json.cpp',
  'time.cpp',
  'singleton.cpp',
  'error.cpp',
  'error_engine.cpp',
  'result.cpp',

  dict_src_files,
  fmt_src_files,
  log_src_files,
  memory_src_files,
  reflect_src_files,
  runtime_src_files,
  string_src_files,
  variant_src_files,
  crypto_src_files,
  db_src_files,
  interprocess_src_files,
  io_src_files,
  futures_src_files,
]

# 源文件列表
libmcpp_src_files = ['variant_c_api.cpp']

dep_arr = [boost_dep, boost_program_options_dep, dbus_dep, glib_dep]
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
  dep_arr += [libsomp_dep, liblogger_dep]
endif

if meson_build
  install_dir = meson.project_build_root() / 'usr/lib64'
  run_command('mkdir', '-p', install_dir, check: false)
else
  install_dir = 'usr/lib64'
endif

# 创建基础库
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
  include_directories: incdir,
  link_with: libmcpp_base_lib,
  dependencies: dep_arr,
)
mcpp_deps += [libmcpp_base_dep]

subdir('core')
subdir('engine')
subdir('expr')
subdir('module')

if get_option('default_library') == 'static'
  libmcpp_lib = shared_library(
    'mcpp',
    libmcpp_src_files,
    include_directories: [incdir, srcdir],
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
    include_directories: [incdir, srcdir],
    dependencies: dep_arr,
    install: true,
    install_dir: install_dir,
    build_by_default: true,
  )
endif

# 导出库依赖
libmcpp_dep = declare_dependency(
  include_directories: incdir,
  link_with: libmcpp_lib,
  dependencies: dep_arr,
)