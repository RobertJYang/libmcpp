engine_src_files = files(
  'errors/std_errors.cpp',
  'base.cpp',
  'call_stack.cpp',
  'context.cpp',
  'engine.cpp',
  'interface.cpp',
  'metadata.cpp',
  'object.cpp',
  'path.cpp',
  'path_iterator.cpp',
  'property.cpp',
  'service.cpp',
  'std_interface.cpp',
  'utils.cpp',
)

engine_src_files += dbus_src_files

libmcpp_engine_lib = library(
  'mcpp_engine',
  engine_src_files,
  include_directories: [incdir, srcdir],
  dependencies: [dep_arr, libmcpp_core_dep],
  install: true,
  install_dir: install_dir,
  build_by_default: true,
)
libmcpp_engine_dep = declare_dependency(
  include_directories: incdir,
  link_with: libmcpp_engine_lib,
  dependencies: [dep_arr, libmcpp_core_dep],
)