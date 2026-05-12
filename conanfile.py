import json
import os
import sys
import time
from conanbase import ConanBase
from conan.tools.meson import Meson, MesonToolchain
from conan.tools.env import VirtualBuildEnv
from conan.tools.files import copy, update_conandata
from conan.tools.scm import Git

required_conan_version = ">=2.13.0"


class AppConan(ConanBase):
    language = "c++"
    generators = "CMakeDeps", "VirtualBuildEnv", "PkgConfigDeps"
    exports_sources = ()
    _source_excludes = (
        ".git",
        ".git/*",
        ".gitignore",
        ".gitmodules",
        ".vscode",
        ".vscode/*",
        ".idea",
        ".idea/*",
        "__pycache__",
        "__pycache__/*",
        "*/__pycache__/*",
        "*.pyc",
        "*.pyo",
        "build",
        "build/*",
        "builddir*",
        "builddir*/*",
        ".build",
        ".build/*",
        "temp",
        "temp/*",
        ".cache",
        ".cache/*",
        ".mclang",
        ".mclang/*",
        ".mcli",
        ".mcli/*",
        "*.log",
        "bingo-*.txt",
        "coverage.info",
        "coverage_filtered.info",
        ".ccache_wrapper",
        ".pkg-config",
        ".DS_Store",
    )
    options = {
        "asan": [True, False],
        "gcov": [True, False],
        "test": [True, False],
        "test_utilities": [True, False],
        "examples": [True, False],
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
        "test_utilities": False,
        "examples": False,
        "enable_luajit": True,
        # conan 构建（DT / 交叉编译生产）默认走旧架构，保持与存量部署兼容；
        # 想在 conan 环境下试新架构：-o use_shm=True -o use_old_shm=False。
        "use_shm": False,
        "use_old_shm": True,
    }

    # 基于meson构建的基类，适用于libmcpp项目
    def layout(self):
        super().layout()

    def package_id(self):
        # test/examples 是本地构建开关，但也会启用打包的 test utilities。
        # package_id 按最终包内容归一化，避免等价配置生成不同二进制 ID。
        build_test_utilities = (
            self._option_value_enabled(self.info.options.get_safe("test", False))
            or self._option_value_enabled(
                self.info.options.get_safe("test_utilities", False)
            )
            or self._option_value_enabled(self.info.options.get_safe("examples", False))
        )
        self.info.options.rm_safe("test")
        self.info.options.rm_safe("examples")
        self.info.options.test_utilities = build_test_utilities

        # Runtime dependencies have their own test/jit binary variants. Keep
        # libmcpp tied to dependency versions, not to their package IDs/options.
        self.info.requires.semver_mode()

    def _option_value_enabled(self, value):
        return str(value).lower() in ("true", "1", "yes", "on")

    def _option_enabled(self, option_name):
        value = self.options.get_safe(option_name, False)
        return self._option_value_enabled(value)

    def _build_test_utilities(self):
        return (
            # 依赖场景下也需要随着 test=True 一起产出 test utilities，
            # 但是否编译 libmcpp 自己的测试目标仍由 _build_own_tests() 单独控制。
            self._option_enabled("test")
            or self._option_enabled("test_utilities")
            or self._build_examples()
        )

    def _build_examples(self):
        return self._option_enabled("examples")

    def requirements(self):
        super().requirements()
        if self._build_test_utilities():
            self.requires("gtest/[>=1.14.0]@openubmc/stable")

    def export(self):
        super_export = getattr(super(), "export", None)
        if super_export:
            super_export()
        else:
            copy(self, "conanbase.py", self.recipe_folder, self.export_folder)

        git = Git(self, self.recipe_folder)
        try:
            message = git.run("log --no-merges -n 1")
            commit = git.get_commit()
        except Exception:
            self._export_local_source(message="")
            return

        if git.is_dirty():
            self._export_local_source(message=message)
            return

        remote_url = self._find_remote_url(git, commit)
        if not remote_url:
            self._export_local_source(message=message)
            return

        update_conandata(
            self,
            {"sources": {self.version: {"branch": commit, "url": remote_url, "message": message}}},
        )

    def _find_remote_url(self, git, commit):
        try:
            remotes = git.run("remote")
        except Exception:
            return None

        for remote in remotes.splitlines():
            try:
                if git.run(f"ls-remote {remote} {commit}").strip():
                    return git.get_remote_url(remote)
            except Exception:
                continue
        return None

    def _export_local_source(self, message):
        update_conandata(
            self,
            {
                "sources": {
                    self.version: {
                        "branch": None,
                        "url": None,
                        "pwd": os.getcwd(),
                        "timestamp": int(time.time()),
                        "message": message,
                    }
                }
            },
        )

    def _active_project_name(self):
        for index, arg in enumerate(sys.argv):
            if arg == "--name" and index + 1 < len(sys.argv):
                return sys.argv[index + 1].lower()
            if arg.startswith("--name="):
                return arg.split("=", 1)[1].lower()

        workspace = os.environ.get("PWD", "")
        return self._project_name_from_service(workspace)

    def _project_name_from_service(self, workspace):
        if not workspace:
            return None

        service_json = os.path.join(workspace, "mds", "service.json")
        if not os.path.isfile(service_json):
            return None

        try:
            with open(service_json, encoding="utf-8") as file:
                return json.load(file).get("name", "").lower()
        except (OSError, json.JSONDecodeError):
            return None

    def _build_own_tests(self):
        recipe_project_name = (
            (str(self.name).lower() if self.name else None)
            or self._project_name_from_service(self.recipe_folder)
            or self._project_name_from_service(self.source_folder)
        )
        return bool(self.options.test) and self._active_project_name() == recipe_project_name

    def _meson_bool(self, value):
        return "true" if value else "false"

    def _meson_reconfigure_args(self):
        """为已有 builddir 显式同步会漂移的 Meson 选项，避免清空缓存。"""
        build_tests = self._build_own_tests()
        build_test_utilities = self._build_test_utilities()
        args = [
            f"-Dtests={self._meson_bool(build_tests)}",
            f"-Dtests_utilities={self._meson_bool(build_test_utilities)}",
            f"-Dexamples={self._meson_bool(self._build_examples())}",
            f"-Duse_shm={self._meson_bool(bool(self.options.use_shm))}",
            f"-Duse_old_shm={self._meson_bool(bool(self.options.use_old_shm))}",
        ]
        for subproject_name in ("mcbase", "mcengine", "mcexpr", "mcdbus", "mcapp"):
            args.append(
                f"-D{subproject_name}:tests={self._meson_bool(build_tests)}"
            )
        for subproject_name in ("mcbase", "mcengine", "mcapp"):
            args.append(
                f"-D{subproject_name}:tests_utilities={self._meson_bool(build_test_utilities)}"
            )
        args.extend(
            [
                f"-Dmcengine:use_shm={self._meson_bool(bool(self.options.use_shm))}",
                f"-Dmcdbus:use_old_shm={self._meson_bool(bool(self.options.use_old_shm))}",
                f"-Dmcapp:use_old_shm={self._meson_bool(bool(self.options.use_old_shm))}",
            ]
        )
        return args

    def _reconfigure_existing_build(self):
        configure_args = " ".join(self._meson_reconfigure_args())
        self.run(f'meson configure "{self.build_folder}" {configure_args}')

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
        build_tests = self._build_own_tests()
        build_test_utilities = self._build_test_utilities()
        tc.project_options["tests"] = build_tests
        tc.project_options["tests_utilities"] = build_test_utilities
        # Meson subproject default_options only apply when a subproject is first
        # configured. Pass explicit subproject options so incremental mcli test
        # runs do not silently leave library tests disabled.
        for subproject_name in ("mcbase", "mcengine", "mcexpr", "mcdbus", "mcapp"):
            tc.subproject_options.setdefault(subproject_name, []).append({"tests": build_tests})
        for subproject_name in ("mcbase", "mcengine", "mcapp"):
            tc.subproject_options.setdefault(subproject_name, []).append({"tests_utilities": build_test_utilities})
        tc.project_options["examples"] = self._build_examples()
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
        if os.path.exists(os.path.join(self.build_folder, "meson-private", "coredata.dat")):
            self._reconfigure_existing_build()
        else:
            meson.configure()
        meson.build()
        if self._build_own_tests():
            self.test()

    def test(self):
        """运行 Meson 测试"""
        import subprocess

        filter_arg = os.environ.get("MCLI_TEST_FILTER")
        verbose = os.environ.get("MCLI_TEST_VERBOSE")

        cmd = ["meson", "test", "-v", "-C", self.build_folder]

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
        if self._build_test_utilities() and os.path.isdir(mcengine_test_include):
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
            libdir = "usr/lib64"
        else:
            libdir = "usr/lib"

        self.cpp_info.libdirs = [libdir]

        # conan 2.x：用 runenv_info 替代已 deprecated 的 env_info；
        # 语义等价于原 env_info.LD_LIBRARY_PATH/PATH.append，但走的是 conan 2 原生
        # 的 VirtualRunEnv 注入路径，避免 "WARN: deprecated 'env_info' used in ..."
        self.runenv_info.append_path(
            "LD_LIBRARY_PATH", os.path.join(self.package_folder, libdir)
        )
        self.runenv_info.append_path(
            "PATH", os.path.join(self.package_folder, "opt/bmc/apps/libmcpp")
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
        self.cpp_info.components["mcpp_base"].system_libs = ["z", "ssl", "crypto"]
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

        # 聚合测试辅助组件：上层测试只依赖 libmcpp::test_utilities 即可，
        # 不需要手动展开 mcbase/mcengine/mcapp 各层 test utilities。
        self.cpp_info.components["test_utilities"].libs = []
        build_own_tests = self._build_own_tests()
        build_test_utilities = self._build_test_utilities()
        if build_test_utilities:
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
        if build_test_utilities:
            self.cpp_info.components["test_utilities"].requires.append("gtest::gtest")
        self.cpp_info.components["test_utilities"].set_property(
            "pkg_config_custom_content",
            f"libdir=${{prefix}}/{libdir}\n" "Requires: libmcpp dbus-1\n",
        )

        if build_own_tests:
            # 与 Meson 侧测试目标对齐：先声明组件再挂依赖，避免未定义组件就 append
            self.cpp_info.components["libmcpp_test"].libs = []
            self.cpp_info.components["libmcpp_test"].includedirs = include_dirs
            self.cpp_info.components["libmcpp_test"].requires = [
                "test_utilities",
                "gtest::gtest",
            ]
