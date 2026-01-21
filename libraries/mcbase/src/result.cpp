/**
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

#include <mc/error_engine.h>
#include <mc/exception.h>
#include <mc/result.h>

namespace mc {

namespace detail {
void throw_method_call_exception(const mc::error_ptr& err) {
    mc::method_call_exception ex;
    if (err) {
        err->to_exception(ex);
    } else {
        get_default_error()->to_exception(ex);
    }
    throw ex;
}

mc::error_ptr get_default_error() {
    auto& error_engine = mc::error_engine::get_instance();
    auto  last_error   = error_engine.last_error();
    if (last_error && last_error->is_set()) {
        return last_error;
    }

    return mc::make_error("org.freedesktop.DBus.Error.Failed",
                          "Failed to execute method");
}

mc::method_call_exception make_method_call_exception(const mc::error_ptr& err) {
    mc::method_call_exception ex;
    if (err) {
        err->to_exception(ex);
    } else {
        get_default_error()->to_exception(ex);
    }
    return ex;
}

} // namespace detail

} // namespace mc