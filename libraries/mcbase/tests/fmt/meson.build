test_fmt_files = files(
    'test_chrono.cpp',
    'test_format.cpp',
    'test_format_dict.cpp',
    'test_format_errors.cpp',
    'test_format_v.cpp',
    'test_formatter_subheaders.cpp',
    'test_format_std.cpp',
    'test_mc_formatter.cpp',
    'test_named_args.cpp',
    'test_smart_placeholder.cpp',
)

# 兼容性测试程序（可选构建）
if get_option('compatibility_tests')
    # mc::format 兼容性测试（使用 C++17）
    test(
        'mc_format_test',
        executable(
            'mc_format_test',
            'compatibility_test.cpp',
            cpp_args: ['-std=c++17', '-DUSE_MC_FORMAT'], # 使用 mc::format
            dependencies: [gtest_dep, mcbase_dep],
            install: false,
            build_by_default: false,
        ),
        is_parallel: false,
        timeout: 300,
    )

    # std::format 兼容性测试（使用 C++20）
    test(
        'std_format_test',
        executable(
            'std_format_test',
            'compatibility_test.cpp',
            cpp_args: ['-std=c++20'], # 使用 std::format
            dependencies: [gtest_dep],
            install: false,
            build_by_default: false,
        ),
        is_parallel: false,
        timeout: 300,
    )
endif