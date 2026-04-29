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

#ifndef MC_BASE_SECUREC_H
#define MC_BASE_SECUREC_H

#include <stdarg.h>
#include <stddef.h>

#ifndef SECUREC_API
#if defined(__GNUC__) || defined(__clang__)
#define SECUREC_API __attribute__((visibility("default")))
#else
#define SECUREC_API
#endif
#endif

#ifndef EOK
#define EOK 0
#endif

#ifndef errno_t
typedef int errno_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

SECUREC_API errno_t memset_s(void* dest, size_t destMax, int c, size_t count);
SECUREC_API errno_t memmove_s(void* dest, size_t destMax, const void* src, size_t count);
SECUREC_API errno_t memcpy_s(void* dest, size_t destMax, const void* src, size_t count);
SECUREC_API errno_t strcat_s(char* strDest, size_t destMax, const char* strSrc);

#if defined(__GNUC__) || defined(__clang__)
SECUREC_API int sprintf_s(char* strDest, size_t destMax, const char* format, ...)
    __attribute__((format(printf, 3, 4)));
SECUREC_API int vsnprintf_s(char* strDest, size_t destMax, size_t count, const char* format, va_list argList)
    __attribute__((format(printf, 4, 0)));
SECUREC_API int snprintf_s(char* strDest, size_t destMax, size_t count, const char* format, ...)
    __attribute__((format(printf, 4, 5)));
#else
SECUREC_API int sprintf_s(char* strDest, size_t destMax, const char* format, ...);
SECUREC_API int vsnprintf_s(char* strDest, size_t destMax, size_t count, const char* format, va_list argList);
SECUREC_API int snprintf_s(char* strDest, size_t destMax, size_t count, const char* format, ...);
#endif

SECUREC_API errno_t strncpy_s(char* strDest, size_t destMax, const char* strSrc, size_t count);

#ifdef __cplusplus
}
#endif

#endif
