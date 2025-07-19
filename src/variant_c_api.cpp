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

/**
 * @file variant_c_api.cpp
 * @brief 实现 variant C API 接口
 */
#include <mc/variant.h>
#include <mc/variant_c_api.h>

#include <mc/expr/context.h>
#include <mc/expr/engine.h>

#include <string>

extern "C" {

mc_variant_t* mc_variant_from_int64(int64_t value) {
    try {
        auto* variant = new mc::variant(value);
        return reinterpret_cast<mc_variant_t*>(variant);
    } catch (...) {
        return nullptr;
    }
}

mc_variant_t* mc_variant_from_double(double value) {
    try {
        auto* variant = new mc::variant(value);
        return reinterpret_cast<mc_variant_t*>(variant);
    } catch (...) {
        return nullptr;
    }
}

mc_variant_t* mc_variant_from_bool(bool value) {
    try {
        auto* variant = new mc::variant(value);
        return reinterpret_cast<mc_variant_t*>(variant);
    } catch (...) {
        return nullptr;
    }
}

mc_variant_t* mc_variant_from_string(const char* value) {
    try {
        if (!value) {
            return nullptr;
        }
        auto* variant = new mc::variant(std::string(value));
        return reinterpret_cast<mc_variant_t*>(variant);
    } catch (...) {
        return nullptr;
    }
}

void mc_variant_delete(mc_variant_t* variant) {
    if (variant) {
        auto* v = reinterpret_cast<mc::variant*>(variant);
        delete v;
    }
}

mc_engine_t* mc_engine_new(void) {
    try {
        auto* engine = new mc::expr::engine();
        return reinterpret_cast<mc_engine_t*>(engine);
    } catch (...) {
        return nullptr;
    }
}

void mc_engine_delete(mc_engine_t* engine) {
    if (engine) {
        auto* e = reinterpret_cast<mc::expr::engine*>(engine);
        delete e;
    }
}

mc_context_t* mc_context_new(mc_engine_t* engine) {
    try {
        if (!engine) {
            return nullptr;
        }
        auto* e           = reinterpret_cast<mc::expr::engine*>(engine);
        auto  context     = e->make_context();
        auto* context_ptr = new mc::expr::context(std::move(context));
        return reinterpret_cast<mc_context_t*>(context_ptr);
    } catch (...) {
        return nullptr;
    }
}

void mc_context_delete(mc_context_t* context) {
    if (context) {
        auto* c = reinterpret_cast<mc::expr::context*>(context);
        delete c;
    }
}

int mc_context_register_variable(mc_context_t* context, const char* name, const mc_variant_t* variant) {
    try {
        if (!context || !name || !variant) {
            return -1;
        }
        auto* c = reinterpret_cast<mc::expr::context*>(context);
        auto* v = reinterpret_cast<const mc::variant*>(variant);
        c->register_variable(std::string(name), *v);
        return 0;
    } catch (...) {
        return -1;
    }
}

void* mc_engine_evaluate_as_gvariant(mc_engine_t* engine, const char* expr, const mc_context_t* context) {
    try {
        if (!engine || !expr || !context) {
            return nullptr;
        }
        auto* e = reinterpret_cast<mc::expr::engine*>(engine);
        auto* c = reinterpret_cast<const mc::expr::context*>(context);
        return e->evaluate_as_gvariant(std::string_view(expr), *c);
    } catch (...) {
        return nullptr;
    }
}

} // extern "C"