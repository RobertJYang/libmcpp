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
            # 为 Debug 类型添加 -Os 优化参数
            d.cpp_args.append("-Os")

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
        # 检查 ninja 工具是否安装，若未安装则自动安装
        import shutil
        import sys
        import subprocess
        if sys.platform.startswith("linux"):
            ninja_path = shutil.which("ninja")
            if ninja_path is None:
                print("[INFO] 未检测到 ninja 工具，正在自动安装...")
                try:
                    subprocess.run(["apt-get", "update"], check=True)
                    subprocess.run(["apt-get", "install", "-y", "ninja-build"], check=True)
                    print("[INFO] ninja 工具安装完成")
                except Exception as e:
                    print(f"[ERROR] 自动安装 ninja 失败: {e}")
                    raise RuntimeError("ninja 工具未安装且自动安装失败，请手动安装 ninja-build")
            else:
                print(f"[INFO] 已检测到 ninja 工具: {ninja_path}")
        #self._codegen()
        meson = Meson(self)
        meson.configure()
        # 智能计算最优并发数量
        optimal_jobs = self._calculate_optimal_jobs()
        print(f"[INFO] 使用 {optimal_jobs} 个并发任务进行编译")
        meson.build(f"-j {optimal_jobs}")
    
    def _calculate_optimal_jobs(self):
        """智能计算最优编译并发数量，参考smart_build.sh的逻辑"""
        import subprocess
        
        try:
            # 获取CPU核数
            cpu_cores = os.cpu_count() or 1
            
            # 获取内存信息
            total_mem_kb = 0
            available_mem_kb = 0
            swap_total_kb = 0
            
            try:
                # 读取内存信息
                with open('/proc/meminfo', 'r') as f:
                    for line in f:
                        if line.startswith('MemTotal:'):
                            total_mem_kb = int(line.split()[1])
                        elif line.startswith('MemAvailable:'):
                            available_mem_kb = int(line.split()[1])
                        elif line.startswith('SwapTotal:'):
                            swap_total_kb = int(line.split()[1])
            except (FileNotFoundError, IOError):
                # 如果无法读取/proc/meminfo，使用保守估计
                print("[WARNING] 无法读取内存信息，使用保守的并发策略")
                return min(cpu_cores // 2, 8)
            
            # 转换为GB
            total_mem_gb = total_mem_kb // 1024 // 1024
            available_mem_gb = available_mem_kb // 1024 // 1024
            swap_total_gb = swap_total_kb // 1024 // 1024
            
            print(f"[INFO] 系统信息: CPU核数={cpu_cores}, 总内存={total_mem_gb}GB, 可用内存={available_mem_gb}GB, Swap={swap_total_gb}GB")
            
            # 检查内存是否足够
            if available_mem_gb < 4:
                print("[WARNING] 可用内存不足4GB，使用最小并发数")
                return 1
            
            # 基于内存的并发数计算
            # C++编译平均每个进程消耗约1-2GB内存，保守估计用2GB
            mem_based_jobs = (available_mem_gb * 2 // 3) // 2  # 使用2/3可用内存，每个任务2GB
            
            # 基于CPU的并发数计算
            cpu_based_jobs = cpu_cores // 4  # 使用一部分CPU核数
            
            # 如果没有swap，更保守一些
            if swap_total_gb == 0:
                mem_based_jobs = (mem_based_jobs * 2) // 4
                print("[WARNING] 系统没有配置swap分区，使用更保守的并发策略")
            
            # 取较小值，但至少为1
            optimal_jobs = min(mem_based_jobs, cpu_based_jobs)
            optimal_jobs = max(optimal_jobs, 1)
            
            # 最大并发数限制（避免过度并发）
            max_jobs = 24
            optimal_jobs = min(optimal_jobs, max_jobs)
            
            # 显示计算过程
            print(f"[INFO] 计算结果: 基于内存={mem_based_jobs}, 基于CPU={cpu_based_jobs}, 最终选择={optimal_jobs}")
            
            return optimal_jobs
            
        except Exception as e:
            print(f"[WARNING] 计算并发数时出错: {e}，使用默认值8")
            return 8

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
        
        # 对Debug模式编译生成的文件进行strip处理
        if self.settings.build_type == "Debug":
            self._strip_debug_files()
        
        if self.settings.build_type in ("Dt", ) and os.getenv('TRANSTOBIN') is None:
            return
        os.chdir(self.package_folder)

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
                                if ("executable" in file_type or 
                                    "shared object" in file_type or 
                                    "dynamically linked" in file_type):
                                    
                                    # 执行strip操作
                                    try:
                                        subprocess.run([strip_tool, "-s", file_path], 
                                                     check=True, timeout=30)
                                        print(f"Stripped debug symbols from: {file_path}")
                                    except subprocess.CalledProcessError as e:
                                        print(f"Warning: Failed to strip {file_path}: {e}")
                                    except subprocess.TimeoutExpired:
                                        print(f"Warning: Strip timeout for {file_path}")
                    except Exception as e:
                        print(f"Warning: Error processing {file_path}: {e}")
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
        
        # 配置libmcpp的pkg-config
        self.cpp_info.components["libmcpp"].libs = ["libmcpp"]
        self.cpp_info.components["libmcpp"].libdirs = ["usr/lib64"]
        self.cpp_info.components["libmcpp"].system_libs = ["dbus-1", "glib-2.0", "boost_program_options", "somp", "logging"]
        self.cpp_info.components["libmcpp"].set_property("pkg_config_name", "libmcpp")
        self.cpp_info.components["libmcpp"].set_property("pkg_config_custom_content", 
           f"libdir=${{prefix}}/{libdir}\n"
           "Requires: dbus-1 glib-2.0\n")
        
        # 配置test_utilities的pkg-config
        self.cpp_info.components["test_utilities"].libs = ["mc_test_utilities"]
        self.cpp_info.components["test_utilities"].libdirs = ["usr/lib64"]
        self.cpp_info.components["test_utilities"].system_libs = ["dbus-1", "boost_program_options", "somp", "logging"]
        self.cpp_info.components["test_utilities"].set_property("pkg_config_name", "test_utilities")
        self.cpp_info.components["test_utilities"].set_property("pkg_config_custom_content",
           f"libdir=${{prefix}}/{libdir}\n"
           "Requires: dbus-1\n")
        
        