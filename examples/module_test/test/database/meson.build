# 创建测试数据库模块目录
test_database_dir = modules_dir / 'mc' / 'test'
run_command('mkdir', '-p', test_database_dir, check: false)

example_inc_dirs = [incdir]

# 构建测试数据库模块
test_database_module = shared_library(
    'database',
    'module.cpp',
    include_directories: example_inc_dirs,
    dependencies: dep_arr,
    install: false,
    install_dir: get_option('libdir') / 'libmcpp/modules/mc/test',
    name_prefix: 'lib',
    name_suffix: 'so',
    build_by_default: true,
)

# 复制模块到测试目录
custom_target(
    'copy_test_database_module',
    output: 'copy_test_database_module',
    command: [
        'cp',
        test_database_module.full_path(),
        test_database_dir / 'database.so',
    ],
    depends: test_database_module,
    build_by_default: true,
)