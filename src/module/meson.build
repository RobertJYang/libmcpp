# 定义模块源文件列表
module_src_files = files(
  'module_loader.cpp',
  'module_manager.cpp',
)

# 构建 include_directories 列表（使用根目录定义的 stubdir_global）
base_inc_dirs = [incdir, srcdir]
if not enable_conan_compile
  base_inc_dirs += [stubdir_global]
endif

libmcpp_module_lib = library(
  'mcpp_module',
  module_src_files,
  include_directories: base_inc_dirs,
  dependencies: [dep_arr, libmcpp_engine_dep],
  install: true,
  install_dir: install_dir,
  build_by_default: true,
)
libmcpp_module_dep = declare_dependency(
  include_directories: incdir,
  link_with: libmcpp_module_lib,
  dependencies: [dep_arr, libmcpp_engine_dep],
)