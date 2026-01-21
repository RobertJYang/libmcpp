core_src_files = files(
  # object.cpp 和 connection_manager.cpp 已移至 mcbase
  'application.cpp',
  'config_manager.cpp',
  'config_validator.cpp',
  'default_supervisor.cpp',
  'dependency_sorter.cpp',
  'plugin_manager.cpp',
  'service.cpp',
  'service_manager.cpp',
  'supervisor_manager.cpp',
  'timer.cpp',
)

# 包含目录（相对于 src/core/meson.build）
incdir = include_directories('../../include')
srcdir = include_directories('..')

libmcpp_core_lib = library(
  'mcpp_core',
  core_src_files,
  include_directories: [incdir, srcdir],
  dependencies: dep_arr,
  install: true,
  install_dir: install_dir,
  build_by_default: true,
)
libmcpp_core_dep = declare_dependency(
  include_directories: [incdir, srcdir],
  link_with: libmcpp_core_lib,
  dependencies: [dep_arr, libmcpp_base_dep],
)