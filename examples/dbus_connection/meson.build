# DBus连接示例程序构建文件

# 示例可执行文件
dbus_connection_example = executable(
    'dbus_connection_example',
    sources: ['main.cpp'],
    dependencies: [
        dependency('threads'),
        dependency('boost', modules: ['system']),
        libmcpp_dep,
    ],
    install: false,
)