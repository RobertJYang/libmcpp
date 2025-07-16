# 定义模块源文件列表
module_src_files = files(
  'module_loader.cpp',
  'module_manager.cpp',
)

libmcpp_module_lib = shared_library(
  'mcpp_module',
  module_src_files,
  include_directories: [incdir, srcdir],
  dependencies: [dep_arr, libmcpp_engine_dep],
  install: true,
  install_dir: install_dir,
)
libmcpp_module_dep = declare_dependency(
  include_directories: incdir,
  link_with: libmcpp_module_lib,
  dependencies: [dep_arr, libmcpp_engine_dep],
)