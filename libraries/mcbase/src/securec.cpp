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

#include <securec.h>

#include <stdio.h>
#include <string.h>

namespace {

size_t securec_strnlen_impl(const char* str, size_t max_len)
{
    size_t len = 0;
    if (str == NULL) {
        return 0;
    }

    while (len < max_len && str[len] != '\0') {
        ++len;
    }
    return len;
}

} // namespace

extern "C" {

errno_t memset_s(void* dest, size_t destMax, int c, size_t count)
{
    if (dest == NULL || destMax == 0 || count > destMax) {
        return -1;
    }

    (void)memset(dest, c, count);
    return EOK;
}

errno_t memmove_s(void* dest, size_t destMax, const void* src, size_t count)
{
    if (dest == NULL || src == NULL || destMax == 0 || count > destMax) {
        return -1;
    }

    (void)memmove(dest, src, count);
    return EOK;
}

errno_t memcpy_s(void* dest, size_t destMax, const void* src, size_t count)
{
    if (dest == NULL || src == NULL || destMax == 0 || count > destMax) {
        return -1;
    }

    (void)memcpy(dest, src, count);
    return EOK;
}

errno_t strcat_s(char* strDest, size_t destMax, const char* strSrc)
{
    size_t dest_len;
    size_t src_len;

    if (strDest == NULL || strSrc == NULL || destMax == 0) {
        return -1;
    }

    dest_len = securec_strnlen_impl(strDest, destMax);
    src_len  = strlen(strSrc);
    if (dest_len >= destMax || src_len > (destMax - dest_len - 1)) {
        return -1;
    }

    (void)memcpy(strDest + dest_len, strSrc, src_len + 1);
    return EOK;
}

int sprintf_s(char* strDest, size_t destMax, const char* format, ...)
{
    int     ret;
    va_list args;

    if (strDest == NULL || format == NULL || destMax == 0) {
        return -1;
    }

    va_start(args, format);
    ret = vsnprintf(strDest, destMax, format, args);
    va_end(args);
    if (ret < 0 || (size_t)ret >= destMax) {
        strDest[destMax - 1] = '\0';
        return -1;
    }
    return ret;
}

int vsnprintf_s(char* strDest, size_t destMax, size_t count, const char* format, va_list argList)
{
    int    ret;
    size_t limit;

    if (strDest == NULL || format == NULL || destMax == 0) {
        return -1;
    }

    limit = destMax - 1;
    if (count < limit) {
        limit = count;
    }

    ret = vsnprintf(strDest, limit + 1, format, argList);
    if (ret < 0 || (size_t)ret > limit) {
        strDest[limit] = '\0';
        return -1;
    }
    return ret;
}

int snprintf_s(char* strDest, size_t destMax, size_t count, const char* format, ...)
{
    int     ret;
    va_list args;

    if (strDest == NULL || format == NULL || destMax == 0) {
        return -1;
    }

    va_start(args, format);
    ret = vsnprintf_s(strDest, destMax, count, format, args);
    va_end(args);
    return ret;
}

errno_t strncpy_s(char* strDest, size_t destMax, const char* strSrc, size_t count)
{
    size_t src_len;
    size_t copy_len;

    if (strDest == NULL || strSrc == NULL || destMax == 0) {
        return -1;
    }

    src_len  = strlen(strSrc);
    copy_len = src_len < count ? src_len : count;
    if (copy_len >= destMax) {
        return -1;
    }

    (void)memcpy(strDest, strSrc, copy_len);
    strDest[copy_len] = '\0';
    return EOK;
}

} // extern "C"
