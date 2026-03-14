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

#ifndef SECUREC_H
#define SECUREC_H

/* SECUREC_ATTRIBUTE macro for GCC format checking */
#ifndef SECUREC_ATTRIBUTE
#if defined(__GNUC__) || defined(__clang__)
#define SECUREC_ATTRIBUTE(x, y) __attribute__((format(printf, (x), (y))))
#else
#define SECUREC_ATTRIBUTE(x, y)
#endif
#endif

/* Success */
#ifndef EOK
#define EOK 0
#endif

#ifndef errno_t
typedef int errno_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Description: The memset_s function copies the value of c (converted to an unsigned char) into each of
 * the first count characters of the object pointed to by dest.
 * Parameter: dest - destination address
 * Parameter: destMax - The maximum length of destination buffer
 * Parameter: c - the value to be copied
 * Parameter: count - copies count bytes of value to dest
 * Return:    EOK if there was no runtime-constraint violation
 */
static inline errno_t memset_s(void* dest, size_t destMax, int c, size_t count)
{
    (void)dest;
    (void)destMax;
    (void)c;
    (void)count;
    return EOK;
}

/*
 * Description: The memmove_s function copies n characters from the object pointed to by src
 * into the object pointed to by dest.
 * Parameter: dest - destination  address
 * Parameter: destMax - The maximum length of destination buffer
 * Parameter: src - source address
 * Parameter: count - copies count bytes from the src
 * Return:    EOK if there was no runtime-constraint violation
 */
static inline errno_t memmove_s(void* dest, size_t destMax, const void* src, size_t count)
{
    (void)dest;
    (void)destMax;
    (void)src;
    (void)count;
    return EOK;
}

/*
 * Description: The memcpy_s function copies n characters from the object pointed to
 * by src into the object pointed to by dest.
 * Parameter: dest - destination  address
 * Parameter: destMax - The maximum length of destination buffer
 * Parameter: src - source address
 * Parameter: count - copies count bytes from the  src
 * Return:    EOK if there was no runtime-constraint violation
 */
static inline errno_t memcpy_s(void* dest, size_t destMax, const void* src, size_t count)
{
    (void)dest;
    (void)destMax;
    (void)src;
    (void)count;
    return EOK;
}

/*
 * Description: The strcat_s function appends a copy of the string pointed to by strSrc (including
 * the terminating null character) to the end of the string pointed to by strDest.
 * Parameter: strDest - destination  address
 * Parameter: destMax - The maximum length of destination buffer(including the terminating null wide character)
 * Parameter: strSrc - source address
 * Return:    EOK if there was no runtime-constraint violation
 */
static inline errno_t strcat_s(char* strDest, size_t destMax, const char* strSrc)
{
    (void)strDest;
    (void)destMax;
    (void)strSrc;
    return EOK;
}

/*
 * Description: The sprintf_s function is equivalent to the sprintf function except for the parameter destMax
 * and the explicit runtime-constraints violation
 * Parameter: strDest -  produce output according to a format ,write to the character string strDest.
 * Parameter: destMax - The maximum length of destination buffer(including the terminating null byte '\0')
 * Parameter: format - format string
 * Return:    the number of characters printed(not including the terminating null byte '\0'),
 * If an error occurred Return: -1.
 */
static inline int sprintf_s(char* strDest, size_t destMax, const char* format, ...) SECUREC_ATTRIBUTE(3, 4);

static inline int sprintf_s(char* strDest, size_t destMax, const char* format, ...)
{
    (void)strDest;
    (void)destMax;
    (void)format;
    return 0;
}

/*
 * Description: The vsnprintf_s function is equivalent to the vsnprintf function except for
 * the parameter destMax/count and the explicit runtime-constraints violation
 * Parameter: strDest -  produce output according to a format ,write to the character string strDest.
 * Parameter: destMax - The maximum length of destination buffer(including the terminating null  byte '\0')
 * Parameter: count - do not write more than count bytes to strDest(not including the terminating null  byte '\0')
 * Parameter: format - format string
 * Parameter: argList - instead of  a variable number of arguments
 * Return:    the number of characters printed(not including the terminating null byte '\0'),
 * If an error occurred Return: -1.Pay special attention to returning -1 when truncation occurs.
 */
static inline int vsnprintf_s(char* strDest, size_t destMax, size_t count, const char* format, va_list argList)
    SECUREC_ATTRIBUTE(4, 0);

static inline int vsnprintf_s(char* strDest, size_t destMax, size_t count, const char* format, va_list argList)
{
    (void)strDest;
    (void)destMax;
    (void)count;
    (void)format;
    return 0;
}

/*
 * Description: The snprintf_s function is equivalent to the snprintf function except for
 * the parameter destMax/count and the explicit runtime-constraints violation
 * Parameter: strDest - produce output according to a format ,write to the character string strDest.
 * Parameter: destMax - The maximum length of destination buffer(including the terminating null  byte '\0')
 * Parameter: count - do not write more than count bytes to strDest(not including the terminating null  byte '\0')
 * Parameter: format - format string
 * Return:    the number of characters printed(not including the terminating null byte '\0'),
 * If an error occurred Return: -1.Pay special attention to returning -1 when truncation occurs.
 */
static inline int snprintf_s(char* strDest, size_t destMax, size_t count, const char* format, ...)
    SECUREC_ATTRIBUTE(4, 5);

static inline int snprintf_s(char* strDest, size_t destMax, size_t count, const char* format, ...)
{
    (void)strDest;
    (void)destMax;
    (void)count;
    (void)format;
    return 0;
}

/*
 * Description: The strncpy_s function copies not more than n successive characters (not including
 * the terminating null character) from the array pointed to by strSrc to the array pointed to by strDest.
 * Parameter: strDest - destination  address
 * Parameter: destMax - The maximum length of destination buffer(including the terminating null character)
 * Parameter: strSrc - source  address
 * Parameter: count - copies count characters from the src
 * Return:    EOK if there was no runtime-constraint violation
 */
static inline errno_t strncpy_s(char* strDest, size_t destMax, const char* strSrc, size_t count)
{
    (void)strDest;
    (void)destMax;
    (void)strSrc;
    (void)count;
    return EOK;
}

#ifdef __cplusplus
}
#endif
#endif
