# 安装所有头文件和内联文件（包括 .h, .inl 等），排除 meson.build
all_headers = run_command(
  'find', '.', '-type', 'f',
  '-not', '-name', 'meson.build',
  check: false
).stdout().strip().split('\n')

install_data(
  all_headers,
  install_dir: get_option('includedir'),
  preserve_path: true,
)
