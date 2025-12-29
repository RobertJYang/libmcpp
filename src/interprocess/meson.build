interprocess_src_files = [
  'interprocess/shared_memory.cpp',
  'interprocess/shared_memory_allocator.cpp',
  'interprocess/shared_memory_manager.cpp',
  'interprocess/mutex/ipc_mutex.cpp',
  'interprocess/mutex/ipc_shared_mutex.cpp',
]

# message_queue 仅在 Linux 上支持
if host_machine.system() == 'linux'
  interprocess_src_files += [
    'interprocess/message_queue.cpp',
  ]
endif