# mcengine SHM 微基准
#
# 构建：默认 OFF，用 -Dlibraries/mcengine:with_benches=true 打开。
# 不挂入 meson test()，避免污染常规测试套件；运行方式见
# docs/plans/2026-04-XX-shm-runtime-baseline.md。

bench_shm_object_throughput_exe = executable(
  'bench_shm_object_throughput',
  files('bench_shm_object_throughput.cpp'),
  include_directories: [incdir, srcdir],
  dependencies: [
    boost_dep,
    mcengine_dep,
  ],
  link_with: mcengine_lib,
  install: false,
)

bench_shm_recovery_time_exe = executable(
  'bench_shm_recovery_time',
  files('bench_shm_recovery_time.cpp'),
  include_directories: [incdir, srcdir],
  dependencies: [
    boost_dep,
    mcengine_dep,
  ],
  link_with: mcengine_lib,
  install: false,
)
