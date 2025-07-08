import os
from conanbase import ConanBase
from conan.tools.meson import Meson, MesonToolchain, MesonDeps
from conan.tools.gnu import PkgConfigDeps

class AppConan(ConanBase):
    language = "c++"

# 基于meson构建的基类，适用于libmcpp项目
    def layout(self):
        self.folders.source = '.'
        self.folders.build = "builddir"

    def generate(self):
        d = MesonDeps(self)
        is_dt = self.settings.build_type == "Dt"
        if not is_dt:
            d.cpp_link_args.append("-Bstatic")
            d.cpp_link_args.append("-lboost_program_options")
        else:
            self.settings.build_type = "Debug"

        d.cpp_link_args.append("-lstdc++fs")
        d.cpp_args.append("-Wno-unused-variable")
        d.cpp_args.append("-Wno-unused-parameter")
        d.cpp_args.append("-Wno-sign-compare")
        d.cpp_args.append("-fpermissive")
        d.cpp_args.append("-Wno-pedantic")
        d.cpp_args.append("-fno-strict-aliasing")
        d.cpp_args.append("-Wno-deprecated-copy")
        d.generate()

        os.environ["PKG_CONFIG"] = "/usr/bin/pkg-config"
        
        tc = MesonToolchain(self, "ninja")
        if self.settings.arch == "armv8" or self.settings.arch == "x86_64":
            tc.project_options["libdir"] = 'usr/lib64'
        else:
            tc.project_options["libdir"] = 'usr/lib'
        tc.project_options["enable_conan_compile"] = "true"
        if is_dt:
            tc.project_options["tests"] = "true"
        else:
            tc.project_options["tests"] = "false"
        tc.project_options["meson_build"] = "false"

        if self.settings.arch in ["armv8"]:
            tc.properties["pkg_config_libdir"] = self.env["PKG_CONFIG_PATH"].split(":")
        tc.generate()
        pc = PkgConfigDeps(self)
        pc.generate()
    
      
    def build(self):
        if self.language != "c++":
            return
        #self._codegen()
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        meson = Meson(self)
        meson.install()
        if os.path.isfile("permissions.ini"):
            self.copy("permissions.ini")
        if os.path.isfile("mds/model.json"):
           self.copy("model.json", src="mds", dst="include/mds")
        if os.path.isfile("mds/service.json"):
           self.copy("service.json", src="mds", dst="include/mds")
        if os.path.isdir("include"):
            self.copy("*", src="include", dst="include")
        if os.path.isdir("mds"):
            self.copy("*", src="mds", dst="usr/share/doc/openubmc/libmcpp/mds")
        if os.path.isdir("docs"):
            self.copy("*", src="docs", dst="usr/share/doc/openubmc/libmcpp/docs")
        self.copy("*.md", dst="usr/share/doc/openubmc/libmcpp/docs")
        self.copy("*.MD", dst="usr/share/doc/openubmc/libmcpp/docs")
        if self.settings.build_type in ("Dt", ) and os.getenv('TRANSTOBIN') is None:
            return
        os.chdir(self.package_folder)

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
        
        # 配置libmcpp的pkg-config
        self.cpp_info.components["libmcpp"].libs = ["libmcpp"]
        self.cpp_info.components["libmcpp"].libdirs = ["usr/lib64"]
        self.cpp_info.components["libmcpp"].system_libs = ["dbus-1", "glib-2.0", "boost_program_options", "somp"]
        self.cpp_info.components["libmcpp"].set_property("pkg_config_name", "libmcpp")
        self.cpp_info.components["libmcpp"].set_property("pkg_config_custom_content", 
           f"libdir=${{prefix}}/{libdir}\n"
           "Requires: dbus-1 glib-2.0\n")
        
        # 配置test_utilities的pkg-config
        self.cpp_info.components["test_utilities"].libs = ["mc_test_utilities"]
        self.cpp_info.components["test_utilities"].libdirs = ["usr/lib64"]
        self.cpp_info.components["test_utilities"].system_libs = ["dbus-1", "boost_program_options", "somp"]
        self.cpp_info.components["test_utilities"].set_property("pkg_config_name", "test_utilities")
        self.cpp_info.components["test_utilities"].set_property("pkg_config_custom_content",
           f"libdir=${{prefix}}/{libdir}\n"
           "Requires: dbus-1\n")
        
        