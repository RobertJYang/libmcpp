--[[
Copyright (c) 2026 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
]]

-- dft 模块测试入口文件
-- 先加载 C 模块，避免循环依赖

-- 临时移除当前模块路径，确保能加载 C 模块而不是递归加载自己
local old_path = package.path
package.path = ""

-- 加载 C 模块 ldft.so
local dft_module = require('ldft')

-- 恢复路径
package.path = old_path

-- 将 C 模块设为全局变量供测试使用
_G.dft = dft_module

-- 导入该目录下所有测试文件
require('dft/test_variant_convert')
