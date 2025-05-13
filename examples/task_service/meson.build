# 源文件列表
src_files = [
    'main.cpp',
    'test_service.cpp',
    'task/my_task_interface.cpp',
    'task/my_task_object.cpp',
]

# 示例可执行文件
executable(
    'task_service',
    sources: src_files,
    include_directories: [
        '.',
    ],
    dependencies: [libmcpp_dep, boost_dep, test_utilities_dep, dbus_dep],
    install: false,
)