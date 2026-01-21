# 创建协议模块目录
protocol_dir = modules_dir / 'mc' / 'protocol'
run_command('mkdir', '-p', protocol_dir, check: false)

# 构建协议模块（使用 init.so 模式）
protocol_module = shared_library(
    'init',
    'init.cpp',
    include_directories: incdir,
    dependencies: dep_arr,
    install: true,
    install_dir: get_option('libdir') / 'libmcpp/modules/mc/protocol',
    name_prefix: 'lib',
    name_suffix: 'so',
)

# 复制模块到测试目录
custom_target(
    'copy_protocol_module',
    output: 'copy_protocol_module',
    command: ['cp', protocol_module.full_path(), protocol_dir / 'init.so'],
    depends: protocol_module,
    build_by_default: true,
)