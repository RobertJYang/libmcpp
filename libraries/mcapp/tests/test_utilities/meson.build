test_utilities_sources = [
  'dbus_daemon_manager.cpp',
  'resource_manager.cpp',
  'test_base.cpp',
]

test_utilities_lib = static_library(
  'mcapp_test_utilities',
  test_utilities_sources,
  include_directories: [incdir],
  dependencies: [
    mcbase_dep,
    raw_dbus_dep,
  ],
  install: true,
  install_dir: get_option('libdir'),
  build_by_default: true,
)

mcapp_test_utilities_dep = declare_dependency(
  link_with: test_utilities_lib,
  include_directories: [incdir],
  dependencies: [
    mcbase_dep,
    mcbase_test_utilities_dep,
    mcengine_dep,
    raw_dbus_dep,
  ],
)
