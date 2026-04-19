# 包含子目录的 meson.build 文件
subdir('intrusive')
subdir('dict')
subdir('fmt')
subdir('log')
subdir('memory')
subdir('reflect')
subdir('signal')
subdir('protocol')
subdir('runtime')
subdir('shm')
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
  'event.cpp',
  'result.cpp',
  'array.cpp',
  'filesystem.cpp',
  'object_base.cpp',
  'object.cpp',

  intrusive_src_files,
  dict_src_files,
  fmt_src_files,
  log_src_files,
  memory_src_files,
  reflect_src_files,
  signal_src_files,
  protocol_src_files,
  runtime_src_files,
  shm_src_files,
  string_src_files,
  variant_src_files,
  db_src_files,
  io_src_files,
  futures_src_files,
  module_src_files,
]

if not securec_dep.found()
  base_src_files += ['securec.cpp']
endif

dep_arr = [boost_dep]
if securec_dep.found()
  dep_arr += [securec_dep]
endif

# GCC 7 requires -lstdc++fs for std::experimental::filesystem
cc = meson.get_compiler('cpp')
stdcxxfs = cc.find_library('stdc++fs', required: false)
if stdcxxfs.found()
  dep_arr += [stdcxxfs]
endif
mcbasedeps = []

install_dir = get_option('libdir')

# 创建基础库
# 通过 project() 中的 default_library=shared 控制为动态库
# 设置 install_rpath 以支持相对路径链接（macOS: @loader_path, Linux: $ORIGIN）
mcbase_install_rpath = host_machine.system() == 'darwin' ? '@loader_path:@loader_path/../lib' : '$ORIGIN:$ORIGIN/../lib'

mcbase_lib = library(
  'mcbase',
  base_src_files,
  include_directories: [incdir, srcdir],
  dependencies: dep_arr,
  cpp_args: mcbase_sanitizer_cpp_args,
  link_args: mcbase_sanitizer_link_args,
  install: true,
  install_dir: install_dir,
  install_rpath: mcbase_install_rpath,
  build_by_default: true,
  version: '1.0.0',
  soversion: '1',
)

mcbase_dep = declare_dependency(
  include_directories: incdir,
  link_with: mcbase_lib,
  dependencies: dep_arr,
  compile_args: mcbase_sanitizer_cpp_args,
  link_args: mcbase_sanitizer_link_args,
)
