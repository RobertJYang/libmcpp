# 包含子目录的 meson.build 文件
subdir('core')
subdir('dict')
subdir('fmt')
subdir('log')
subdir('memory')
subdir('reflect')
subdir('runtime')
subdir('string')
subdir('variant')
subdir('db')
subdir('io')
subdir('futures')
subdir('module')

base_src_files = [
  # 根目录源文件
  'common.cpp',
  'exception.cpp',
  'json.cpp',
  'time.cpp',
  'singleton.cpp',
  'error.cpp',
  'error_engine.cpp',
  'result.cpp',
  'array.cpp',
  'filesystem.cpp',

  core_src_files,
  dict_src_files,
  fmt_src_files,
  log_src_files,
  memory_src_files,
  reflect_src_files,
  runtime_src_files,
  string_src_files,
  variant_src_files,
  db_src_files,
  io_src_files,
  futures_src_files,
  module_src_files,
]

dep_arr = [boost_dep]
mcbasedeps = []

if meson_build
  install_dir = meson.project_build_root() / 'usr/lib64'
  run_command('mkdir', '-p', install_dir, check: false)
else
  install_dir = 'usr/lib64'
endif

# 创建基础库
# 通过 project() 中的 default_library=shared 控制为动态库
mcbase_lib = library(
  'mcbase',
  base_src_files,
  include_directories: [incdir, srcdir],
  dependencies: dep_arr,
  install: true,
  install_dir: install_dir,
  build_by_default: true,
  version: '1.0.0',
  soversion: '1',
)

mcbase_dep = declare_dependency(
  include_directories: incdir,
  link_with: mcbase_lib,
  dependencies: dep_arr,
)
