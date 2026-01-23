# 创建网络模块目录
network_dir = modules_dir / 'mc'
run_command('mkdir', '-p', network_dir, check: false)

# 构建网络模块
network_module = shared_library(
    'network',
    'module.cpp',
    include_directories: incdir,
    dependencies: dep_arr,
    link_with: [libmcpp_lib],
    install: true,
    install_dir: get_option('libdir') / 'libmcpp/modules/mc',
    name_prefix: 'lib',
    name_suffix: 'so',
)

# 复制模块到测试目录
custom_target(
    'copy_network_module',
    output: 'copy_network_module',
    command: ['cp', network_module.full_path(), network_dir / 'network.so'],
    depends: network_module,
    build_by_default: true,
)