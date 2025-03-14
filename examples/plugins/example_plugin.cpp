/**
 * @file example_plugin.cpp
 * @brief 示例插件实现
 */

#include "example_plugin.h"
#include <iostream>

namespace po = mc::po;

/**
 * @brief 创建插件实例的导出函数
 * @return 插件实例指针
 */
extern "C" mc::plugin* create_plugin() {
    return new examples::example_plugin();
}