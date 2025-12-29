test_interprocess_files = [
  'interprocess/mutex/test_ipc_mutex.cpp',
  'interprocess/mutex/test_shared_mutex.cpp',
  'interprocess/test_shared_memory.cpp',
  'interprocess/test_shared_memory_manager.cpp',
  'interprocess/test_shared_memory_allocator.cpp',
]

# message_queue 仅在 Linux 上支持
if host_machine.system() == 'linux'
  test_interprocess_files += [
    'interprocess/test_message_queue.cpp',
  ]
endif