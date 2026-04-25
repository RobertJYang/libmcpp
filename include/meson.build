# 安装头文件
install_headers(
  subdir : 'mc',
)

# 安装 C API 头文件
install_headers(
  'mc/variant_c_api.h',
  subdir : 'mc',
)

if build_tests
  install_emptydir(join_paths(get_option('includedir'), 'test_utilities'))
endif