# 与旧 shm 实现无关的 dbus 测试用例：任何 use_old_shm 配置都会编入。
# test_dynamic_object.cpp 中依赖 shm_tree 的子用例自身用 #if MCDBUS_USE_OLD_SHM 守护，
# 文件作为整体始终编译。
# gvariant_convert 工具与 shm 数据结构解耦，对应实现 (shm/gvariant_convert.cpp) 在
# mcdbus 侧也是常驻编译，所以测试同样常驻。
test_dbus_files = [
    'dbus/test_reflect.cpp',
    'dbus/test_dbus_message.cpp',
    'dbus/test_connection.cpp',
    'dbus/test_validator.cpp',
    'dbus/test_error.cpp',
    'dbus/test_match.cpp',
    'dbus/test_sd_bus.cpp',
    'dbus/test_signal_send_receive.cpp',
    'dbus/test_dynamic_object.cpp',
    'dbus/shm/test_gvariant_convert.cpp',
    'dbus/dispatch/test_dispatch.cpp',
]

# 仅在 use_old_shm=true 时才编译的旧 dbus/shm 兼容路径专属测试：
# 这些用例直接调用 mc::dbus::harbor / shm_tree / local_msg / serialize / shm_pending_msgs
# 等只有 use_old_shm=true 时才进入 libmcdbus 的 ::shm 实现符号。
# 关 use_old_shm 时整体不进编译单元，避免遗留靠 GTEST_SKIP 的"空壳"测试。
if get_option('use_old_shm')
    test_dbus_files += [
        'dbus/shm/test_local_msg.cpp',
        'dbus/shm/test_serialize.cpp',
        'dbus/shm/test_harbor.cpp',
        'dbus/shm/test_shm_tree_query.cpp',
    ]
endif
