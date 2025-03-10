//-- Copyright (c) 2024 Huawei Technologies Co., Ltd.
//-- openUBMC is licensed under Mulan PSL v2.
//-- You can use this software according to the terms and conditions of the Mulan PSL v2.
//-- You may obtain a copy of Mulan PSL v2 at:
//--         http://license.coscl.org.cn/MulanPSL2
//-- THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
//-- EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
//-- MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
//-- See the Mulan PSL v2 for more details.

#ifndef MCPP_FUTURE_STATUS_HPP
#define MCPP_FUTURE_STATUS_HPP

namespace mc {
namespace future {

enum class future_status { ready, timeout, deferred };

enum class launch {
    dispatch = 1,  // 立即执行
    async = 2,     // 异步执行
    deferred = 4,  // 延迟执行
    any = async | deferred
};

}  // namespace future
}  // namespace mc

#endif  // MCPP_FUTURE_STATUS_HPP