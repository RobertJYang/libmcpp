core_src_files = files(
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

base_inc_dirs = [incdir, srcdir]

libmcpp_core_lib = library(
  'mcpp_core',
  core_src_files,
  include_directories: base_inc_dirs,
  dependencies: dep_arr,
  install: true,
  install_dir: install_dir,
  build_by_default: true,
)
libmcpp_core_dep = declare_dependency(
  include_directories: incdir,
  link_with: libmcpp_core_lib,
  dependencies: [dep_arr, libmcpp_base_internal_dep],
)