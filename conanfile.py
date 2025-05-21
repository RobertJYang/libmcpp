import os
from conanbase import ConanBase
from conan.tools.meson import Meson, MesonToolchain, MesonDeps
from conan.tools.gnu import PkgConfigDeps

class AppConan(ConanBase):
    language = "c++"

    def package_info(self):
        pass

# 基于meson构建的基类，适用于libmcpp项目
    def layout(self):
        self.folders.source = '.'
        self.folders.build = "builddir"

    def generate(self):
        d = MesonDeps(self)
        if self.settings.build_type != "Dt":
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
        tc.project_options["enable_shared_memory"] = "true"

        if self.settings.arch in ["armv8"]:
            tc.properties["pkg_config_libdir"] = self.env["PKG_CONFIG_PATH"].split(":")
        tc.generate()
        pc = PkgConfigDeps(self)
        pc.generate()
    
    def _configure_meson(self):
        # 设置meson构建选项
        defs = {}
        if self.settings.build_type == "Dt":
            defs["tests"] = "true"
            self.settings.build_type = "Debug"
        else:
            defs["tests"] = "false"
            
        defs["shared_mutex_max_readers"] = "0"
      
    def build(self):
        if self.language != "c++":
            return
        #self._codegen()
        self._configure_meson()
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        self._configure_meson()
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
        else:
            self.cpp_info.libdirs = ["usr/lib"]
            self.env_info.LD_LIBRARY_PATH.append(os.path.join(self.package_folder, "usr/lib"))
        self.env_info.PATH.append(os.path.join(self.package_folder, "opt/bmc/apps/libmcpp"))
        # 如果使用 pkgconfig
        self.cpp_info.set_property("pkg_config_custom_content", 
           "libdir=${prefix}/usr/lib64\n")
        

        
        