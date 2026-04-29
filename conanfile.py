import os
from conanbase import ConanBase
from conan.tools.meson import Meson, MesonToolchain
from conan.tools.env import VirtualBuildEnv
from conan.tools.files import copy

required_conan_version = ">=2.13.0"


class AppConan(ConanBase):
    language = "c++"
    generators = "CMakeDeps", "VirtualBuildEnv", "PkgConfigDeps"
    options = {
        "asan": [True, False],
        "gcov": [True, False],
        "test": [True, False],
        "test_utilities": [True, False],
        "enable_luajit": [True, False],
        # 进程间通信架构开关，与顶层 meson 选项一一对应，禁止同时为 True：
        #   use_shm=True  + use_old_shm=False ：新 SHM 架构
        #   use_shm=False + use_old_shm=True  ：旧 dbus/shm 兼容（依赖 skynet，conan 默认）
        #   use_shm=False + use_old_shm=False ：mcengine local heap + mcdbus 纯 dbus
        "use_shm": [True, False],
        "use_old_shm": [True, False],
    }
    default_options = {
        "asan": False,
        "gcov": False,
        "test": False,
        "test_utilities": True,
        "enable_luajit": True,
        # conan 构建（DT / 交叉编译生产）默认走旧架构，保持与存量部署兼容；
        # 想在 conan 环境下试新架构：-o use_shm=True -o use_old_shm=False。
        "use_shm": False,
        "use_old_shm": True,
        # 远端预编译包使用以下 options 构建，保持一致以匹配二进制包 ID
        "liblogger/*:enable_luajit": True,
        "liblogger/*:test": True,
        "libsomp/*:enable_luajit": True,
        "libsomp/*:test": True,
    }

    # 基于meson构建的基类，适用于libmcpp项目
    def layout(self):
        self.folders.source = "."
        self.folders.build = "builddir"
        self.folders.generators = "builddir/conan_toolchain"

    def requirements(self):
        super().requirements()
        if self.options.test or self.options.test_utilities:
            self.requires("gtest/[>=1.14.0]@openubmc/stable")

    def generate(self):
        os.environ["PKG_CONFIG"] = "/usr/bin/pkg-config"
        tc = MesonToolchain(self, "ninja")
        # 启用 fallback 机制，允许 Meson 使用子项目作为依赖的备选方案
        # 注意：Conan 的 MesonToolchain 默认会设置 wrap_mode=nofallback，这会禁用 fallback
        # 设置 wrap_mode=nodownload 允许使用本地子项目作为 fallback，但不会下载新的 wrap 文件
        if (
            "wrap_mode" in tc.project_options
            and tc.project_options["wrap_mode"] == "nofallback"
        ):
            tc.project_options["wrap_mode"] = "nodownload"
        if self.settings.arch == "armv8" or self.settings.arch == "x86_64":
            tc.project_options["libdir"] = "usr/lib64"
        else:
            tc.project_options["libdir"] = "usr/lib"
        tc.project_options["enable_conan_compile"] = True
        if self.options.test:
            tc.project_options["tests"] = True
        else:
            tc.project_options["tests"] = False
        tc.project_options["tests_utilities"] = bool(self.options.test_utilities)
        tc.project_options["meson_build"] = False
        # 顶层 meson 会做互斥校验（use_shm 与 use_old_shm 不能同时 True），
        # 这里再前置一次让 conan 用户更早看到错误，且错误信息更贴 conan 选项。
        if bool(self.options.use_shm) and bool(self.options.use_old_shm):
            raise Exception(
                "use_shm 与 use_old_shm 互斥，不能同时为 True。"
                "请二选一：-o use_shm=True -o use_old_shm=False 或 -o use_shm=False -o use_old_shm=True。"
            )
        tc.project_options["use_shm"] = bool(self.options.use_shm)
        tc.project_options["use_old_shm"] = bool(self.options.use_old_shm)

        # 交叉编译时，将 Conan boost 包路径传给 meson boost_root option，
        # 防止 meson Boost handler 搜索到系统路径下的 x86_64 boost。
        boost_info = self.dependencies.get("boost")
        if boost_info:
            boost_root = boost_info.package_folder
            if boost_root:
                tc.project_options["boost_root"] = boost_root

        ms = VirtualBuildEnv(self)
        if self.settings.arch in ["armv8"]:
            tc.properties["pkg_config_libdir"] = (
                ms.vars().get("PKG_CONFIG_PATH").split(":")
            )

        if self.options.test:
            # 为 Debug 类型添加 -Os 优化参数
            tc.extra_cxxflags.append("-Os")
        tc.extra_ldflags.append("-lstdc++fs")
        tc.extra_cxxflags.append("-Wno-unused-variable")
        tc.extra_cxxflags.append("-Wno-unused-parameter")
        tc.extra_cxxflags.append("-Wno-sign-compare")
        tc.extra_cxxflags.append("-fpermissive")
        tc.extra_cxxflags.append("-Wno-pedantic")
        tc.extra_cxxflags.append("-fno-strict-aliasing")
        # -Wno-deprecated-copy 选项只在 GCC 9+ 或 Clang 中支持
        compiler = str(self.settings.compiler)
        compiler_version = (
            str(self.settings.compiler.version)
            if self.settings.compiler.version
            else None
        )
        if compiler == "gcc" and compiler_version:
            # GCC 9.0+ 支持该选项
            try:
                major_version = int(compiler_version.split(".")[0])
                if major_version >= 9:
                    tc.extra_cxxflags.append("-Wno-deprecated-copy")
            except (ValueError, AttributeError):
                pass  # 如果版本解析失败，跳过该选项
        elif compiler == "clang":
            # Clang 支持该选项
            tc.extra_cxxflags.append("-Wno-deprecated-copy")
        tc.generate()

    def build(self):
        meson = Meson(self)
        meson.configure()
        if self.options.test:
            self.test()
        else:
            meson.build()

    def test(self):
        """运行 Meson 测试"""
        import subprocess

        filter_arg = os.environ.get("MCLI_TEST_FILTER")
        verbose = os.environ.get("MCLI_TEST_VERBOSE")

        cmd = ["meson", "test", "-v"]

        if filter_arg:
            cmd.extend(["--suite", filter_arg])

        test_args_str = os.environ.get("MCLI_TEST_ARGS")
        if test_args_str:
            import shlex
            cmd.extend(shlex.split(test_args_str))

        self.output.info(f"运行 Meson 测试: {' '.join(cmd)}")
        result = subprocess.run(
            cmd,
            capture_output=False,
            text=True,
        )
        if result.returncode != 0:
            raise Exception("Meson 测试失败")

    def package(self):
        meson = Meson(self)
        meson.install()

        if os.path.isfile("permissions.ini"):
            copy(self, "permissions.ini")
        copy(
            self,
            "*",
            src=os.path.join(self.source_folder, "mds"),
            dst=os.path.join(self.package_folder, "include/mds"),
        )
        if os.path.isdir("include"):
            copy(
                self,
                "*",
                src=os.path.join(self.source_folder, "include"),
                dst=os.path.join(self.package_folder, "include"),
            )
        if os.path.isdir("mds"):
            copy(
                self,
                "*",
                src=os.path.join(self.source_folder, "mds"),
                dst=os.path.join(
                    self.package_folder, "usr/share/doc/openubmc/libmcpp/mds"
                ),
            )
        if os.path.isdir("docs"):
            copy(
                self,
                "*",
                src=os.path.join(self.source_folder, "docs"),
                dst=os.path.join(
                    self.package_folder, "usr/share/doc/openubmc/libmcpp/docs"
                ),
            )
        copy(
            self,
            "*.md",
            src=self.source_folder,
            dst=os.path.join(
                self.package_folder, "usr/share/doc/openubmc/libmcpp/docs"
            ),
        )
        copy(
            self,
            "*.MD",
            src=self.source_folder,
            dst=os.path.join(
                self.package_folder, "usr/share/doc/openubmc/libmcpp/docs"
            ),
        )
        mcbase_include = os.path.join(
            self.source_folder, "libraries", "mcbase", "include"
        )
        if os.path.isdir(mcbase_include):
            copy(
                self,
                "*",
                src=mcbase_include,
                dst=os.path.join(self.package_folder, "include"),
            )
        mcengine_test_include = os.path.join(
            self.source_folder,
            "libraries",
            "mcengine",
            "include",
            "test_utilities",
        )
        if os.path.isdir(mcengine_test_include):
            copy(
                self,
                "engine_test_base.h",
                src=mcengine_test_include,
                dst=os.path.join(self.package_folder, "include", "test_utilities"),
            )

        # 对静态库运行ranlib以创建符号索引
        self._run_ranlib_on_static_libs()

        # 对Debug模式编译生成的文件进行strip处理
        if self.settings.build_type == "Debug":
            self._strip_debug_files()

    def _strip_debug_files(self):
        """对Debug模式编译生成的文件进行strip处理"""
        import subprocess
        import stat

        # 根据架构确定strip工具
        if self.settings.arch == "armv8":
            strip_tool = "aarch64-target-linux-gnu-strip"
        elif self.settings.arch == "x86_64":
            strip_tool = "strip"
        else:
            strip_tool = "strip"

        # 查找需要strip的文件
        lib_dirs = []
        if self.settings.arch == "armv8" or self.settings.arch == "x86_64":
            lib_dirs.append("usr/lib64")
        else:
            lib_dirs.append("usr/lib")

        # 添加可执行文件和驱动库目录
        bin_dirs = ["opt/bmc/apps/libmcpp", "opt/bmc/drivers"]

        all_dirs = lib_dirs + bin_dirs

        for directory in all_dirs:
            dir_path = os.path.join(self.package_folder, directory)
            if not os.path.exists(dir_path):
                continue

            for root, dirs, files in os.walk(dir_path):
                for file in files:
                    file_path = os.path.join(root, file)

                    # 检查文件是否为可执行文件或共享库
                    try:
                        if os.path.isfile(file_path) and not os.path.islink(file_path):
                            # 检查文件类型
                            result = subprocess.run(
                                ["file", file_path],
                                capture_output=True,
                                text=True,
                                timeout=10,
                            )
                            if result.returncode == 0:
                                file_type = result.stdout.lower()
                                if (
                                    "executable" in file_type
                                    or "current ar archive" in file_type
                                    or "shared object" in file_type
                                    or "dynamically linked" in file_type
                                ):
                                    # 执行strip操作
                                    # 对于静态库，不要使用-s参数，因为会移除符号索引
                                    if "current ar archive" in file_type:
                                        self.run(
                                            f"{strip_tool} --strip-unneeded {file_path}"
                                        )
                                    else:
                                        self.run(f"{strip_tool} -s {file_path}")
                    except Exception as e:
                        print(f"Warning: Error processing {file_path}: {e}")
                        continue

    def _run_ranlib_on_static_libs(self):
        """对静态库运行ranlib以创建符号索引"""
        import subprocess

        lib_dirs = []
        if self.settings.arch == "armv8" or self.settings.arch == "x86_64":
            lib_dirs.append("usr/lib64")
        else:
            lib_dirs.append("usr/lib")

        for directory in lib_dirs:
            dir_path = os.path.join(self.package_folder, directory)
            if not os.path.exists(dir_path):
                continue

            for root, dirs, files in os.walk(dir_path):
                for file in files:
                    file_path = os.path.join(root, file)

                    # 检查是否为静态库文件
                    # 优先使用file命令检测，更可靠
                    try:
                        if os.path.isfile(file_path) and not os.path.islink(file_path):
                            # 使用file命令检查文件类型
                            result = subprocess.run(
                                ["file", file_path],
                                capture_output=True,
                                text=True,
                                timeout=10,
                            )
                            if result.returncode == 0:
                                file_type = result.stdout.lower()
                                # 检查是否为ar archive格式的静态库
                                if "current ar archive" in file_type:
                                    # 使用ranlib创建符号索引
                                    ranlib_result = subprocess.run(
                                        ["ranlib", file_path],
                                        capture_output=True,
                                        text=True,
                                        timeout=30,
                                    )
                                    if ranlib_result.returncode == 0:
                                        print(f"Successfully ran ranlib on {file_path}")
                                    else:
                                        print(
                                            f"Warning: Failed to run ranlib on {file_path}: {ranlib_result.stderr.strip()}"
                                        )
                    except subprocess.TimeoutExpired:
                        print(f"Warning: Timeout checking file type for {file_path}")
                    except Exception as e:
                        print(f"Warning: Exception processing {file_path}: {str(e)}")
                        continue

    def package_info(self):
        if self.settings.arch == "armv8" or self.settings.arch == "x86_64":
            self.cpp_info.libdirs = ["usr/lib64"]
            self.env_info.LD_LIBRARY_PATH.append(
                os.path.join(self.package_folder, "usr/lib64")
            )
            libdir = "usr/lib64"
        else:
            self.cpp_info.libdirs = ["usr/lib"]
            self.env_info.LD_LIBRARY_PATH.append(
                os.path.join(self.package_folder, "usr/lib")
            )
            libdir = "usr/lib"

        self.env_info.PATH.append(
            os.path.join(self.package_folder, "opt/bmc/apps/libmcpp")
        )

        include_dirs = ["include"]
        self.cpp_info.includedirs = include_dirs

        external_base_requires = [
            "libsomp::libsomp",
            "liblogger::liblogger",
            "boost::boost",
            "json::json",
            "huawei_secure_c::securec",
        ]

        # 配置可独立消费的组件，libs 使用不带 lib 前缀的库名。
        self.cpp_info.components["mcbase"].libs = ["mcbase"]
        self.cpp_info.components["mcbase"].libdirs = [libdir]
        self.cpp_info.components["mcbase"].includedirs = include_dirs
        self.cpp_info.components["mcbase"].requires = [
            "boost::boost",
            "huawei_secure_c::securec",
        ]
        self.cpp_info.components["mcbase"].set_property("pkg_config_name", "mcbase")
        self.cpp_info.components["mcbase"].set_property(
            "pkg_config_custom_content", f"libdir=${{prefix}}/{libdir}\n"
        )

        self.cpp_info.components["mcengine"].libs = ["mcengine"]
        self.cpp_info.components["mcengine"].libdirs = [libdir]
        self.cpp_info.components["mcengine"].includedirs = include_dirs
        self.cpp_info.components["mcengine"].requires = ["mcbase", "boost::boost"]
        self.cpp_info.components["mcengine"].set_property("pkg_config_name", "mcengine")
        self.cpp_info.components["mcengine"].set_property(
            "pkg_config_custom_content", f"libdir=${{prefix}}/{libdir}\n"
        )

        self.cpp_info.components["mcexpr"].libs = ["mcexpr"]
        self.cpp_info.components["mcexpr"].libdirs = [libdir]
        self.cpp_info.components["mcexpr"].includedirs = include_dirs
        self.cpp_info.components["mcexpr"].requires = ["mcbase", "mcengine"]
        self.cpp_info.components["mcexpr"].set_property("pkg_config_name", "mcexpr")
        self.cpp_info.components["mcexpr"].set_property(
            "pkg_config_custom_content", f"libdir=${{prefix}}/{libdir}\n"
        )

        # mcdbus / mcapp 在 use_old_shm=True 时需要 skynet 提供的真实
        # ::shm::* 头（dbus/shm_tree/...）；use_old_shm=False 下走 mock_shm.h，无需 skynet。
        mcdbus_requires = ["mcbase", "mcengine", "boost::boost"]
        mcapp_requires = ["mcbase", "mcengine", "mcdbus"]
        if self.options.use_old_shm:
            mcdbus_requires.append("skynet::skynet")
            mcapp_requires.append("skynet::skynet")

        self.cpp_info.components["mcdbus"].libs = ["mcdbus"]
        self.cpp_info.components["mcdbus"].libdirs = [libdir]
        self.cpp_info.components["mcdbus"].includedirs = include_dirs
        self.cpp_info.components["mcdbus"].requires = mcdbus_requires
        self.cpp_info.components["mcdbus"].set_property("pkg_config_name", "mcdbus")
        self.cpp_info.components["mcdbus"].set_property(
            "pkg_config_custom_content",
            f"libdir=${{prefix}}/{libdir}\n" "Requires: dbus-1 glib-2.0\n",
        )

        self.cpp_info.components["mcapp"].libs = ["mcapp"]
        self.cpp_info.components["mcapp"].libdirs = [libdir]
        self.cpp_info.components["mcapp"].includedirs = include_dirs
        self.cpp_info.components["mcapp"].requires = mcapp_requires
        self.cpp_info.components["mcapp"].set_property("pkg_config_name", "mcapp")
        self.cpp_info.components["mcapp"].set_property(
            "pkg_config_custom_content", f"libdir=${{prefix}}/{libdir}\n"
        )

        self.cpp_info.components["mcpp_base"].libs = ["mcpp_base"]
        self.cpp_info.components["mcpp_base"].libdirs = [libdir]
        self.cpp_info.components["mcpp_base"].includedirs = include_dirs
        self.cpp_info.components["mcpp_base"].requires = [
            "mcbase",
            "mcengine",
            "mcexpr",
            "mcdbus",
            "mcapp",
        ] + external_base_requires
        self.cpp_info.components["mcpp_base"].set_property(
            "pkg_config_name", "mcpp_base"
        )
        self.cpp_info.components["mcpp_base"].set_property(
            "pkg_config_custom_content",
            f"libdir=${{prefix}}/{libdir}\n" "Requires: dbus-1 glib-2.0\n",
        )

        # libmcpp 只声明自身库和组件依赖，不二次包装子库。
        self.cpp_info.components["libmcpp"].libs = ["mcpp"]
        self.cpp_info.components["libmcpp"].libdirs = [libdir]
        self.cpp_info.components["libmcpp"].includedirs = include_dirs
        self.cpp_info.components["libmcpp"].set_property("pkg_config_name", "libmcpp")
        libmcpp_requires = [
            "mcpp_base",
            "mcbase",
            "mcengine",
            "mcexpr",
            "mcdbus",
            "mcapp",
        ]
        # 仅在启用旧 dbus/shm 兼容机制时，libmcpp 才链 skynet 提供的真实 ::shm 实现。
        if self.options.use_old_shm:
            libmcpp_requires.append("skynet::skynet")
        self.cpp_info.components["libmcpp"].requires = libmcpp_requires
        self.cpp_info.components["libmcpp"].set_property(
            "pkg_config_custom_content",
            f"libdir=${{prefix}}/{libdir}\n" "Requires: dbus-1 glib-2.0\n",
        )

        # 配置test_utilities的pkg-config
        self.cpp_info.components["test_utilities"].libs = []
        if self.options.test or self.options.test_utilities:
            self.cpp_info.components["test_utilities"].libs.extend(
                [
                    "mcbase_test_utilities",
                    "mcengine_test_utilities",
                    "mcapp_test_utilities",
                ]
            )
        self.cpp_info.components["test_utilities"].libdirs = [libdir]
        self.cpp_info.components["test_utilities"].includedirs = include_dirs
        self.cpp_info.components["test_utilities"].set_property(
            "pkg_config_name", "test_utilities"
        )
        self.cpp_info.components["test_utilities"].requires = [
            "libmcpp",
            "mcbase",
            "mcengine",
            "mcapp",
        ]
        if self.options.test or self.options.test_utilities:
            self.cpp_info.components["test_utilities"].requires.append("gtest::gtest")
        self.cpp_info.components["test_utilities"].set_property(
            "pkg_config_custom_content",
            f"libdir=${{prefix}}/{libdir}\n" "Requires: libmcpp dbus-1\n",
        )

        if self.options.test:
            # 与 Meson 侧测试目标对齐：先声明组件再挂依赖，避免未定义组件就 append
            self.cpp_info.components["libmcpp_test"].libs = []
            self.cpp_info.components["libmcpp_test"].includedirs = include_dirs
            self.cpp_info.components["libmcpp_test"].requires = [
                "test_utilities",
                "gtest::gtest",
            ]
