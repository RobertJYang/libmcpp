test_dbus_files = [
    'dbus/test_reflect.cpp',
    'dbus/test_dbus_message.cpp',
    'dbus/test_connection.cpp',
    'dbus/test_validator.cpp',
    'dbus/test_error.cpp',
    'dbus/test_match.cpp',
    'dbus/shm/test_local_msg.cpp',
    'dbus/shm/test_serialize.cpp',
    'dbus/shm/test_gvariant_convert.cpp',
    'dbus/shm/test_harbor.cpp',
    'dbus/dispatch/test_dispatch.cpp',
]

enable_conan_compile = get_option('enable_conan_compile')
if enable_conan_compile
  test_dbus_files += ['dbus/shm/test_shm_call.cpp']
endif