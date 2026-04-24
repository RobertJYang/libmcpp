mcapp_src_files = files(
  'app_proto.cpp',
  'base_app.cpp',
  'application.cpp',
  'dbus_proto.cpp',
  'service.cpp',
  'service_registry.cpp',
)

base_inc_dirs = [incdir, srcdir]

mcapp_lib = library(
  'mcapp',
  mcapp_src_files,
  include_directories: base_inc_dirs,
  dependencies: dep_arr,
  install: true,
  build_by_default: true,
)
mcapp_dep = declare_dependency(
  include_directories: incdir,
  link_with: mcapp_lib,
  dependencies: dep_arr,
)
