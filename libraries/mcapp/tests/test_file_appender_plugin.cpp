/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <cstdlib>
#include <fstream>
#include <mc/log/appender.h>

namespace mc::app::tests {

class test_file_appender_plugin : public mc::log::appender {
public:
    bool init(const mc::variant& args) override
    {
        if (!args.is_object()) {
            return false;
        }

        auto dict = args.as<mc::dict>();
        if (dict.contains("filename") && dict["filename"].is_string()) {
            m_filename = dict["filename"].as<std::string>();
        }

        if (m_filename.empty()) {
            const char* env = std::getenv("MCAPP_TEST_DEFAULT_FILE_LOG_PATH");
            if (env != nullptr && env[0] != '\0') {
                m_filename = env;
            }
        }

        return !m_filename.empty();
    }

    void append(const mc::log::message& msg) override
    {
        std::ofstream file(m_filename, std::ios::app);
        if (!file.is_open()) {
            return;
        }

        file << msg.get_message() << "\n";
    }

private:
    std::string m_filename;
};

} // namespace mc::app::tests

MC_REGISTER_APPENDER(mc::app::tests::test_file_appender_plugin)
