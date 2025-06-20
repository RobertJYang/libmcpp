# 源文件列表
src_files = [
  'common.cpp',
  'exception.cpp',
  'filesystem.cpp',
  'variant.cpp',
  'json.cpp',
  'time.cpp',
  'dict.cpp',
  'mutable_dict.cpp',
  'string.cpp',
  'singleton.cpp',
  'expr/node/literal_node.cpp',
  'expr/node/variable_node.cpp',
  'expr/node/unary_op_node.cpp',
  'expr/node/binary_op_node.cpp',
  'expr/node/function_call_node.cpp',
  'expr/node/property_access_node.cpp',
  'expr/node/conditional_node.cpp',
  'expr/node/object_method_call_node.cpp',
  'expr/node/template_string_node.cpp',
  'expr/context.cpp',
  'expr/function.cpp',
  'expr/engine.cpp',
  'expr/lexer.cpp',
  'expr/parser.cpp',
  'expr/builtin.cpp',
  'expr/builtin/math.cpp',
  'expr/builtin/string.cpp',
  'expr/builtin/conversion.cpp',
  'expr/function/call.cpp',
  'expr/function/parser.cpp',
  'expr/function/collection.cpp',
  'core/application.cpp',
  'core/default_supervisor.cpp',
  'core/dependency_sorter.cpp',
  'core/config_manager.cpp',
  'core/config_validator.cpp',
  'core/plugin_manager.cpp',
  'core/service_manager.cpp',
  'core/supervisor_manager.cpp',
  'core/object.cpp',
  'core/service.cpp',
  'core/connection_manager.cpp',
  'core/timer.cpp',
  'log/log_message.cpp',
  'log/logger.cpp',
  'log/log_manager.cpp',
  'log/appender_factory.cpp',
  'log/appenders/console_appender.cpp',
  'reflect/signature.cpp',
  'reflect/reflect.cpp',
  'interprocess/shared_memory.cpp',
  'interprocess/shared_memory_allocator.cpp',
  'interprocess/shared_memory_manager.cpp',
  'interprocess/mutex/ipc_mutex.cpp',
  'interprocess/mutex/ipc_shared_mutex.cpp',
  'db/byte_buffer.cpp',
  'db/transaction.cpp',
  'db/database.cpp',
  'io/io_buffer.cpp',
  'io/io_stream.cpp',
  'dbus/message.cpp',
  'dbus/validator.cpp',
  'dbus/connection.cpp',
  'dbus/connection_impl.cpp',
  'dbus/signal.cpp',
  'dbus/match.cpp',
  'dbus/dispatch/watch.cpp',
  'dbus/dispatch/timeout.cpp',
  'dbus/dispatch/pending_call.cpp',
  'dbus/shm/gvariant_convert.cpp',
  'dbus/shm/harbor.cpp',
  'dbus/shm/local_msg.cpp',
  'dbus/shm/serialize.cpp',
  'dbus/shm/shm_tree.cpp',
  'engine/base.cpp',
  'engine/object.cpp',
  'engine/engine.cpp',
  'engine/service.cpp',
  'engine/std_interface.cpp',
  'engine/error.cpp',
  'engine/error_engine.cpp',
  'engine/context.cpp',
  'engine/property.cpp',
  'engine/utils.cpp',
  'engine/path.cpp',
  'engine/path_iterator.cpp',
  'futures/state_pool.cpp',
  'futures/callback_list.cpp',
  'runtime/runtime.cpp',
  'runtime/runtime_context.cpp',
  'runtime/any_executor.cpp',
  'runtime/executor.cpp',
  'runtime/thread_list.cpp',
]

dep_arr = [boost_dep, boost_program_options_dep, dbus_dep, glib_dep]

enable_shared_memory = get_option('enable_shared_memory')
add_project_arguments(
  '-DENABLE_SHARED_MEMORY=' + enable_shared_memory.to_int().to_string(),
  language: 'cpp',
)
if enable_shared_memory
  message('已开启共享内存')
  libsomp_dep = dependency('libsomp', method: 'pkg-config', required: true)
  dep_arr += [libsomp_dep]
endif

if meson_build
  install_dir = meson.project_build_root() / 'usr/lib64'
  run_command('mkdir', '-p', install_dir, check: false)
else
  install_dir = 'usr/lib64'
endif

# 创建共享库
libmcpp_lib = shared_library(
  'libmcpp',
  src_files,
  include_directories: [incdir],
  dependencies: dep_arr,
  install: true,
  install_dir: install_dir,
)

# 导出库依赖
libmcpp_dep = declare_dependency(
  include_directories: incdir,
  link_with: libmcpp_lib,
  dependencies: dep_arr,
)