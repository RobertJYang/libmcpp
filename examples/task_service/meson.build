# 源文件列表
src_files = [
    'main.cpp',
    'test_service.cpp',
    'task/task_interface.cpp',
    'task/task_object.cpp',
]

# 示例可执行文件
executable(
    'task_service',
    sources: src_files,
    dependencies: [libmcpp_dep, boost_dep, test_utilities_dep],
    install: false,
)