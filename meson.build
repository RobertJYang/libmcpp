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
  subproject_dir: 'libraries', # 指定子项目目录为 libraries
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
build_examples = get_option('examples')
build_test_utilities = build_tests or build_examples or get_option('tests_utilities')
enable_coverage = get_option('enable_coverage')
enable_conan_compile = get_option('enable_conan_compile')
use_shm = get_option('use_shm')
use_old_shm = get_option('use_old_shm')

# 进程间通信架构由 use_shm（新 SHM）与 use_old_shm（旧 harbor/shm_tree 兼容）两个独立开关控制。
# 顶层只做"互斥 + 透传"，运行时语义/skynet 依赖校验下沉到对应子工程：
#   - mcengine 子工程使用 use_shm；
#   - mcdbus / mcapp 子工程使用 use_old_shm；
#   - mcdbus 子工程在 use_old_shm=true 时强校验 skynet 必须可用，否则 configure 报错。
if use_shm and use_old_shm
  error(
    'use_shm 与 use_old_shm 互斥，不能同时为 true。',
    '请二选一：-Duse_shm=true -Duse_old_shm=false（新架构，meson 默认）',
    '或 -Duse_shm=false -Duse_old_shm=true（旧 dbus/shm 兼容，conan 默认）。',
  )
endif

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
boost_root = get_option('boost_root')
if boost_root == ''
  boost_root = run_command('sh', '-c', 'echo $BOOST_ROOT', check: false).stdout().strip()
endif

if boost_root != ''
  boost_include_dir = boost_root
  if not cpp_compiler.has_header('boost/version.hpp', args: ['-I' + boost_include_dir], required: false)
    boost_include_dir = boost_root / 'include'
  endif
  if not cpp_compiler.has_header('boost/version.hpp', args: ['-I' + boost_include_dir], required: false)
    error('Boost headers not found in ' + boost_root)
  endif
  boost_dep = declare_dependency(compile_args: ['-I' + boost_include_dir])
  message('使用 Boost: ' + boost_include_dir)
else
  boost_dep = dependency('boost', required: false)
  if not boost_dep.found()
    if not cpp_compiler.has_header('boost/version.hpp', required: false)
      error('Boost headers not found')
    endif
    boost_dep = declare_dependency()
  endif
  message('使用系统 Boost')
endif

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

# stub 头目录（非 Conan 构建时用于占位缺失的外部接口）
if not enable_conan_compile
  stubdir_global = include_directories('stub')
else
  stubdir_global = []
endif

# 测试依赖
if build_tests
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
    'tests_utilities=' + build_test_utilities.to_string(),
    'securec=' + mcbase_securec_option,
    'boost_root=' + boost_root,
  ],
  required: true,
)
# 获取 subproject 以访问其他变量（如 incdir, lib 等）
# 注意：我们需要总是创建 subproject，因为我们需要访问其变量（如 mcbase_lib, mcbase_test_lib 等）
# 即使 dependency 使用了系统依赖，我们仍然需要 subproject 来构建测试等
mcbase_subproject = subproject(
  'mcbase',
  default_options: [
    'default_library=shared',
    'tests=' + build_tests.to_string(),
    'tests_utilities=' + build_test_utilities.to_string(),
    'meson_build=' + meson_build.to_string(),
    'securec=' + mcbase_securec_option,
    'boost_root=' + boost_root,
  ],
)
# 从子项目获取其他需要的变量
mcbase_incdir = mcbase_subproject.get_variable('incdir')
mcbase_lib = mcbase_subproject.get_variable('mcbase_lib')
if build_tests
  mcbase_test_lib = mcbase_subproject.get_variable('mcbase_test_lib')
  mcbase_test_dep = mcbase_subproject.get_variable('mcbase_test_dep')
  mcbase_test_utilities_dep = mcbase_subproject.get_variable('test_utilities_dep')
  mock_dynamic_module = mcbase_subproject.get_variable('mock_dynamic_module')
else
  mcbase_test_lib = []
  mcbase_test_dep = declare_dependency() # 空依赖
  mcbase_test_utilities_dep = declare_dependency() # 空依赖
  mock_dynamic_module = [] # 空列表
endif

mcengine_subproject = subproject(
  'mcengine',
  default_options: [
    'default_library=static',
    'tests=' + build_tests.to_string(),
    'tests_utilities=' + build_test_utilities.to_string(),
    'enable_coverage=' + enable_coverage.to_string(),
    'boost_root=' + boost_root,
    'use_shm=' + use_shm.to_string(),
  ],
)
mcengine_dep = mcengine_subproject.get_variable('mcengine_dep')
mcengine_lib = mcengine_subproject.get_variable('mcengine_lib')

mcexpr_subproject = subproject(
  'mcexpr',
  default_options: [
    'default_library=static',
    'tests=' + build_tests.to_string(),
    'enable_coverage=' + enable_coverage.to_string(),
    'boost_root=' + boost_root,
  ],
)
mcexpr_dep = mcexpr_subproject.get_variable('mcexpr_dep')
mcexpr_lib = mcexpr_subproject.get_variable('mcexpr_lib')

mcdbus_subproject = subproject(
  'mcdbus',
  default_options: [
    'default_library=static',
    'tests=' + build_tests.to_string(),
    'enable_coverage=' + enable_coverage.to_string(),
    'enable_conan_compile=' + enable_conan_compile.to_string(),
    'use_old_shm=' + use_old_shm.to_string(),
  ],
)
mcdbus_dep = mcdbus_subproject.get_variable('mcdbus_dep')
mcdbus_lib = mcdbus_subproject.get_variable('mcdbus_lib')
mcdbus_internal_dep = mcdbus_subproject.get_variable('mcdbus_internal_dep')

mcapp_subproject = subproject(
  'mcapp',
  default_options: [
    'tests=' + build_tests.to_string(),
    'tests_utilities=' + build_test_utilities.to_string(),
    'enable_coverage=' + enable_coverage.to_string(),
    'use_old_shm=' + use_old_shm.to_string(),
  ],
)
mcapp_dep = mcapp_subproject.get_variable('mcapp_dep')
mcapp_lib = mcapp_subproject.get_variable('mcapp_lib')

# 一致性校验：subproject(default_options=...) 仅在子工程"首次加载"时生效，后续
# `meson configure -Duse_shm=... -Duse_old_shm=...` 单独覆盖顶层选项时，子工程选项
# 不会自动跟随。这里在所有子工程加载完后做一次硬校验，确保顶层 use_shm / use_old_shm
# 与 mcengine:use_shm / mcdbus:use_old_shm / mcapp:use_old_shm 始终保持单一真相；
# 任何错位都立刻 error，并指引用户用全 -D 显式覆盖或干脆 rm -rf builddir。
mcengine_use_shm_actual = mcengine_subproject.get_variable('use_shm')
mcdbus_use_old_shm_actual = mcdbus_subproject.get_variable('use_old_shm')
mcapp_use_old_shm_actual = mcapp_subproject.get_variable('use_old_shm')
if mcengine_use_shm_actual != use_shm or mcdbus_use_old_shm_actual != use_old_shm or mcapp_use_old_shm_actual != use_old_shm
  error(
    '顶层 use_shm / use_old_shm 与子工程实际值不一致：',
    '顶层 use_shm=' + use_shm.to_string() + '，use_old_shm=' + use_old_shm.to_string(),
    'mcengine:use_shm=' + mcengine_use_shm_actual.to_string() + '（期望 ' + use_shm.to_string() + '）',
    'mcdbus:use_old_shm=' + mcdbus_use_old_shm_actual.to_string() + '（期望 ' + use_old_shm.to_string() + '）',
    'mcapp:use_old_shm=' + mcapp_use_old_shm_actual.to_string() + '（期望 ' + use_old_shm.to_string() + '）',
    '原因：subproject default_options 不会在 reconfigure 时重新下发。',
    '解决：要么 rm -rf builddir 重新 setup；要么显式同时设置全部子选项，例如：',
    '  meson configure builddir -Duse_shm=' + use_shm.to_string() + ' -Duse_old_shm=' + use_old_shm.to_string()
    + ' -Dmcengine:use_shm=' + use_shm.to_string()
    + ' -Dmcdbus:use_old_shm=' + use_old_shm.to_string()
    + ' -Dmcapp:use_old_shm=' + use_old_shm.to_string(),
  )
endif
if build_test_utilities
  test_utilities_dep = mcapp_subproject.get_variable('mcapp_test_utilities_dep')
else
  test_utilities_dep = declare_dependency()
endif

subdir('include')
subdir('src')
if build_examples
  subdir('examples')
endif
if build_tests
  subdir('tests')
endif