core_src_files = files(
  'application.cpp',
  'config_manager.cpp',
  'config_validator.cpp',
  'connection_manager.cpp',
  'default_supervisor.cpp',
  'dependency_sorter.cpp',
  'object.cpp',
  'plugin_manager.cpp',
  'service.cpp',
  'service_manager.cpp',
  'supervisor_manager.cpp',
  'timer.cpp',
)

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
  include_directories: incdir,
  link_with: libmcpp_core_lib,
  dependencies: [dep_arr, libmcpp_base_dep],
)