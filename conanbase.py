import os
import stat
import urllib3
from conans import ConanFile, CMake
from bmcgo.component.gen import GenComp

urllib3.disable_warnings()

# 构建时由工具自动生成到业务仓，做为conanfile.py的基类参与组件构建
# 如需要调试，请修改模板文件（目录中存在python版本号、bmcgo集成开发环境应用名称，请适配）：
#   ~/.local/lib/python3.8/site-packages/bmcgo/component/template/conanbase.py.mako


class ConanBase(ConanFile):
    name = "libmcpp"
    version = "0.0.1"
    settings = "os", "compiler", "build_type", "arch"
    license = "Mulan PSL v2"
    generators = "cmake"
    language = "lua"
    _cmake = None
    _codegen_version = 14
    scm = {
        "type": "git",
        "url": "https://gitcode.com/cardm/libmcpp.git",
        "revision": "auto"
    }
    options = {
        "asan": [True, False],
        "gcov": [True, False],
        "manufacture": [True, False],
    }
    default_options = {
        "asan": False,
        "gcov": False,
        "manufacture": False,
    }

    def generate(self):
        file_descriptor = os.open("conan_toolchain.cmake", os.O_RDWR | os.O_CREAT, stat.S_IWUSR | stat.S_IRUSR)
        file_handler = os.fdopen(file_descriptor, "w")
        file_handler.write("add_compile_definitions(\"_FORTIFY_SOURCE=2\")\n")
        file_handler.close()

    @property
    def _source_subfolder(self):
        return "source_subfolder"

    @property
    def _build_subfolder(self):
        return "build_subfolder"

    def export(self):
        self.copy("mds/service.json")
        self.copy("conanbase.py")


    def requirements(self):
        uc = ""
        if self.user and self.channel:
            if self.channel == "dev":
                uc = f"@{self.user}/rc"
            else:
                uc = f"@{self.user}/{self.channel}"
        # 编译依赖
        self.requires("boost/1.82.0.B001@openUBMC.release/rc")


    def _codegen(self):
        args = ["-s", "mds/service.json"]
        gen = GenComp(args)
        gen.run(self._codegen_version)

    def _new_cmake(self):
        return CMake(self)

    def _configure_cmake(self):
        if self._cmake:
            return self._cmake
        self._cmake = self._new_cmake()
        self._cmake.definitions["BUILD_MANUFACTURE"] = self.options.manufacture
        if self.settings.build_type == "Dt":
            self._cmake.definitions["ENABLE_TEST"] = "ON"

        # 向CMAKE传递版本号信息
        version = self.version.split(".")
        if len(version) >= 1:
            self._cmake.definitions["PACKAGE_VERSION_MAJOR"] = version[0]
        if len(version) >= 2:
            self._cmake.definitions["PACKAGE_VERSION_MINOR"] = version[1]
        if len(version) >= 3:
            self._cmake.definitions["PACKAGE_VERSION_REVISION"] = version[2]
        # 设置额外编译选项或者重定义CFLAGS CXXFLAGS,也可以设置其他开关
        # 示例: os.environ['CFLAGS'] = f"{os.getenv('CFLAGS')} -fPIE"

        if self.settings.arch == "armv8" or self.settings.arch == "x86_64":
            self._cmake.definitions["CMAKE_INSTALL_LIBDIR"] = "usr/lib64"
        else:
            self._cmake.definitions["CMAKE_INSTALL_LIBDIR"] = "usr/lib"

        if self.options.asan:
            print("Enable asan flags")
            flag = " -fsanitize=address -fsanitize-recover=address,all -fno-omit-frame-pointer -fno-stack-protector -O0"
            os.environ['CFLAGS'] = os.getenv('CFLAGS') + flag
            os.environ['CXXFLAGS'] = os.getenv('CXXFLAGS') + flag
        if self.options.gcov:
            print("Enable gcov flags")
            os.environ['CFLAGS'] = f"{os.getenv('CFLAGS')} -ftest-coverage -fprofile-arcs"
            os.environ['CXXFLAGS'] = f"{os.getenv('CXXFLAGS')} -ftest-coverage -fprofile-arcs"

        # 配合generate添加宏定义
        self._cmake.definitions["CMAKE_TOOLCHAIN_FILE"] = "conan_toolchain.cmake"
        # rpath配置
        self._cmake.definitions["CMAKE_SKIP_BUILD_RPATH"] = True
        self._cmake.definitions["CMAKE_SKIP_RPATH"] = True
        self._cmake.definitions["CMAKE_SKIP_INSTALL_RPATH"] = True
        self._cmake.definitions["CMAKE_BUILD_WITH_INSTALL_RPATH"] = False
        self._cmake.definitions["CMAKE_INSTALL_RPATH_USE_LINK_PATH"] = False
        self._cmake.configure(args=["--no-warn-unused-cli"])
        return self._cmake

    def build(self):
        if self.language != "c":
            return
        self._codegen()
        cmake = self._configure_cmake()
        cmake.build()

    def package(self):
        cmake = self._configure_cmake()
        cmake.install()
        if os.path.isfile("permissions.ini"):
            self.copy("permissions.ini")
        if os.path.isfile("mds/model.json"):
           self.copy("model.json", src="mds", dst="include/mds")
        if os.path.isfile("mds/service.json"):
           self.copy("service.json", src="mds", dst="include/mds")
        if os.path.isdir("build"):
            self.copy("*", src="build", dst="include")
        if os.path.isdir("mds"):
            self.copy("*", src="mds", dst="usr/share/doc/openubmc/libmcpp/mds")
        if os.path.isdir("docs"):
            self.copy("*", src="docs", dst="usr/share/doc/openubmc/libmcpp/docs")
        self.copy("*.md", dst="usr/share/doc/openubmc/libmcpp/docs")
        self.copy("*.MD", dst="usr/share/doc/openubmc/libmcpp/docs")
        if self.settings.build_type in ("Dt", ) and os.getenv('TRANSTOBIN') is None:
            return
        os.chdir(self.package_folder)
        for root, dirs, files in os.walk("."):
            for file in files:
                if not file.endswith(".lua") or os.path.islink(file):
                    continue
                file_path = os.path.join(root, file)
                self.run(f"{os.path.expanduser('~')}/.conan/bin/luac -o {file_path} {file_path}")
                self.run(f"{os.path.expanduser('~')}/.conan/bin/luac -s {file_path}")

    def package_info(self):
        if self.settings.arch == "armv8" or self.settings.arch == "x86_64":
            self.cpp_info.libdirs = ["usr/lib64"]
            self.env_info.LD_LIBRARY_PATH.append(os.path.join(self.package_folder, "usr/lib64"))
        else:
            self.cpp_info.libdirs = ["usr/lib"]
            self.env_info.LD_LIBRARY_PATH.append(os.path.join(self.package_folder, "usr/lib"))
        self.env_info.PATH.append(os.path.join(self.package_folder, "opt/bmc/apps/libmcpp"))
