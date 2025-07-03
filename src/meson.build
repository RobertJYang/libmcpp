# 包含子目录的 meson.build 文件
subdir('core')
subdir('db')
subdir('dbus')
subdir('dict')
subdir('engine')
subdir('expr')
subdir('futures')
subdir('interprocess')
subdir('io')
subdir('log')
subdir('memory')
subdir('reflect')
subdir('runtime')
subdir('variant')

# 源文件列表
src_files = [
  # 根目录源文件
  'common.cpp',
  'exception.cpp',
  'filesystem.cpp',
  'json.cpp',
  'time.cpp',
  'string.cpp',
  'singleton.cpp',
  
  # 各子目录的源文件
  core_src_files,
  db_src_files,
  dbus_src_files,
  dict_src_files,
  engine_src_files,
  expr_src_files,
  futures_src_files,
  interprocess_src_files,
  io_src_files,
  log_src_files,
  memory_src_files,
  reflect_src_files,
  runtime_src_files,
  variant_src_files,
]

dep_arr = [boost_dep, boost_program_options_dep, dbus_dep, glib_dep]

enable_conan_complie = get_option('enable_conan_complie')
add_project_arguments(
  '-DENABLE_CONAN_COMPLIE=' + enable_conan_complie.to_int().to_string(),
  language: 'cpp',
)
if enable_conan_complie
  message('已开启共享内存')
  libsomp_dep = dependency('libsomp', method: 'pkg-config', required: true)
  liblogging_dep = dependency('liblogging', method: 'pkg-config', required: true)
  dep_arr += [libsomp_dep, liblogging_dep]
else
  # 添加logging.cpp（需要特殊的包含路径）
  src_files += ['../tests/log/logging.cpp']
endif

if meson_build
  install_dir = meson.project_build_root() / 'usr/lib64'
  run_command('mkdir', '-p', install_dir, check: false)
else
  install_dir = 'usr/lib64'
endif

# 创建共享库
libmcpp_lib = shared_library(
  'libmcpp',
  src_files,
  include_directories: [incdir, file_appender_inc],
  dependencies: dep_arr,
  install: true,
  install_dir: install_dir,
)

# 导出库依赖
libmcpp_dep = declare_dependency(
  include_directories: incdir,
  link_with: libmcpp_lib,
  dependencies: dep_arr,
)