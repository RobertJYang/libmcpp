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

#ifndef MC_DBUS_APP_BUS_H
#define MC_DBUS_APP_BUS_H

#include <dbus/dbus.h>
#include <glib-2.0/glib.h>
#include <mc/dict.h>
#include <mc/variant.h>

namespace mc::dbus {
constexpr int32_t MAX_SIG_LEN         = 100;
constexpr int32_t MAX_CONTAINER_DEPTH = 32;
constexpr int32_t MAX_CONTAINER_SIZE  = 1024;
constexpr int32_t MAX_SIGNATURE_LEN   = 255;

/**
 * @brief GVariant签名单元
 */
struct MC_API sig_unit {
public:
    int         type;                 /**< 类型码 */
    int         len;                  /**< 签名长度 */
    char        buf[MAX_SIG_LEN + 1]; /**< 签名缓冲区 */
    const char* sub_types;            /**< 子类型指针 */
    const char* next_types;           /**< 下一个类型指针 */

    /**
     * @brief 检查子类型是否有效
     * @return 有效返回true，否则返回false
     */
    bool sub_types_is_valid() const {
        return sub_types != nullptr && *sub_types != '\0';
    }

    /**
     * @brief 获取签名长度
     * @param types [in] 类型字符串
     * @param allow_dict_entry [in] 是否允许dict entry
     * @param array_depth [in] 数组深度
     * @param struct_depth [in] 结构体深度
     * @return 返回签名长度
     */
    static int get_sig_len(const char* types, bool allow_dict_entry, size_t array_depth,
                           size_t struct_depth);

private:
    static int get_dict_len(const char* types, bool allow_dict_entry, size_t array_depth,
                            size_t struct_depth);
    static int get_struct_len(const char* types, bool allow_dict_entry, size_t array_depth,
                              size_t struct_depth);
};

/**
 * @brief GVariant自动释放包装器
 */
struct MC_API gvariant_auto_free {
    /**
     * @brief 默认构造函数
     */
    gvariant_auto_free() = default;

    /**
     * @brief 从GVariant指针构造
     * @param v [in] GVariant指针
     * @param add_ref [in] 是否增加引用计数
     */
    explicit gvariant_auto_free(GVariant* v, bool add_ref = false);

    /**
     * @brief 析构函数
     */
    ~gvariant_auto_free();

    /**
     * @brief 拷贝构造函数
     * @param other [in] 源对象
     */
    gvariant_auto_free(const gvariant_auto_free& other);

    /**
     * @brief 拷贝赋值运算符
     * @param other [in] 源对象
     * @return 返回当前对象的引用
     */
    gvariant_auto_free& operator=(const gvariant_auto_free& other);

    /**
     * @brief 移动构造函数
     * @param other [in/out] 源对象
     */
    gvariant_auto_free(gvariant_auto_free&& other) noexcept;

    /**
     * @brief 移动赋值运算符
     * @param other [in/out] 源对象
     * @return 返回当前对象的引用
     */
    gvariant_auto_free& operator=(gvariant_auto_free&& other) noexcept;

    /**
     * @brief 释放GVariant指针
     */
    void release();

    GVariant* ptr{nullptr}; /**< GVariant指针 */
};

/**
 * @brief GVariant构建器
 */
class MC_API gvariant_builder : public GVariantBuilder {
public:
    /**
     * @brief 构造函数
     * @param type [in] GVariant类型
     */
    explicit gvariant_builder(const GVariantType* type);

    /**
     * @brief 析构函数
     */
    ~gvariant_builder();

    /**
     * @brief 添加值
     * @param value [in] GVariant值
     */
    void add(GVariant* value);

    /**
     * @brief 结束构建并返回结果
     * @return 返回构建的GVariant对象
     */
    GVariant* end();
};

/**
 * @brief GVariant与mc::variant转换器
 */
class MC_API gvariant_convert {
public:
    /**
     * @brief 将GVariant转换为mc::variant
     * @param value [in] GVariant值
     * @return 返回mc::variant对象
     */
    static variant to_mc_variant(GVariant* value);

    /**
     * @brief 将mc::variant按指定类型转换为GVariant
     * @param value [in] mc::variant对象
     * @param types [in] 类型签名
     * @return 返回GVariant对象
     */
    static GVariant* to_gvariant(const variant& value, const char* types);

    /**
     * @brief 将mc::variant转换为GVariant（自动推断类型）
     * @param value [in] mc::variant对象
     * @return 返回GVariant对象
     */
    static GVariant* to_gvariant(const variant& value);

private:
    static std::tuple<GVariant*, const char*> to_gvariant_inner(const variant& v,
                                                                const char*    types);
    static dict                               dict_to_mc_variant(GVariant* value, int n);
    static variants                           array_to_mc_variant(GVariant* value, int n);
    static variant                            container_to_mc_variant(GVariant* value);
    static GVariant*                          new_gvariant_dict(const variant& v, sig_unit& sig);
    static GVariant*                          new_gvariant_struct(const variant& v, sig_unit& sig);
    static GVariant*                          new_gvariant_array(const variant& v, sig_unit& sig);
};

} // namespace mc::dbus

#endif // MC_DBUS_APP_BUS_H