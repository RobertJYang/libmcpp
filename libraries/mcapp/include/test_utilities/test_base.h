/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_LIBMCPP_TEST_BASE_H
#define MC_LIBMCPP_TEST_BASE_H

#include <mc/app/application.h>
#if defined(MCDBUS_USE_OLD_SHM) && MCDBUS_USE_OLD_SHM
#include <cstdlib>
#include <dirent.h>
#include <mc/dbus/shm/harbor.h>
#include <mc/filesystem.h>
#include <sys/statvfs.h>
#include <system_error>
#endif
#include <mc/singleton.h>

#include <test_utilities/base.h>
#include <test_utilities/dbus_daemon_manager.h>
#include <test_utilities/engine_test_base.h>

namespace mc {
namespace test {

class MC_API TestWithDbusDaemon : public TestWithEngine {
protected:
    static dbus_daemon_manager& get_dbus_daemon()
    {
        return mc::singleton<dbus_daemon_manager>::instance();
    }

    static void SetUpTestSuite()
    {
#if defined(MCDBUS_USE_OLD_SHM) && MCDBUS_USE_OLD_SHM
        mc::dbus::harbor::reset_for_test();
        init_test_shared_memory();
#endif
        TestWithEngine::SetUpTestSuite();
        ASSERT_TRUE(get_dbus_daemon().start()) << "启动 DBus 守护进程失败";
    }

    static void TearDownTestSuite()
    {
#if defined(MCDBUS_USE_OLD_SHM) && MCDBUS_USE_OLD_SHM
        mc::dbus::harbor::reset_for_test();
        destroy_test_shared_memory();
#endif
        TestWithEngine::TearDownTestSuite();
    };

#if defined(MCDBUS_USE_OLD_SHM) && MCDBUS_USE_OLD_SHM
private:
    static void configure_test_shared_memory_size()
    {
        constexpr const char* test_shm_size = "16777216";
        if (std::getenv("MDBUS_SHM_SIZE") == nullptr) {
            setenv("MDBUS_SHM_SIZE", test_shm_size, 1);
        }
        ilog("[shm_diag] MDBUS_SHM_SIZE=${size}", ("size", std::getenv("MDBUS_SHM_SIZE")));
    }

    static void init_test_shared_memory()
    {
        // 先清理上次测试可能残留的共享内存
        destroy_test_shared_memory();
        configure_test_shared_memory_size();

        // 诊断：探测 /dev/shm 的空间和状态
        struct statvfs vfs {};
        if (statvfs("/dev/shm", &vfs) == 0) {
            auto total_mb = static_cast<unsigned long>(vfs.f_blocks * vfs.f_frsize) / (1024 * 1024);
            auto free_mb  = static_cast<unsigned long>(vfs.f_bfree * vfs.f_frsize) / (1024 * 1024);
            ilog("[shm_diag] /dev/shm space: total=${total_mb}MB, free=${free_mb}MB, "
                 "block_size=${block_size}",
                 ("total_mb", total_mb)("free_mb", free_mb)("block_size", vfs.f_bsize));
        } else {
            elog("[shm_diag] statvfs /dev/shm failed: ${error}", ("error", strerror(errno)));
        }

        // 诊断：列出 /dev/shm 中已有的文件
        {
            DIR* dir = opendir("/dev/shm");
            if (dir) {
                std::string    files;
                struct dirent* entry = nullptr;
                int            count = 0;
                while ((entry = readdir(dir)) != nullptr && count < 50) {
                    if (entry->d_name[0] == '.') {
                        continue;
                    }
                    if (!files.empty()) {
                        files += ", ";
                    }
                    files += entry->d_name;
                    count++;
                }
                closedir(dir);
                ilog("[shm_diag] /dev/shm existing files (${count}): ${files}", ("count", count)("files", files));
            } else {
                elog("[shm_diag] opendir /dev/shm failed: ${error}", ("error", strerror(errno)));
            }
        }

        ilog("[shm_diag] before init: /dev/shm exists=${exists}, "
             "init_shm.lock exists=${lock_exists}, "
             "MDBUS_SHM exists=${shm_exists}, "
             "is_exist=${is_exist}",
             ("exists", mc::filesystem::exists("/dev/shm"))("lock_exists",
                                                            mc::filesystem::exists("/dev/shm/init_shm.lock"))(
                 "shm_exists", mc::filesystem::exists("/dev/shm/MDBUS_SHM"))("is_exist",
                                                                             ::shm::shared_memory::is_exist()));

        // 创建 /dev/shm 目录（如不存在）
        mc::filesystem::create_directories("/dev/shm");

        // 创建 init_shm.lock 文件，模拟 framework 进程的初始化
        const char* lock_path = "/dev/shm/init_shm.lock";
        FILE*       fp        = fopen(lock_path, "w");
        if (fp) {
            fclose(fp);
            ilog("[shm_diag] created init_shm.lock successfully");
        } else {
            elog("[shm_diag] failed to create init_shm.lock: ${error}", ("error", strerror(errno)));
        }

        // 触发 shared_memory::get_instance()，open_or_create 会自动创建共享内存段
        // 不 catch 异常：如果共享内存创建失败，异常会传播到 SetUpTestSuite()，
        // gtest 会将该测试套件的所有测试标记为 SKIPPED
        ilog("[shm_diag] calling shared_memory::get_instance()...");
        ::shm::shared_memory::get_instance();
        ilog(
            "[shm_diag] shared_memory::get_instance() succeeded, "
            "is_exist=${is_exist}, "
            "MDBUS_SHM exists=${shm_exists}",
            ("is_exist", ::shm::shared_memory::is_exist())("shm_exists", mc::filesystem::exists("/dev/shm/MDBUS_SHM")));
    }

    static void destroy_test_shared_memory()
    {
        ilog("[shm_diag] destroy_test_shared_memory: "
             "is_exist=${is_exist}, "
             "MDBUS_SHM exists=${shm_exists}, "
             "init_shm.lock exists=${lock_exists}",
             ("is_exist", ::shm::shared_memory::is_exist())("shm_exists", mc::filesystem::exists("/dev/shm/MDBUS_SHM"))(
                 "lock_exists", mc::filesystem::exists("/dev/shm/init_shm.lock")));

        try {
            ::shm::shared_memory::destory();
        } catch (...) {
        }

        mc::filesystem::remove("/dev/shm/MDBUS_SHM_LOCK_MANAGER");
        mc::filesystem::remove("/dev/shm/MDBUS_SHM_MUTEX_MAMAGER");
        mc::filesystem::remove("/dev/shm/init_shm.lock");

        ilog(
            "[shm_diag] after destroy: "
            "is_exist=${is_exist}, "
            "MDBUS_SHM exists=${shm_exists}",
            ("is_exist", ::shm::shared_memory::is_exist())("shm_exists", mc::filesystem::exists("/dev/shm/MDBUS_SHM")));
    }
#endif
};

class MC_API TestWithApplication : public TestWithDbusDaemon {
protected:
    static void SetUpTestSuite()
    {
        TestWithDbusDaemon::SetUpTestSuite();

        mc::app::application().start();
    }

    static void TearDownTestSuite()
    {
        mc::app::application().stop();
        mc::app::application::reset_for_test();
        TestWithDbusDaemon::TearDownTestSuite();
    };
};

} // namespace test
} // namespace mc

#endif // MC_LIBMCPP_TEST_BASE_H
