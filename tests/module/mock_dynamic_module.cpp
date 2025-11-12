/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
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
#include <stdexcept>
#include <mc/module.h>
#include <mc/reflect/reflect.h>

MC_MODULE(mc_test_dynamic)

namespace mc::test_dynamic_module {

class dynamic_object {
public:
    MC_REFLECTABLE("mc.test.dynamic.DynamicObject")

    int sum(int lhs, int rhs) const {
        return lhs + rhs;
    }
};

} // namespace mc::test_dynamic_module

MC_MODULE_REFLECT(mc_test_dynamic, mc::test_dynamic_module::dynamic_object, ((sum, "sum")))

extern "C" MC_API void* mc_open_mc_test_dynamic() {
    const char* flag = std::getenv("MC_TEST_DYNAMIC_RETURN_NULL");
    if (flag != nullptr && flag[0] != '\0') {
        return nullptr;
    }
    static mc::reflect::factory_ptr factory_holder =
        mc::reflect::reflection_factory::instance_ptr<mc_test_dynamic_namespace>();
    return factory_holder.get();
}

extern "C" MC_API void mc_close_mc_test_dynamic() {
    const char* flag = std::getenv("MC_TEST_DYNAMIC_CLOSE_THROW");
    if (flag != nullptr && flag[0] != '\0') {
        throw std::runtime_error("close dynamic module");
    }
}
