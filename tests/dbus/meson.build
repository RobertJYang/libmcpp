test_dbus_files = [
    'dbus/test_local_msg.cpp',
    'dbus/test_serialize.cpp',
    'dbus/test_gvariant_convert.cpp',
    'dbus/test_reflect.cpp',
    'dbus/test_dbus_message.cpp',
    'dbus/test_connection.cpp',
]

enable_shared_memory = get_option('enable_shared_memory')
if enable_shared_memory
  test_dbus_files += ['dbus/test_shm_call.cpp']
endif