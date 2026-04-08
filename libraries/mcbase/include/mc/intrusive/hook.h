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

/**
 * @file hook.h
 * @brief 封装 Boost 的侵入式钩子类型
 */
#ifndef MC_INTRUSIVE_HOOK_H
#define MC_INTRUSIVE_HOOK_H

#include <boost/intrusive/link_mode.hpp>
#include <boost/intrusive/list_hook.hpp>
#include <boost/intrusive/set_hook.hpp>
#include <boost/intrusive/slist_hook.hpp>
#include <boost/intrusive/unordered_set_hook.hpp>

namespace mc::intrusive {

using boost::intrusive::auto_unlink;
using boost::intrusive::constant_time_size;
using boost::intrusive::link_mode;
using boost::intrusive::safe_link;

using boost::intrusive::list_base_hook;
using boost::intrusive::list_member_hook;

using boost::intrusive::slist_base_hook;
using boost::intrusive::slist_member_hook;

using boost::intrusive::set_base_hook;
using boost::intrusive::set_member_hook;

using boost::intrusive::unordered_set_base_hook;
using boost::intrusive::unordered_set_member_hook;

} // namespace mc::intrusive

#endif // MC_INTRUSIVE_HOOK_H