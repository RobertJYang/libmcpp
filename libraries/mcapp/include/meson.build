install_subdir(
  'mc',
  install_dir: get_option('includedir'),
)

if build_test_utilities
  install_headers(
    'test_utilities/dbus_daemon_manager.h',
    'test_utilities/resource_manager.h',
    'test_utilities/test_base.h',
    subdir: 'test_utilities',
  )
endif
