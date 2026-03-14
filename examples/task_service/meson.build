# 源文件列表
src_files = [
    'main.cpp',
    'test_service.cpp',
    'task/my_task_interface.cpp',
    'task/my_task_object.cpp',
]

# 构建 include_directories 列表（使用根目录定义的 stubdir_global）
example_inc_dirs = ['.']
if not enable_conan_compile
  example_inc_dirs += [stubdir_global]
endif

# 示例可执行文件
executable(
    'task_service',
    sources: src_files,
    include_directories: example_inc_dirs,
    dependencies: [libmcpp_dep, boost_dep, test_utilities_dep, dbus_dep],
    install: false,
)