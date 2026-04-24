project(
  'libmcpp',
  'cpp',
  version: '0.1.0',
  default_options: [
    'warning_level=3',
    'cpp_std=c++17',
    'libdir=lib',
    'cpp_args=-fPIC -Wall -Wno-unused-parameter -Wno-unused-variable -Wno-sign-compare -Wno-pedantic',
    'default_library=static',
  ],
  subproject_dir: 'libraries',  # 指定子项目目录为 libraries
)

# 添加编译器选项
add_project_arguments('-fvisibility=hidden', language: 'cpp')

cpp_compiler = meson.get_compiler('cpp')
if cpp_compiler.get_id() == 'clang'
  add_project_arguments('-Wno-#warnings', language: 'cpp')
endif

# -Wno-deprecated-copy 选项只在 GCC 9+ 或 Clang 中支持
if cpp_compiler.get_id() == 'gcc'
  # 检查 GCC 版本是否 >= 9
  if cpp_compiler.version().version_compare('>=9.0')
    add_project_arguments('-Wno-deprecated-copy', language: 'cpp')
  endif
elif cpp_compiler.get_id() == 'clang'
  # Clang 支持该选项
  add_project_arguments('-Wno-deprecated-copy', language: 'cpp')
endif

# 配置选项
shared_mutex_max_readers = get_option('shared_mutex_max_readers')
if shared_mutex_max_readers != 0
  add_project_arguments(
    '-DMC_SHARED_MUTEX_MAX_READERS=' + shared_mutex_max_readers.to_string(),
    language: 'cpp',
  )
endif

meson_build = get_option('meson_build')
build_tests = get_option('tests')
tests_utilities = get_option('tests_utilities')
enable_coverage = get_option('enable_coverage')
enable_conan_compile = get_option('enable_conan_compile')

# 编译宏定义（必须在构建目标之前）
add_project_arguments(
  '-DENABLE_CONAN_COMPILE=' + enable_conan_compile.to_int().to_string(),
  language: 'cpp',
)

# 代码覆盖率配置
if enable_coverage
  coverage_args = []
  coverage_link_args = []
  coverage_args = [
    '--coverage',
    '-fprofile-arcs',
    '-ftest-coverage',
    '-fprofile-update=atomic',
  ]
  coverage_link_args = ['--coverage']
  add_project_arguments(coverage_args, language: 'cpp')
  add_project_link_arguments(coverage_link_args, language: 'cpp')
  message('代码覆盖率统计已启用')
endif

# 依赖项
boost_dep = dependency('boost', method: 'pkg-config', required: true)
boost_program_options_dep = dependency(
  'boost',
  modules: ['program_options'],
  method: 'pkg-config',
  required: true,
)

# 设置编译宏
if meson.is_cross_build()
  current_buildtype = get_option('buildtype')
  if current_buildtype == 'debug'
    add_project_arguments('-DBUILD_TYPE=BUILD_TYPE_DEBUG', language: 'cpp')
  else
    add_project_arguments('-DBUILD_TYPE=BUILD_TYPE_RELEASE', language: 'cpp')
  endif
  add_project_arguments('-DLOGGING_PATH="/usr/lib64/liblogging.so"', language: 'cpp')
else
  add_project_arguments('-DBUILD_TYPE=BUILD_TYPE_DT', language: 'cpp')
  add_project_arguments('-DLOGGING_PATH=""', language: 'cpp')
endif

# 尝试使用 pkg-config 查找依赖
dbus_dep = dependency('dbus-1', method: 'pkg-config', required: true)
glib_dep = dependency('glib-2.0', method: 'pkg-config', required: true)
if enable_conan_compile
  openssl_dep = dependency('openssl', required: true)
else
  openssl_dep = declare_dependency()
endif

# 包含目录
incdir = include_directories('include')
srcdir = include_directories('src')

# 定义 stub 目录（在子目录中使用）
enable_conan_compile_global = get_option('enable_conan_compile')
if not enable_conan_compile_global
  stubdir_global = include_directories('stub')
else
  stubdir_global = []
endif

# 测试依赖
if build_tests or tests_utilities
  gtest_dep = dependency('gtest', method: 'pkg-config', required: true)
endif

mcbase_securec_option = 'disabled'
if enable_conan_compile
  mcbase_securec_option = 'enabled'
endif

# 子目录
# 使用 dependency 和 fallback 机制来引用 mcbase 子项目
# 这样会先尝试系统查找（pkg-config/cmake），找不到才使用 subproject
# 注意：如果系统找到了 mcbase，fallback 不会被触发，subproject 不会被创建
# 如果系统没找到，fallback 会被触发，subproject 会被创建
mcbase_dep = dependency(
  'mcbase',
  fallback: ['mcbase', 'mcbase_dep'],
  default_options: [
    'default_library=shared',
    'tests=' + build_tests.to_string(),
    'tests_utilities=' + tests_utilities.to_string(),
    'securec=' + mcbase_securec_option,
  ],
  required: true,
)
# 获取 subproject 以访问其他变量（如 incdir, lib 等）
# 注意：我们需要总是创建 subproject，因为我们需要访问其变量（如 mcbase_lib, mcbase_test_lib 等）
# 即使 dependency 使用了系统依赖，我们仍然需要 subproject 来构建测试等
mcbase_subproject = subproject('mcbase', default_options: [
  'default_library=shared',
  'tests=' + build_tests.to_string(),
  'tests_utilities=' + tests_utilities.to_string(),
  'meson_build=' + meson_build.to_string(),
  'securec=' + mcbase_securec_option,
])
# 从子项目获取其他需要的变量
mcbase_incdir = mcbase_subproject.get_variable('incdir')
mcbase_lib = mcbase_subproject.get_variable('mcbase_lib')
if build_tests
  mcbase_test_lib = mcbase_subproject.get_variable('mcbase_test_lib')
  mcbase_test_dep = mcbase_subproject.get_variable('mcbase_test_dep')
  mcbase_test_utilities_dep = mcbase_subproject.get_variable('test_utilities_dep')
  mock_dynamic_module = mcbase_subproject.get_variable('mock_dynamic_module')
elif tests_utilities
  # 只需要测试工具库，不需要测试库本身
  mcbase_test_lib = []
  mcbase_test_dep = declare_dependency()  # 空依赖
  mcbase_test_utilities_dep = mcbase_subproject.get_variable('test_utilities_dep')
  mock_dynamic_module = []  # 空列表
else
  mcbase_test_lib = []
  mcbase_test_dep = declare_dependency()  # 空依赖
  mcbase_test_utilities_dep = declare_dependency()  # 空依赖
  mock_dynamic_module = []  # 空列表
endif

mcengine_subproject = subproject('mcengine', default_options: [
  'default_library=static',
  'tests=' + build_tests.to_string(),
  'enable_coverage=' + enable_coverage.to_string(),
])
mcengine_dep = mcengine_subproject.get_variable('mcengine_dep')
mcengine_lib = mcengine_subproject.get_variable('mcengine_lib')

mcexpr_subproject = subproject('mcexpr', default_options: [
  'default_library=static',
  'tests=' + build_tests.to_string(),
  'enable_coverage=' + enable_coverage.to_string(),
])
mcexpr_dep = mcexpr_subproject.get_variable('mcexpr_dep')
mcexpr_lib = mcexpr_subproject.get_variable('mcexpr_lib')

mcdbus_subproject = subproject('mcdbus', default_options: [
  'default_library=static',
  'tests=' + build_tests.to_string(),
  'enable_coverage=' + enable_coverage.to_string(),
  'enable_conan_compile=' + enable_conan_compile.to_string(),
])
mcdbus_dep = mcdbus_subproject.get_variable('mcdbus_dep')
mcdbus_lib = mcdbus_subproject.get_variable('mcdbus_lib')

mcapp_subproject = subproject('mcapp', default_options: [
  'tests=' + build_tests.to_string(),
  'enable_coverage=' + enable_coverage.to_string(),
])
mcapp_dep = mcapp_subproject.get_variable('mcapp_dep')
mcapp_lib = mcapp_subproject.get_variable('mcapp_lib')

subdir('include')
subdir('src')
subdir('modules')
subdir('tests/test_utilities')
if build_tests
  if meson_build
    subdir('examples')
  endif
  subdir('tests')
endif