mcdbus_src_files = files(
  'dispatch/pending_call.cpp',
  'dispatch/timeout.cpp',
  'dispatch/watch.cpp',
  'shm/gvariant_convert.cpp',
  'shm/harbor.cpp',
  'shm/local_msg.cpp',
  'shm/serialize.cpp',
  'shm/shm_tree.cpp',
  'bus_mode_impl.cpp',
  'connection.cpp',
  'connection_impl.cpp',
  'error.cpp',
  'match.cpp',
  'message.cpp',
  'signal.cpp',
  'validator.cpp',
  'sd_bus.cpp',
  'dynamic_object.cpp',
)

mcdbus_inc_dirs = [incdir, srcdir]

mcdbus_lib = library(
  'mcdbus',
  mcdbus_src_files,
  include_directories: mcdbus_inc_dirs,
  dependencies: dep_arr,
  install: true,
  build_by_default: true,
)

mcdbus_dep = declare_dependency(
  include_directories: incdir,
  link_with: mcdbus_lib,
  dependencies: dep_arr,
)
