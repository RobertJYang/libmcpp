import os
from conanbase import ConanBase
from conan.tools.meson import Meson, MesonToolchain
from conan.tools.env import VirtualBuildEnv
from conan.tools.files import copy

required_conan_version = ">=2.13.0"


class AppConan(ConanBase):
    language = "c++"
    generators = "CMakeDeps", "VirtualBuildEnv", "PkgConfigDeps"

# 基于meson构建的基类，适用于libmcpp项目
    def layout(self):
        self.folders.source = '.'
        self.folders.build = "builddir"

    def requirements(self):
        super().requirements()
        if self.options.test:
            self.requires("gtest/[>=1.14.0]@openubmc/stable")

    def generate(self):
        os.environ["PKG_CONFIG"] = "/usr/bin/pkg-config"
        tc = MesonToolchain(self, "ninja")
        if self.settings.arch == "armv8" or self.settings.arch == "x86_64":
            tc.project_options["libdir"] = 'usr/lib64'
        else:
            tc.project_options["libdir"] = 'usr/lib'
        tc.project_options["enable_conan_compile"] = True
        if self.options.test:
            tc.project_options["tests"] = True
        else:
            tc.project_options["tests"] = False
        tc.project_options["meson_build"] = False

        ms = VirtualBuildEnv(self)
        if self.settings.arch in ["armv8"]:
            tc.properties["pkg_config_libdir"] = ms.vars().get("PKG_CONFIG_PATH").split(":")

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
        compiler_version = str(self.settings.compiler.version) if self.settings.compiler.version else None
        if compiler == "gcc" and compiler_version:
            # GCC 9.0+ 支持该选项
            try:
                major_version = int(compiler_version.split('.')[0])
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
        meson.build()

    def package(self):
        meson = Meson(self)
        meson.install()

        if os.path.isfile("permissions.ini"):
            copy(self, "permissions.ini")
        copy(self, "*", src=os.path.join(self.source_folder, "mds"), dst=os.path.join(self.package_folder, "include/mds"))
        if os.path.isdir("include"):
            copy(self, "*", src=os.path.join(self.source_folder, "include"), dst=os.path.join(self.package_folder, "include"))
        if os.path.isdir("mds"):
            copy(self, "*", src=os.path.join(self.source_folder, "mds"), dst=os.path.join(self.package_folder, "usr/share/doc/openubmc/libmcpp/mds"))
        if os.path.isdir("docs"):
            copy(self, "*", src=os.path.join(self.source_folder, "docs"), dst=os.path.join(self.package_folder, "usr/share/doc/openubmc/libmcpp/docs"))
        copy(self, "*.md", src=self.source_folder, dst=os.path.join(self.package_folder, "usr/share/doc/openubmc/libmcpp/docs"))
        copy(self, "*.MD", src=self.source_folder, dst=os.path.join(self.package_folder, "usr/share/doc/openubmc/libmcpp/docs"))

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
                            result = subprocess.run(["file", file_path],
                                                  capture_output=True, text=True, timeout=10)
                            if result.returncode == 0:
                                file_type = result.stdout.lower()
                                if ("executable" in file_type or "current ar archive" in file_type or
                                    "shared object" in file_type or
                                    "dynamically linked" in file_type):
                                    # 执行strip操作
                                    # 对于静态库，不要使用-s参数，因为会移除符号索引
                                    if "current ar archive" in file_type:
                                        self.run(f"{strip_tool} --strip-unneeded {file_path}")
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
                            result = subprocess.run(["file", file_path],
                                                  capture_output=True, text=True, timeout=10)
                            if result.returncode == 0:
                                file_type = result.stdout.lower()
                                # 检查是否为ar archive格式的静态库
                                if "current ar archive" in file_type:
                                    # 使用ranlib创建符号索引
                                    ranlib_result = subprocess.run(["ranlib", file_path],
                                                                  capture_output=True, text=True, timeout=30)
                                    if ranlib_result.returncode == 0:
                                        print(f"Successfully ran ranlib on {file_path}")
                                    else:
                                        print(f"Warning: Failed to run ranlib on {file_path}: {ranlib_result.stderr.strip()}")
                    except subprocess.TimeoutExpired:
                        print(f"Warning: Timeout checking file type for {file_path}")
                    except Exception as e:
                        print(f"Warning: Exception processing {file_path}: {str(e)}")
                        continue

    def package_info(self):
        if self.settings.arch == "armv8" or self.settings.arch == "x86_64":
            self.cpp_info.libdirs = ["usr/lib64"]
            self.env_info.LD_LIBRARY_PATH.append(os.path.join(self.package_folder, "usr/lib64"))
            libdir = "usr/lib64"
        else:
            self.cpp_info.libdirs = ["usr/lib"]
            self.env_info.LD_LIBRARY_PATH.append(os.path.join(self.package_folder, "usr/lib"))
            libdir = "usr/lib"

        self.env_info.PATH.append(os.path.join(self.package_folder, "opt/bmc/apps/libmcpp"))

        include_dirs = ["include"]
        self.cpp_info.includedirs = include_dirs

        # 配置libmcpp的pkg-config
        self.cpp_info.components["libmcpp"].libs = ["libmcpp"]
        self.cpp_info.components["libmcpp"].libdirs = ["usr/lib64"]
        self.cpp_info.components["libmcpp"].includedirs = include_dirs
        self.cpp_info.components["libmcpp"].set_property("pkg_config_name", "libmcpp")
        self.cpp_info.components["libmcpp"].requires = ["libsomp::libsomp", "liblogger::liblogger", "boost::boost", "skynet::skynet", "json::json", "huawei_secure_c::securec"]
        self.cpp_info.components["libmcpp"].set_property("pkg_config_custom_content",
           f"libdir=${{prefix}}/{libdir}\n"
           "Requires: dbus-1 glib-2.0\n")

        # 配置test_utilities的pkg-config
        self.cpp_info.components["test_utilities"].libs = ["mc_test_utilities"]
        self.cpp_info.components["test_utilities"].libdirs = ["usr/lib64"]
        self.cpp_info.components["test_utilities"].includedirs = include_dirs
        self.cpp_info.components["test_utilities"].set_property("pkg_config_name", "test_utilities")
        self.cpp_info.components["test_utilities"].set_property("pkg_config_custom_content",
           f"libdir=${{prefix}}/{libdir}\n"
           "Requires: dbus-1\n")

        if self.options.test:
            self.cpp_info.components["libmcpp_test"].requires.append("gtest::gtest")