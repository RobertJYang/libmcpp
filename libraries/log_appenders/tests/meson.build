log_appenders_test_sources = files(
  'main.cpp',
  'test_file_appender.cpp',
  'test_socket_appender.cpp',
)

log_appenders_test_exe = executable(
  'log_appenders_test',
  log_appenders_test_sources,
  include_directories: [log_appenders_inc, log_appenders_private_inc],
  dependencies: [
    gtest_dep,
    mcbase_dep,
    mcengine_dep,
    glib_dep,
    mcbase_test_utilities_dep,
    appenders_plugins_dep,
  ],
  install: false,
  build_by_default: false,
)

test(
  'log_appenders unit tests',
  log_appenders_test_exe,
  is_parallel: false,
  timeout: 300,
)
